#!/usr/bin/env python3
"""Exercise real central TLS rooms, legal commands, retries and reconnects."""
import argparse
import asyncio
import contextlib
import json
import math
import os
from pathlib import Path
import platform
import resource
import socket
import ssl
import subprocess
import tempfile
import time


def free_port():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


class Peer:
    def __init__(self, context, port, hello):
        self.context, self.port, self.hello = context, port, hello
        self.room, self.snapshot = {}, None
        self.token, self.instance, self.code = "", "", ""
        self.sequence = 1
        self.replies = asyncio.Queue()
        self.reader_task = self.heartbeat_task = None
        self.writer = None

    async def send(self, message):
        payload = json.dumps(message, separators=(",", ":")).encode()
        self.writer.write(len(payload).to_bytes(4, "big") + payload)
        await self.writer.drain()

    async def read(self, reader):
        try:
            while True:
                size = int.from_bytes(await reader.readexactly(4), "big")
                if not 0 < size <= 1024 * 1024:
                    raise RuntimeError("invalid central frame length")
                message = json.loads(await reader.readexactly(size))
                if self.code and message.get("session", self.code) != self.code:
                    raise RuntimeError("cross-room delivery")
                kind = message["type"]
                if kind == "Room":
                    self.room = message
                    self.sequence = max(self.sequence, int(message["nextSequence"]))
                elif kind == "StateSnapshot":
                    view = message["view"]
                    for row in view["cards"]:
                        card = row["instance"]
                        if int(card["owner"]) != int(view["viewer"]) and int(card["zone"]) in (0, 1):
                            raise RuntimeError("opponent hand or deck leaked")
                    self.snapshot = message
                elif kind in ("Welcome", "RoomJoined", "Accepted", "Rejected", "Error", "ActionOptions", "Left"):
                    await self.replies.put(message)
        except asyncio.CancelledError:
            raise
        except Exception as error:
            await self.replies.put(error)

    async def heartbeat(self):
        while True:
            await asyncio.sleep(2)
            await self.send({"type": "Heartbeat"})

    async def call(self, message, expected):
        await self.send(message)
        reply = await asyncio.wait_for(self.replies.get(), timeout=5)
        if isinstance(reply, Exception):
            raise reply
        if reply["type"] not in expected:
            raise RuntimeError(f"unexpected reply for {message['type']}: {reply}")
        return reply

    async def connect(self, resume=False):
        reader, self.writer = await asyncio.open_connection(
            "127.0.0.1", self.port, ssl=self.context, server_hostname="localhost")
        self.replies = asyncio.Queue()
        self.reader_task = asyncio.create_task(self.read(reader))
        welcome = await self.call(self.hello, {"Welcome"})
        if resume:
            joined = await self.call({"type": "Resume", "token": self.token,
                                      "instanceId": self.instance, "code": self.code}, {"RoomJoined"})
            if joined["room"]["code"] != self.code or joined["room"]["seat"] != 1:
                raise RuntimeError("resume changed room or seat")
            self.room = joined["room"]
        else:
            self.token, self.instance = welcome["token"], welcome["instanceId"]
        self.heartbeat_task = asyncio.create_task(self.heartbeat())

    async def close(self):
        for task in (self.reader_task, self.heartbeat_task):
            if task:
                task.cancel()
                with contextlib.suppress(asyncio.CancelledError, Exception):
                    await task
        if self.writer:
            self.writer.close()
            with contextlib.suppress(Exception):
                await asyncio.wait_for(self.writer.wait_closed(), timeout=1)


async def wait_until(predicate):
    async with asyncio.timeout(5):
        while not predicate():
            await asyncio.sleep(0.01)


async def legal_command(peer):
    state = peer.snapshot
    view = state["view"]
    if view["actionSummary"]["safeAdvance"]:
        return view["actionSummary"]["safeAdvance"]
    selections = []
    for index in range(32):
        response = await peer.call({
            "type": "ActionQuery", "request": f"query-{index}", "generation": "1",
            "revision": state["revision"], "decision": str((view.get("decision") or {}).get("id", "0")),
            "intent": {"source": "0", "offset": 0, "limit": 64, "selections": selections},
        }, {"ActionOptions"})
        options = response["options"]
        if options["error"]:
            raise RuntimeError("legal action query failed: " + options["error"])
        if options["complete"]:
            if options["complete"]["type"] == 11:
                raise RuntimeError("capacity workload must not surrender")
            return options["complete"]
        choices = options["options"]
        if options["field"] == 0:
            choices = [row for row in choices if row["value"] not in ("11", "19")]
            choices.sort(key=lambda row: row["value"] not in ("9", "13", "14", "18", "10"))
        if not choices:
            raise RuntimeError("actionable view has no legal continuation")
        selections.append({"field": options["field"], "value": choices[0]["value"]})
    raise RuntimeError("action query exceeded 32 selections")


async def exercise_room(index, context, port, hello, deck, deadline, report):
    peers = []
    cycle = 0
    reconnected = False
    try:
        while time.monotonic() < deadline:
            if not peers or peers[0].room.get("phase") == "completed":
                if peers:
                    for peer in peers:
                        await peer.call({"type": "Leave"}, {"Left"})
                        await peer.close()
                    await asyncio.sleep(0.4)
                peers = [Peer(context, port, hello), Peer(context, port, hello)]
                owner, guest = peers
                await owner.connect()
                created = await owner.call({"type": "CreateRoom", "deck": deck}, {"RoomJoined"})
                owner.code = created["room"]["code"]
                owner.room = created["room"]
                await guest.connect()
                joined = await guest.call({"type": "JoinRoom", "code": owner.code, "deck": deck}, {"RoomJoined"})
                guest.code = joined["room"]["code"]
                guest.room = joined["room"]
                await owner.send({"type": "Configure", "ownerFirst": True, "automatic": False})
                await owner.send({"type": "Ready"})
                await guest.send({"type": "Ready"})
                await wait_until(lambda: owner.snapshot and guest.snapshot)
                report["matchesStarted"] += 1
            owner, guest = peers
            if index % 4 == 0 and cycle >= 10 and not reconnected:
                previous = guest.code
                await guest.close()
                await asyncio.sleep(0.3)
                await guest.connect(resume=True)
                await wait_until(lambda: not owner.room.get("reconnecting") and not guest.room.get("reconnecting"))
                if guest.code != previous:
                    raise RuntimeError("reconnect allocated a duplicate room")
                report["reconnects"] += 1
                reconnected = True
            started = time.monotonic()
            if owner.room.get("phase") == "aborted":
                raise RuntimeError("unexpected room abort: " + owner.room.get("abortReason", ""))
            if owner.room.get("storageBlocked") or owner.room.get("technicalPaused"):
                raise RuntimeError("unexpected storage or technical pause")
            actionable = [peer for peer in peers if peer.snapshot and peer.snapshot["view"]["actionSummary"]["actionable"]]
            if not actionable:
                await asyncio.sleep(0.01)
                continue
            peer = actionable[cycle % len(actionable)]
            command = await legal_command(peer)
            state = peer.snapshot
            request = {"type": "Command", "sequence": str(peer.sequence), "request": f"load-{cycle}",
                       "revision": state["revision"], "decision": str((state["view"].get("decision") or {}).get("id", "0")),
                       "command": command}
            before = time.monotonic()
            accepted = await peer.call(request, {"Accepted", "Rejected"})
            if accepted["type"] != "Accepted":
                raise RuntimeError("valid capacity command rejected: " + str(accepted))
            peer.sequence = max(peer.sequence, int(request["sequence"]) + 1)
            report["latenciesMs"].append((time.monotonic() - before) * 1000)
            report["acceptedCommands"] += 1
            if cycle % 25 == 0:
                retry = await peer.call(request, {"Accepted"})
                if retry != accepted:
                    raise RuntimeError("retry was executed twice or changed its original receipt")
                report["duplicateRetries"] += 1
            revision = int(accepted["revision"])
            await wait_until(lambda: all(int(p.snapshot["revision"]) >= revision for p in peers))
            cycle += 1
            await asyncio.sleep(max(0, 0.5 - (time.monotonic() - started)))
    finally:
        for peer in peers:
            await peer.close()


async def workload(options, process, port, cert, hello, deck):
    context = ssl.create_default_context(cafile=str(cert))
    report = {"rooms": options.rooms, "clients": options.rooms * 2, "durationSeconds": options.duration,
              "acceptedCommands": 0, "duplicateRetries": 0, "reconnects": 0, "matchesStarted": 0,
              "latenciesMs": [], "peakRssKiB": 0, "environment": platform.platform(),
              "cpuAffinity": sorted(os.sched_getaffinity(process.pid)), "addressSpaceLimitBytes": 4 * 1024**3}
    started = time.monotonic()
    deadline = started + options.duration

    async def sample():
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError("server exited under load")
            for line in Path(f"/proc/{process.pid}/status").read_text().splitlines():
                if line.startswith("VmHWM:"):
                    report["peakRssKiB"] = max(report["peakRssKiB"], int(line.split()[1]))
            await asyncio.sleep(0.2)

    async def slow_reader():
        # An admitted TLS client that stops sending/reading must not block the 40 players.
        reader, writer = await asyncio.open_connection("127.0.0.1", port, ssl=context, server_hostname="localhost")
        payload = json.dumps(hello).encode()
        writer.write(len(payload).to_bytes(4, "big") + payload)
        await writer.drain()
        await asyncio.sleep(min(9, options.duration))
        try:
            while await asyncio.wait_for(reader.read(4096), timeout=2):
                pass
        finally:
            writer.close()
            with contextlib.suppress(Exception):
                await writer.wait_closed()
        report["slowPeerDisconnected"] = True

    async with asyncio.TaskGroup() as group:
        group.create_task(sample())
        group.create_task(slow_reader())
        for index in range(options.rooms):
            group.create_task(exercise_room(index, context, port, hello, deck, deadline, report))
    report["elapsedSeconds"] = round(time.monotonic() - started, 3)
    latencies = sorted(report.pop("latenciesMs"))
    report["p95ReceiptMs"] = round(latencies[math.ceil(len(latencies) * 0.95) - 1], 3) if latencies else None
    report["commandsPerRoomPerSecond"] = round(report["acceptedCommands"] / options.rooms / options.duration, 3)
    if report["p95ReceiptMs"] is None or report["p95ReceiptMs"] >= 250 or report["peakRssKiB"] >= 2 * 1024**2:
        raise RuntimeError("capacity latency/RSS target failed: " + str(report))
    if options.duration >= 30 and report["commandsPerRoomPerSecond"] < 1.8:
        raise RuntimeError("workload did not sustain about two valid commands per room per second")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--assets", type=Path, default=Path("assets/release-v260"))
    parser.add_argument("--rooms", type=int, default=2)
    parser.add_argument("--duration", type=float, default=15)
    parser.add_argument("--report", type=Path, default=Path("build/central-smoke.json"))
    options = parser.parse_args()
    if not 1 <= options.rooms <= 20 or options.duration < 1:
        parser.error("rooms must be 1..20 and duration at least one second")
    assets = options.assets.resolve()
    metadata = json.loads((assets / "catalog.json").read_text())["metadata"]
    hello = {"type": "Hello", "protocol": 5, "program": "2.6.2", "rules": metadata["rulesVersion"],
             "mechanics": metadata["mechanicsRevision"], "cards": metadata["cardSetVersion"], "hash": "cc7c6005e8f96820"}
    main_deck = json.loads((assets / "deck.json").read_text())
    side = json.loads((assets / "side-deck.json").read_text())
    deck = {"baseFormation": side["initialFormation"],
            "cards": [card["id"] for card in main_deck for _ in range(card["count"])],
            "sideDeck": [card["id"] for card in side["cards"] for _ in range(card["count"])]}
    with tempfile.TemporaryDirectory(prefix="wizard-central-load-") as temporary:
        root = Path(temporary)
        cert, key = root / "server.crt", root / "server.key"
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                        "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost",
                        "-keyout", str(key), "-out", str(cert)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        port, health_port = free_port(), free_port()
        def limits():
            os.sched_setaffinity(0, sorted(os.sched_getaffinity(0))[:2])
            resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3, 4 * 1024**3))
        with (root / "server.log").open("w+") as log:
            process = subprocess.Popen([str(options.exe.resolve()), "--assets", str(assets), "--data", str(root / "rooms"),
                                        "--cert", str(cert), "--key", str(key), "--bind", "127.0.0.1", "--port", str(port),
                                        "--health-port", str(health_port)], stdout=log, stderr=subprocess.STDOUT, preexec_fn=limits)
            try:
                for attempt in range(150):
                    if process.poll() is not None:
                        log.seek(0)
                        raise RuntimeError("server did not start: " + log.read())
                    try:
                        with socket.create_connection(("127.0.0.1", health_port), timeout=0.1):
                            break
                    except OSError:
                        time.sleep(0.1)
                else:
                    raise RuntimeError("server readiness timeout")
                report = asyncio.run(workload(options, process, port, cert, hello, deck))
                process.terminate()
                if process.wait(timeout=20) != 0:
                    raise RuntimeError("server shutdown failed")
                options.report.parent.mkdir(parents=True, exist_ok=True)
                options.report.write_text(json.dumps(report, indent=2) + "\n")
                print(json.dumps(report, indent=2))
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=20)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)


if __name__ == "__main__":
    main()
