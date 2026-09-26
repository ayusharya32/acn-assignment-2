import os
import signal
import socket
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor


HOST = "127.0.0.1"
PORT = 9000

FILE_DIR = "files"
CONFIG = "config.json"

SIZES = [1000, 2000, 3000, 4000, 5000, 6000, 3000, 5000]


def create_get_files():
    os.makedirs(FILE_DIR, exist_ok=True)

    for i, size in enumerate(SIZES):
        with open(os.path.join(FILE_DIR, f"get_{i}.txt"), "wb") as f:
            f.write(b"X" * size)


def send_get(index, start_event):
    start_event.wait()

    s = socket.socket()
    s.settimeout(10)

    try:
        s.connect((HOST, PORT))
        s.sendall(f"GET get_{index}.txt\n".encode())

        # Consume the response.
        while True:
            data = s.recv(4096)
            if not data:
                break

    except Exception as e:
        print(f"[GET {index}] {e}")

    finally:
        s.close()


def send_put(index, start_event):
    size = SIZES[index]

    start_event.wait()

    s = socket.socket()
    s.settimeout(10)

    try:
        s.connect((HOST, PORT))

        header = f"PUT put_{index}.txt {size}\n".encode()
        s.sendall(header)

        # Server should send OK 0 before body.
        response = s.recv(1024)

        if not response.startswith(b"OK"):
            print(f"[PUT {index}] unexpected response: {response}")
            return

        # Send exactly the declared number of bytes.
        s.sendall(b"X" * size)

        # Server confirmation.
        s.recv(1024)

    except Exception as e:
        print(f"[PUT {index}] {e}")

    finally:
        s.close()


def run_batch(scheduler, operation):
    print()
    print("=" * 70)
    print(f"{scheduler.upper()} + {operation.upper()}")
    print("=" * 70)

    server = subprocess.Popen(
        [
            "./build/server",
            "--sched", scheduler,
            "--file", FILE_DIR,
            "--config", CONFIG,
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    try:
        # Give server enough time to bind/listen.
        # IMPORTANT: do NOT connect here, because that would create
        # an extra idle connection for the parser.
        time.sleep(0.5)

        start_event = __import__("threading").Event()

        target = send_get if operation == "get" else send_put

        # Start all 8 client threads first.
        with ThreadPoolExecutor(max_workers=8) as executor:

            futures = [
                executor.submit(target, i, start_event)
                for i in range(8)
            ]

            # Release all eight clients approximately simultaneously.
            start_event.set()

            # Wait for all clients, but never forever.
            for future in futures:
                try:
                    future.result(timeout=15)
                except Exception as e:
                    print(f"[CLIENT ERROR] {e}")

        # Allow server output to settle.
        time.sleep(0.5)

    finally:
        if server.poll() is None:
            server.send_signal(signal.SIGINT)

        try:
            output, _ = server.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            output, _ = server.communicate()

    print("\nSERVER OUTPUT:")
    print(output)

    print("WORKER ORDER:")

    for line in output.splitlines():
        if "[WORKER]" in line:
            print(line)


def main():
    create_get_files()

    print("Request sizes:")
    print(SIZES)

    print("\nArrival order:")
    print(SIZES)

    print("\nExpected FCFS order:")
    print(SIZES)

    print("\nExpected SJF order:")
    print(sorted(SIZES))

    run_batch("fcfs", "get")
    run_batch("sjf", "get")
    run_batch("fcfs", "put")
    run_batch("sjf", "put")


if __name__ == "__main__":
    main()