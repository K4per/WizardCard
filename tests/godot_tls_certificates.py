"""Test the real Godot transport against valid and invalid local TLS servers."""
import argparse
import datetime
import json
from pathlib import Path
import socket
import ssl
import subprocess
import tempfile
import threading

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID


def certificate(root, name, expired=False):
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    subject = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "localhost")])
    now = datetime.datetime.now(datetime.timezone.utc)
    cert = (x509.CertificateBuilder().subject_name(subject).issuer_name(subject).public_key(key.public_key())
            .serial_number(x509.random_serial_number()).not_valid_before(now - datetime.timedelta(days=3))
            .not_valid_after(now + datetime.timedelta(days=-1 if expired else 2))
            .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True)
            .add_extension(x509.SubjectAlternativeName([x509.DNSName("localhost")]), critical=False)
            .sign(key, hashes.SHA256()))
    crt, pem = root / (name + ".crt"), root / (name + ".key")
    crt.write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    pem.write_bytes(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
    return crt, pem


def server(crt, key, stop):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(crt, key)
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen()
    listener.settimeout(.2)

    def run():
        with listener:
            while not stop.is_set():
                try:
                    peer, _ = listener.accept()
                except socket.timeout:
                    continue
                try:
                    peer.settimeout(3)
                    with context.wrap_socket(peer, server_side=True) as tls:
                        while not stop.is_set():
                            if not tls.recv(1024):
                                break
                except (OSError, ssl.SSLError):
                    peer.close()
    thread = threading.Thread(target=run, daemon=True)
    thread.start()
    return listener.getsockname()[1], thread


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", required=True)
    parser.add_argument("--project", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="wizard-tls-certificates-") as directory:
        root = Path(directory)
        valid, valid_key = certificate(root, "valid")
        expired, expired_key = certificate(root, "expired", True)
        other, _ = certificate(root, "other")
        stop = threading.Event()
        valid_port, valid_thread = server(valid, valid_key, stop)
        expired_port, expired_thread = server(expired, expired_key, stop)
        with socket.socket() as unused:
            unused.bind(("127.0.0.1", 0))
            unused_port = unused.getsockname()[1]
        cases = [
            {"name": "valid", "port": valid_port, "ca": str(valid), "host": "localhost", "accepted": True},
            {"name": "wrong-ca", "port": valid_port, "ca": str(other), "host": "localhost", "accepted": False},
            {"name": "retry-after-wrong-ca", "port": valid_port, "ca": str(valid), "host": "localhost", "accepted": True},
            {"name": "wrong-hostname", "port": valid_port, "ca": str(valid), "host": "127.0.0.1", "accepted": False},
            {"name": "expired", "port": expired_port, "ca": str(expired), "host": "localhost", "accepted": False},
            {"name": "unreachable", "port": unused_port, "ca": str(valid), "host": "127.0.0.1", "accepted": False, "reason": "不可达"},
            {"name": "retry-after-failures", "port": valid_port, "ca": str(valid), "host": "localhost", "accepted": True},
        ]
        config = root / "cases.json"
        config.write_text(json.dumps(cases), encoding="utf-8")
        try:
            result = subprocess.run([args.godot, "--headless", "--path", str(args.project), "--script",
                                     "res://tests/tls_certificate_probe.gd", "--", str(config)], timeout=120)
            raise SystemExit(result.returncode)
        finally:
            stop.set()
            valid_thread.join(4)
            expired_thread.join(4)


if __name__ == "__main__":
    main()
