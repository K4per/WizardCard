#!/usr/bin/env python3
import http.client
import json
import os
import pathlib
import socket
import ssl
import subprocess
import sys
import tempfile
import time


def free_port():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


def read_message(stream):
    header = bytearray()
    while len(header) < 4:
        part = stream.recv(4 - len(header))
        if not part:
            raise RuntimeError("server closed before replying")
        header.extend(part)
    size = int.from_bytes(header, "big")
    payload = bytearray()
    while len(payload) < size:
        part = stream.recv(size - len(payload))
        if not part:
            raise RuntimeError("server closed during reply")
        payload.extend(part)
    return json.loads(payload)


def send_message(stream, value):
    payload = json.dumps(value, separators=(",", ":")).encode()
    stream.sendall(len(payload).to_bytes(4, "big") + payload)


def expanded_deck(assets):
    main = json.loads((assets / "deck.json").read_text())
    side = json.loads((assets / "side-deck.json").read_text())
    return {
        "baseFormation": side["initialFormation"],
        "cards": [card["id"] for card in main for _ in range(card["count"])],
        "sideDeck": [card["id"] for card in side["cards"] for _ in range(card["count"])],
    }


def wait_ready(port, process):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError("server exited during startup: " + process.stdout.read())
        try:
            connection = http.client.HTTPConnection("127.0.0.1", port, timeout=0.5)
            connection.request("GET", "/ready")
            response = connection.getresponse()
            body = response.read()
            connection.close()
            if response.status == 200 and json.loads(body).get("ready") is True:
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError("server readiness endpoint did not become healthy")


def read_type(stream, expected):
    for _ in range(128):
        message = read_message(stream)
        if message["type"] == expected:
            return message
    raise RuntimeError("missing message: " + expected)


def main():
    binary = pathlib.Path(sys.argv[1]).resolve()
    assets = pathlib.Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix="wizard-central-tls-") as temporary:
        root = pathlib.Path(temporary)
        cert, key = root / "server.crt", root / "server.key"
        subprocess.run([
            "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
            "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost",
            "-keyout", str(key), "-out", str(cert),
        ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        game_port, health_port = free_port(), free_port()
        command = [
            str(binary), "--assets", str(assets), "--data", str(root / "data"),
            "--cert", str(cert), "--key", str(key), "--bind", "127.0.0.1",
            "--port", str(game_port), "--health-port", str(health_port),
        ]
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            wait_ready(health_port, process)

            catalog = json.loads((assets / "catalog.json").read_text())
            hello = {
                "type": "Hello", "protocol": 5,
                "rules": catalog["metadata"]["rulesVersion"],
                "mechanics": catalog["metadata"]["mechanicsRevision"],
                "cards": catalog["metadata"]["cardSetVersion"],
                "hash": "41eac6fc263abd5e",
            }
            context = ssl.create_default_context(cafile=str(cert))
            with socket.create_connection(("127.0.0.1", health_port), timeout=3) as oversized_health:
                oversized_health.sendall(b"A" * 8192)
                try:
                    bounded = oversized_health.recv(1) == b""
                except ConnectionResetError:
                    bounded = True
                assert bounded, "health request exceeded the header bound"
            with socket.create_connection(("127.0.0.1", game_port), timeout=3) as raw:
                with context.wrap_socket(raw, server_hostname="localhost") as stream:
                    send_message(stream, hello)
                    welcome = read_message(stream)
                    assert welcome["type"] == "Welcome", welcome
                    assert welcome["protocol"] == 5, welcome
                    assert len(welcome["token"]) >= 64, welcome
                    send_message(stream, {"type": "Heartbeat"})
                    assert read_message(stream)["type"] == "Heartbeat"

            rejected = 0
            for trust, hostname in ((ssl.create_default_context(), "localhost"), (context, "wrong.example")):
                try:
                    with socket.create_connection(("127.0.0.1", game_port), timeout=3) as raw:
                        with trust.wrap_socket(raw, server_hostname=hostname):
                            pass
                except (ssl.SSLError, ConnectionError, OSError):
                    rejected += 1
            assert rejected == 2, "TLS accepted an untrusted CA or a mismatched hostname"

            welcomes = []
            def connected_client():
                raw = socket.create_connection(("127.0.0.1", game_port), timeout=3)
                stream = context.wrap_socket(raw, server_hostname="localhost")
                send_message(stream, hello)
                welcome = read_message(stream)
                assert welcome["type"] == "Welcome"
                welcomes.append(welcome)
                return stream

            with connected_client() as noisy:
                replies = 0
                try:
                    for _ in range(200):
                        send_message(noisy, {"type": "Heartbeat"})
                    while replies < 200:
                        read_message(noisy)
                        replies += 1
                except (OSError, RuntimeError):
                    pass
                assert replies < 100, "Heartbeat bypassed the request rate limit"

            owner = connected_client()
            guest = connected_client()
            deck = expanded_deck(assets)
            send_message(owner, {"type": "CreateRoom", "deck": deck})
            created = read_message(owner)
            assert created["type"] == "RoomJoined", created
            code = created["room"]["code"]
            assert len(code) == 8
            send_message(guest, {"type": "JoinRoom", "code": code, "deck": deck})
            joined = read_message(guest)
            assert joined["type"] == "RoomJoined", joined
            assert joined["room"]["code"] == code

            process.terminate()
            returncode = process.wait(timeout=15)
            output = process.stdout.read()
            assert returncode == 0, f"server exited {returncode}: {output}"
            process = None
            metadata = json.loads(next((root / "data").glob(code + "-*/metadata.json")).read_text())
            assert metadata["phase"] == "aborted", metadata
            assert metadata["abortReason"] == "server_shutdown", metadata
            owner.close()
            guest.close()

            # An actual SIGKILL during play must retain history, abort the match
            # on startup, and invalidate the previous process's credentials.
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            wait_ready(health_port, process)
            owner, guest = connected_client(), connected_client()
            old_welcome = welcomes[-2]
            send_message(owner, {"type": "CreateRoom", "deck": deck})
            crash_code = read_type(owner, "RoomJoined")["room"]["code"]
            send_message(guest, {"type": "JoinRoom", "code": crash_code, "deck": deck})
            read_type(guest, "RoomJoined")
            send_message(owner, {"type": "Ready"})
            send_message(guest, {"type": "Ready"})
            read_type(owner, "StateSnapshot")
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                paths = list((root / "data").glob(crash_code + "-*/metadata.json"))
                if paths and json.loads(paths[0].read_text()).get("phase") == "playing":
                    break
                time.sleep(0.02)
            else:
                raise RuntimeError("playing state was not persisted")
            process.kill()
            process.wait(timeout=5)
            owner.close(); guest.close()
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            wait_ready(health_port, process)
            metadata = json.loads(paths[0].read_text())
            assert metadata["phase"] == "aborted" and metadata["abortReason"] == "server_restart", metadata
            with connected_client() as replacement:
                assert welcomes[-1]["instanceId"] != old_welcome["instanceId"]
                send_message(replacement, {"type": "Resume", "token": old_welcome["token"],
                                           "instanceId": old_welcome["instanceId"], "code": crash_code})
                assert read_type(replacement, "Error")["code"] == "resume_invalid"
            # Fill the real TCP admission bound without finishing TLS; excess
            # sockets close immediately and handshake deadlines reclaim slots.
            time.sleep(0.1)
            held = [socket.create_connection(("127.0.0.1", game_port), timeout=3) for _ in range(64)]
            try:
                with socket.create_connection(("127.0.0.1", game_port), timeout=3) as excess:
                    assert excess.recv(1) == b"", "65th connection exceeded runtime capacity"
                held[0].settimeout(12)
                started = time.monotonic()
                assert held[0].recv(1) == b"", "unfinished TLS handshake was retained"
                assert time.monotonic() - started < 11.5
            finally:
                for stream in held:
                    stream.close()
            with connected_client():
                pass
            print("TLS smoke passed: verified trust, bounded health/rates, SIGTERM and crash restart")
        finally:
            if process is not None:
                process.terminate()
                try:
                    process.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)


if __name__ == "__main__":
    main()
