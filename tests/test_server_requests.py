import socket
import time

HOST = "127.0.0.1"
PORT = 9000


def send_request(request):
    s = socket.create_connection((HOST, PORT))
    s.sendall(request)
    response = s.recv(1024).decode()
    s.close()
    return response


tests = [
    ("HEALTH", b"HEALTH\n", "OK 0\n"),
    ("GET", b"GET file.txt\n", "OK 0\n"),
    ("PUT zero", b"PUT file.txt 0\n", "OK 0\n"),
    ("PUT", b"PUT file.txt 100\n", "OK 0\n"),

    ("Unknown type", b"INVALID\n", "ERR Unknown request type\n"),
    ("Empty", b"\n", "ERR Malformed request\n"),
    ("Whitespace", b"   \n", "ERR Malformed request\n"),

    ("GET missing filename", b"GET\n", "ERR Missing filename\n"),
    ("GET extra count", b"GET file.txt 100\n", "ERR Malformed request\n"),
    ("GET extra token", b"GET file.txt extra\n", "ERR Malformed request\n"),

    ("PUT missing count", b"PUT file.txt\n", "ERR Missing byte count\n"),
    ("PUT non-numeric", b"PUT file.txt abc\n", "ERR Invalid byte count\n"),
    ("PUT negative", b"PUT file.txt -10\n", "ERR Invalid byte count\n"),
    ("PUT decimal", b"PUT file.txt 10.5\n", "ERR Invalid byte count\n"),
    ("PUT extra token", b"PUT file.txt 100 extra\n", "ERR Malformed request\n"),

    ("GET slash", b"GET foo/bar\n", "ERR Invalid filename\n"),
    ("GET dot", b"GET .\n", "ERR Invalid filename\n"),
    ("GET dotdot", b"GET ..\n", "ERR Invalid filename\n"),
    ("PUT slash", b"PUT foo/bar 10\n", "ERR Invalid filename\n"),

    ("Lowercase GET", b"get file.txt\n", "OK 0\n"),
    ("Lowercase PUT", b"put file.txt 10\n", "OK 0\n"),
    ("Lowercase HEALTH", b"health\n", "OK 0\n"),
]


print(f"Running {len(tests)} tests...\n")

passed = 0

for name, request, expected in tests:
    try:
        actual = send_request(request)

        if actual == expected:
            print(f"[PASS] {name}")
            passed += 1
        else:
            print(f"[FAIL] {name}")
            print(f"       Expected: {expected!r}")
            print(f"       Actual:   {actual!r}")

    except Exception as e:
        print(f"[FAIL] {name}")
        print(f"       Error: {e}")

print()
print(f"{passed}/{len(tests)} tests passed")

