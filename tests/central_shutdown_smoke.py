#!/usr/bin/env python3
"""Exercise the shutdown deadline with a real blocked fsync via LD_PRELOAD."""
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import time

from central_tls_smoke import expanded_deck, free_port, read_message, send_message


def main():
    binary, assets = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix="wizard-shutdown-") as temporary:
        root = Path(temporary)
        source, library = root / "stall.c", root / "stall.so"
        source.write_text('''
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
int fsync(int fd) {
    const char *marker = getenv("WIZARD_FSYNC_STALL");
    if (marker && access(marker, F_OK) == 0) {
        const char *entered = getenv("WIZARD_FSYNC_ENTERED");
        int signal_fd = open(entered, O_CREAT | O_WRONLY, 0600);
        if (signal_fd >= 0) close(signal_fd);
        sleep(30);
    }
    int (*original)(int) = dlsym(RTLD_NEXT, "fsync");
    return original(fd);
}
''')
        subprocess.run(["cc", "-shared", "-fPIC", str(source), "-ldl", "-o", str(library)], check=True)
        cert, key = root / "server.crt", root / "server.key"
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                        "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost",
                        "-keyout", str(key), "-out", str(cert)], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        port = free_port()
        environment = dict(os.environ, LD_PRELOAD=str(library), WIZARD_FSYNC_STALL=str(root / "stall"),
                           WIZARD_FSYNC_ENTERED=str(root / "entered"))
        process = subprocess.Popen([str(binary), "--assets", str(assets), "--data", str(root / "data"),
                                    "--cert", str(cert), "--key", str(key), "--bind", "127.0.0.1",
                                    "--port", str(port), "--health-port", str(free_port())], env=environment,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            deadline = time.monotonic() + 10
            while True:
                try:
                    raw = socket.create_connection(("127.0.0.1", port), timeout=1)
                    break
                except OSError:
                    if time.monotonic() >= deadline:
                        raise RuntimeError("server startup timed out")
                    time.sleep(0.05)
            context = ssl.create_default_context(cafile=str(cert))
            with context.wrap_socket(raw, server_hostname="localhost") as stream:
                metadata = json.loads((assets / "catalog.json").read_text())["metadata"]
                send_message(stream, {"type": "Hello", "protocol": 5, "program": "2.6.3", "rules": metadata["rulesVersion"],
                                      "cards": metadata["cardSetVersion"], "mechanics": metadata["mechanicsRevision"],
                                      "hash": "cc7c6005e8f96820"})
                assert read_message(stream)["type"] == "Welcome"
                (root / "stall").touch()
                send_message(stream, {"type": "CreateRoom", "deck": expanded_deck(assets)})
                assert read_message(stream)["type"] == "RoomJoined"
                deadline = time.monotonic() + 5
                while not (root / "entered").exists():
                    if time.monotonic() >= deadline:
                        raise RuntimeError("blocked fsync was not reached")
                    time.sleep(0.02)
                started = time.monotonic()
                process.terminate()
                assert process.wait(timeout=10) == 0
                elapsed = time.monotonic() - started
                assert elapsed < 9, f"SIGTERM exceeded shutdown deadline: {elapsed}"
                print(f"CENTRAL_SHUTDOWN: blocked fsync stopped in {elapsed:.2f}s")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)


if __name__ == "__main__":
    main()
