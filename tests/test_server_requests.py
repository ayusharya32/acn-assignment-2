import socket
import time
import threading

HOST = "127.0.0.1"
PORT = 9000

HEADER_TIMEOUT = 5
SOCKET_TIMEOUT = 20


def connect_client():
    s = socket.create_connection(
        (HOST, PORT),
        timeout=SOCKET_TIMEOUT
    )

    s.settimeout(SOCKET_TIMEOUT)

    return s


def idle_client(name, results):
    try:
        s = connect_client()

        print(f"[CLIENT] {name} connected")

        start = time.monotonic()

        response = s.recv(1024).decode()

        elapsed = time.monotonic() - start

        results[name] = {
            "response": response,
            "elapsed": elapsed,
        }

        print(
            f"[CLIENT] {name} response "
            f"after {elapsed:.2f}s: {response!r}"
        )

        s.close()

    except Exception as e:
        results[name] = {
            "error": repr(e)
        }

        print(
            f"[CLIENT] {name} error: {e}"
        )


def active_client(name, results):
    try:
        s = connect_client()

        print(f"[CLIENT] {name} connected")

        start = time.monotonic()

        s.sendall(b"HEALTH\n")

        response = s.recv(1024).decode()

        elapsed = time.monotonic() - start

        results[name] = {
            "response": response,
            "elapsed": elapsed,
        }

        print(
            f"[CLIENT] {name} response "
            f"after {elapsed:.2f}s: {response!r}"
        )

        s.close()

    except Exception as e:
        results[name] = {
            "error": repr(e)
        }

        print(
            f"[CLIENT] {name} error: {e}"
        )


def run_test():
    results = {}

    threads = []

    # --------------------------------------------------------
    # STEP 1
    # Connect idle client A.
    #
    # This should reach the parser and block in recv().
    # --------------------------------------------------------

    print("\n=== STEP 1: idle client A ===")

    thread_a = threading.Thread(
        target=idle_client,
        args=("A", results)
    )

    thread_a.start()

    # Give A enough time to:
    #
    # accept()
    # queue
    # parser pop
    # recv()
    #
    time.sleep(0.5)

    # --------------------------------------------------------
    # STEP 2
    # Connect idle clients B and C.
    #
    # They should be accepted and placed in connectionQueue
    # while A is still blocking the parser.
    # --------------------------------------------------------

    print("\n=== STEP 2: idle clients B and C ===")

    thread_b = threading.Thread(
        target=idle_client,
        args=("B", results)
    )

    thread_c = threading.Thread(
        target=idle_client,
        args=("C", results)
    )

    thread_b.start()
    thread_c.start()

    time.sleep(0.5)

    # --------------------------------------------------------
    # STEP 3
    # Connect active clients D-H.
    #
    # These should ALSO be accepted and queued even though
    # the parser is blocked on A.
    # --------------------------------------------------------

    print("\n=== STEP 3: active clients D-H ===")

    active_threads = []

    for name in ["D", "E", "F", "G", "H"]:
        thread = threading.Thread(
            target=active_client,
            args=(name, results)
        )

        active_threads.append(thread)
        thread.start()

    # --------------------------------------------------------
    # STEP 4
    # Wait for everyone.
    # --------------------------------------------------------

    thread_a.join()
    thread_b.join()
    thread_c.join()

    for thread in active_threads:
        thread.join()

    return results


def check_results(results):
    print("\n========================================")
    print("RESULTS")
    print("========================================")

    success = True

    # --------------------------------------------------------
    # Idle clients
    # --------------------------------------------------------

    for name in ["A", "B", "C"]:

        result = results.get(name)

        if result is None:
            print(f"[FAIL] {name}: no result")
            success = False
            continue

        if "error" in result:
            print(
                f"[FAIL] {name}: "
                f"{result['error']}"
            )
            success = False
            continue

        response = result["response"]
        elapsed = result["elapsed"]

        expected = "ERR Failed to receive request header\n"

        if response != expected:
            print(
                f"[FAIL] {name}: "
                f"unexpected response={response!r}"
            )

            success = False
            continue

        print(
            f"[PASS] {name}: timeout after "
            f"{elapsed:.2f}s"
        )

    # --------------------------------------------------------
    # Active clients
    # --------------------------------------------------------

    for name in ["D", "E", "F", "G", "H"]:

        result = results.get(name)

        if result is None:
            print(f"[FAIL] {name}: no result")
            success = False
            continue

        if "error" in result:
            print(
                f"[FAIL] {name}: "
                f"{result['error']}"
            )
            success = False
            continue

        response = result["response"]
        elapsed = result["elapsed"]

        if response != "OK 0\n":
            print(
                f"[FAIL] {name}: "
                f"unexpected response={response!r}"
            )

            success = False
            continue

        print(
            f"[PASS] {name}: HEALTH after "
            f"{elapsed:.2f}s"
        )

    return success


print("Starting deterministic parser concurrency test")

results = run_test()

if check_results(results):
    print("\n[PASS] Complete concurrency test")
else:
    print("\n[FAIL] Complete concurrency test")