import socket
import os
import time
import threading

SERVER_HOST = "127.0.0.1"
SERVER_PORT = 9000
FILES_DIR = "./files"


def send_get(filename):
    """Send one GET request and read the complete response."""
    with socket.create_connection((SERVER_HOST, SERVER_PORT), timeout=10) as s:
        request = f"GET {filename}\n".encode()
        s.sendall(request)

        # Read response header.
        data = b""
        while b"\n" not in data:
            chunk = s.recv(4096)
            if not chunk:
                raise RuntimeError("Connection closed while reading GET header")
            data += chunk

        header, body = data.split(b"\n", 1)
        header = header.decode()

        parts = header.split()
        if len(parts) != 2 or parts[0] != "OK":
            raise RuntimeError(f"GET failed: {header}")

        expected_size = int(parts[1])

        # We may already have received some body bytes.
        while len(body) < expected_size:
            chunk = s.recv(min(65536, expected_size - len(body)))
            if not chunk:
                raise RuntimeError("Connection closed before GET body completed")
            body += chunk

        if len(body) != expected_size:
            raise RuntimeError(
                f"GET size mismatch: expected {expected_size}, got {len(body)}"
            )

        return body


def send_put(filename, body):
    """Send one PUT request and verify both OK responses."""
    with socket.create_connection((SERVER_HOST, SERVER_PORT), timeout=10) as s:
        header = f"PUT {filename} {len(body)}\n".encode()
        s.sendall(header)

        # Initial OK 0.
        response = b""
        while b"\n" not in response:
            chunk = s.recv(4096)
            if not chunk:
                raise RuntimeError("Connection closed waiting for PUT response")
            response += chunk

        first_line, extra = response.split(b"\n", 1)

        if first_line != b"OK 0":
            raise RuntimeError(
                f"Unexpected initial PUT response: {first_line!r}"
            )

        # Send body.
        if extra:
            # Normally extra should be empty because this client waits
            # for the initial response before sending the body.
            raise RuntimeError(
                f"Unexpected bytes after initial PUT response: {extra!r}"
            )

        s.sendall(body)

        # Final OK 0.
        response = b""
        while b"\n" not in response:
            chunk = s.recv(4096)
            if not chunk:
                raise RuntimeError("Connection closed waiting for final PUT response")
            response += chunk

        final_line = response.split(b"\n", 1)[0]

        if final_line != b"OK 0":
            raise RuntimeError(
                f"Unexpected final PUT response: {final_line!r}"
            )


def make_test_files():
    os.makedirs(FILES_DIR, exist_ok=True)

    # Small file: several short lines.
    with open(f"{FILES_DIR}/small.txt", "wb") as f:
        f.write(
            b"line-1\n"
            b"line-2\n"
            b"line-3\n"
            b"line-4\n"
            b"line-5\n"
        )

    # File with a long line.
    #
    # Q = 4096, so this first line is deliberately > Q.
    with open(f"{FILES_DIR}/longline.txt", "wb") as f:
        f.write(b"A" * 5000 + b"\n")
        f.write(b"short-line\n")

    # Medium file.
    with open(f"{FILES_DIR}/medium.txt", "wb") as f:
        for i in range(1000):
            f.write(f"line-{i:04d}\n".encode())

    # Empty file.
    open(f"{FILES_DIR}/empty.txt", "wb").close()


def sequential_tests():
    print("\n=== Sequential GET tests ===")

    for filename in [
        "small.txt",
        "longline.txt",
        "medium.txt",
        "empty.txt",
    ]:
        print(f"GET {filename}")
        body = send_get(filename)

        expected = open(f"{FILES_DIR}/{filename}", "rb").read()

        assert body == expected, f"GET mismatch for {filename}"

        print(f"  OK ({len(body)} bytes)")


def put_tests():
    print("\n=== PUT tests ===")

    tests = [
        ("put-small.txt", b"hello\nworld\n"),
        ("put-medium.txt", b"X" * 10000),
        ("put-lines.txt", b"line\n" * 2000),
    ]

    for filename, body in tests:
        print(f"PUT {filename} ({len(body)} bytes)")

        send_put(filename, body)

        path = f"{FILES_DIR}/{filename}"
        stored = open(path, "rb").read()

        assert stored == body, f"PUT mismatch for {filename}"

        print("  OK")


def concurrent_get_tests():
    print("\n=== Concurrent GET tests ===")

    filenames = [
        "small.txt",
        "longline.txt",
        "medium.txt",
        "small.txt",
        "longline.txt",
        "medium.txt",
        "small.txt",
        "longline.txt",
    ]

    results = [None] * len(filenames)
    errors = []

    def worker(i, filename):
        try:
            results[i] = send_get(filename)
        except Exception as e:
            errors.append((i, filename, e))

    threads = []

    for i, filename in enumerate(filenames):
        t = threading.Thread(target=worker, args=(i, filename))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    if errors:
        for error in errors:
            print("ERROR:", error)
        raise RuntimeError("Concurrent GET test failed")

    for i, filename in enumerate(filenames):
        expected = open(f"{FILES_DIR}/{filename}", "rb").read()
        assert results[i] == expected

    print(f"  OK ({len(filenames)} concurrent requests)")


def concurrent_mixed_tests():
    print("\n=== Concurrent mixed GET/PUT tests ===")

    errors = []

    def get_worker(filename):
        try:
            send_get(filename)
        except Exception as e:
            errors.append(("GET", filename, e))

    def put_worker(filename, body):
        try:
            send_put(filename, body)

            stored = open(f"{FILES_DIR}/{filename}", "rb").read()
            if stored != body:
                errors.append(("PUT", filename, "stored data mismatch"))
        except Exception as e:
            errors.append(("PUT", filename, e))

    threads = []

    for i in range(5):
        threads.append(
            threading.Thread(
                target=get_worker,
                args=("longline.txt",)
            )
        )

        threads.append(
            threading.Thread(
                target=put_worker,
                args=(
                    f"concurrent-put-{i}.txt",
                    bytes([65 + i]) * 7000,
                )
            )
        )

    for t in threads:
        t.start()

    for t in threads:
        t.join()

    if errors:
        for error in errors:
            print("ERROR:", error)
        raise RuntimeError("Mixed concurrent test failed")

    print(f"  OK ({len(threads)} concurrent requests)")


def main():
    make_test_files()

    # Give the server a moment to be ready.
    time.sleep(0.5)

    sequential_tests()
    put_tests()
    concurrent_get_tests()
    concurrent_mixed_tests()

    print("\n===================================")
    print("ALL TESTS PASSED")
    print("===================================")


if __name__ == "__main__":
    main()