#!/usr/bin/env python3
import http.client
import json
import pathlib
import socket
import ssl
import subprocess
import sys
import tempfile
import time
import threading


def free_port():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


def stalled_tls_test(godot, project, cert, key):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(cert, key)
    done = threading.Event()
    with socket.socket() as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        listener.settimeout(10)

        def serve():
            with listener.accept()[0] as raw:
                with context.wrap_socket(raw, server_side=True):
                    done.wait(10)

        worker = threading.Thread(target=serve, daemon=True)
        worker.start()
        try:
            result = subprocess.run([
                str(godot), "--headless", "--path", str(project), "--script",
                "res://tests/central_tls_backpressure.gd", "--",
                str(listener.getsockname()[1]), str(cert),
            ], capture_output=True, text=True, timeout=8)
            if result.returncode or "ERROR:" in result.stderr:
                raise RuntimeError("Godot stalled TLS test failed:\n" + result.stdout + result.stderr)
            print(result.stdout.strip())
        finally:
            done.set()
            worker.join(timeout=2)


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


def launch_client(godot, project, assets, role, port, cert, user_data, report, room_code=""):
    command = [
        str(godot), "--headless", "--path", str(project), "--script",
        "res://tests/central_frontend_client.gd", "--", "--role", role,
        "--hostname", "localhost", "--port", str(port), "--trusted-ca", str(cert),
        "--assets", str(assets), "--user-data", str(user_data), "--report", str(report),
    ]
    if room_code:
        command.extend(["--room-code", room_code])
    return subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)


def wait_for_room_code(report, process):
    deadline = time.monotonic() + 25
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError("owner Godot client exited before creating a room: " + process.stdout.read())
        if report.exists():
            value = json.loads(report.read_text())
            if value.get("connected") is True and len(value.get("roomCode", "")) == 8:
                return value["roomCode"]
        time.sleep(0.05)
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)
    raise RuntimeError("owner Godot client did not publish a room code:\n" + process.stdout.read())


def main():
    binary = pathlib.Path(sys.argv[1]).resolve()
    godot = pathlib.Path(sys.argv[2]).resolve()
    project = pathlib.Path(sys.argv[3]).resolve()
    assets = pathlib.Path(sys.argv[4]).resolve()
    with tempfile.TemporaryDirectory(prefix="wizard-central-godot-") as temporary:
        root = pathlib.Path(temporary)
        cert, key = root / "server.crt", root / "server.key"
        subprocess.run([
            "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
            "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost",
            "-keyout", str(key), "-out", str(cert),
        ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        stalled_tls_test(godot, project, cert, key)
        game_port, health_port = free_port(), free_port()
        process = subprocess.Popen([
            str(binary), "--assets", str(assets), "--data", str(root / "rooms"),
            "--cert", str(cert), "--key", str(key), "--bind", "127.0.0.1",
            "--port", str(game_port), "--health-port", str(health_port),
        ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        clients = []
        try:
            wait_ready(health_port, process)
            owner_data, guest_data = root / "owner-data", root / "guest-data"
            owner_report, guest_report = root / "owner.json", root / "guest.json"
            owner = launch_client(godot, project, assets, "host", game_port, cert, owner_data, owner_report)
            clients.append(owner)
            code = wait_for_room_code(owner_report, owner)
            guest = launch_client(godot, project, assets, "guest", game_port, cert, guest_data, guest_report, code)
            clients.append(guest)
            deadline = time.monotonic() + 60
            while any(client.poll() is None for client in clients):
                failed = [client for client in clients if client.poll() not in (None, 0)]
                if failed:
                    diagnostics = "\n".join(client.stdout.read() for client in failed)
                    raise RuntimeError("a Godot central client failed:\n" + diagnostics)
                if time.monotonic() >= deadline:
                    raise RuntimeError("Godot central clients did not finish within 60 seconds")
                time.sleep(0.05)
            output = "\n".join(client.stdout.read() for client in clients)
            if any(client.returncode != 0 for client in clients):
                raise RuntimeError("Godot central frontend smoke failed:\n" + output)
            final_reports = [json.loads(owner_report.read_text()), json.loads(guest_report.read_text())]
            if any(row.get("phase") != "completed" or row.get("replayDelivered") is not True for row in final_reports):
                raise RuntimeError("both clients did not report a completed match and saved replay")
            if any(row.get("roomCode") != code for row in final_reports):
                raise RuntimeError("the two Godot clients reported different rooms")
            owner_replays = sorted((owner_data / "replays").glob("network-*.json"))
            guest_replays = sorted((guest_data / "replays").glob("network-*.json"))
            if len(owner_replays) != 1 or len(guest_replays) != 1:
                raise RuntimeError("expected one saved replay from each Godot client")
            records = [json.loads(owner_replays[0].read_text()), json.loads(guest_replays[0].read_text())]
            if records[0] != records[1]:
                raise RuntimeError("the two clients saved different terminal replay records")
            metadata_path = next((root / "rooms").glob(code + "-*/metadata.json"))
            metadata = json.loads(metadata_path.read_text())
            if metadata.get("phase") != "completed":
                raise RuntimeError("server journal did not record the completed match")
            print(output.strip())
            print("CENTRAL_GODOT_SMOKE: both native TLS clients saved the same verified replay")
        finally:
            for client in clients:
                if client.poll() is None:
                    client.terminate()
            for client in clients:
                if client.poll() is None:
                    try:
                        client.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        client.kill()
                        client.wait(timeout=5)
            if process.poll() is None:
                process.terminate()
            try:
                returncode = process.wait(timeout=15) if process.poll() is None else process.returncode
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
                raise RuntimeError("server did not stop after SIGTERM")
            server_output = process.stdout.read()
            if returncode != 0:
                raise RuntimeError(f"server exited {returncode}: {server_output}")


if __name__ == "__main__":
    main()
