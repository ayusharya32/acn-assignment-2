import socket
import threading
import time
import sys

SERVER_HOST = "127.0.0.1"
SERVER_PORT = 9000

REQUEST_FILE = "tests/workload_requests.txt"

NUM_CLIENT_THREADS = 16
SOCKET_TIMEOUT = 10


class Result:
    def __init__(self):
        self.success = 0
        self.failed = 0
        self.lock = threading.Lock()

    def record_success(self):
        with self.lock:
            self.success += 1

    def record_failure(self):
        with self.lock:
            self.failed += 1


def recv_exact(sock, num_bytes):
    """Receive exactly num_bytes bytes."""
    data = bytearray()

    while len(data) < num_bytes:
        chunk = sock.recv(min(65536, num_bytes - len(data)))

        if not chunk:
            raise RuntimeError(
                f"Connection closed early: "
                f"received {len(data)}/{num_bytes} bytes"
            )

        data.extend(chunk)

    return bytes(data)


def recv_line(sock):
    """Receive one newline-terminated line."""
    data = bytearray()

    while True:
        byte = sock.recv(1)

        if not byte:
            raise RuntimeError("Connection closed while reading response")

        data.extend(byte)

        if byte == b"\n":
            return bytes(data)


def send_get(filename):
    with socket.create_connection(
        (SERVER_HOST, SERVER_PORT),
        timeout=SOCKET_TIMEOUT,
    ) as sock:

        request = f"GET {filename}\n".encode()
        sock.sendall(request)

        response = recv_line(sock).decode().strip()

        parts = response.split()

        if len(parts) != 2 or parts[0] != "OK":
            raise RuntimeError(f"Invalid GET response: {response}")

        try:
            size = int(parts[1])
        except ValueError:
            raise RuntimeError(f"Invalid GET size: {response}")

        if size < 0:
            raise RuntimeError(f"Invalid GET size: {response}")

        body = recv_exact(sock, size)

        if len(body) != size:
            raise RuntimeError(
                f"GET body size mismatch: {len(body)} != {size}"
            )


def send_put(destination, source_file, expected_size):
    with open(f"files/{source_file}", "rb") as f:
        body = f.read()

    if len(body) != expected_size:
        raise RuntimeError(
            f"PUT source size mismatch for {source_file}: "
            f"{len(body)} != {expected_size}"
        )

    with socket.create_connection(
        (SERVER_HOST, SERVER_PORT),
        timeout=SOCKET_TIMEOUT,
    ) as sock:

        request = f"PUT {destination} {expected_size}\n".encode()
        sock.sendall(request)

        # Initial PUT response.
        response = recv_line(sock).decode().strip()

        if response != "OK 0":
            raise RuntimeError(
                f"Invalid initial PUT response: {response}"
            )

        # Send the exact body.
        sock.sendall(body)

        # Final PUT response.
        response = recv_line(sock).decode().strip()

        if response != "OK 0":
            raise RuntimeError(
                f"Invalid final PUT response: {response}"
            )


def execute_request(request):
    parts = request.split()

    if not parts:
        raise RuntimeError("Empty request")

    operation = parts[0]

    if operation == "GET":
        if len(parts) != 2:
            raise RuntimeError(f"Invalid GET request: {request}")

        send_get(parts[1])

    elif operation == "PUT":
        if len(parts) != 4:
            raise RuntimeError(f"Invalid PUT request: {request}")

        destination = parts[1]
        source_file = parts[2]

        try:
            expected_size = int(parts[3])
        except ValueError:
            raise RuntimeError(f"Invalid PUT size: {request}")

        send_put(destination, source_file, expected_size)

    else:
        raise RuntimeError(f"Unknown operation: {operation}")


def worker(requests, next_index, index_lock, result):
    while True:
        with index_lock:
            if next_index[0] >= len(requests):
                return

            index = next_index[0]
            next_index[0] += 1

        request = requests[index]

        try:
            execute_request(request)
            result.record_success()

        except Exception as e:
            result.record_failure()

            print(
                f"[FAILED] request {index + 1}: "
                f"{request} -- {e}"
            )


def main():
    if len(sys.argv) > 2:
        print(
            f"Usage: python3 {sys.argv[0]} "
            f"[number_of_client_threads]"
        )
        return 1

    num_threads = NUM_CLIENT_THREADS

    if len(sys.argv) == 2:
        try:
            num_threads = int(sys.argv[1])
        except ValueError:
            print("Invalid number of client threads")
            return 1

        if num_threads <= 0:
            print("Number of client threads must be positive")
            return 1

    with open(REQUEST_FILE) as f:
        requests = [
            line.strip()
            for line in f
            if line.strip()
        ]

    if not requests:
        print("No requests found")
        return 1

    print(f"Requests: {len(requests)}")
    print(f"Client threads: {num_threads}")
    print(f"Server: {SERVER_HOST}:{SERVER_PORT}")
    print()

    result = Result()
    index_lock = threading.Lock()
    next_index = [0]

    threads = []

    start_time = time.monotonic()

    for _ in range(num_threads):
        thread = threading.Thread(
            target=worker,
            args=(
                requests,
                next_index,
                index_lock,
                result,
            ),
        )

        thread.start()
        threads.append(thread)

    for thread in threads:
        thread.join()

    elapsed = time.monotonic() - start_time

    print()
    print("===== Load Generator Summary =====")
    print(f"Total requests: {len(requests)}")
    print(f"Successful: {result.success}")
    print(f"Failed: {result.failed}")
    print(f"Elapsed time: {elapsed:.3f} seconds")

    if elapsed > 0:
        print(
            f"Load-generator throughput: "
            f"{len(requests) / elapsed:.2f} requests/sec"
        )

    return 0 if result.failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())