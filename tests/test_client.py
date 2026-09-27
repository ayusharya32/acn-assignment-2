import socket
import sys


HOST = "127.0.0.1"
PORT = 9000


def recv_until_newline(sock):
    data = b""

    while b"\n" not in data:
        chunk = sock.recv(4096)
        if not chunk:
            raise RuntimeError("Connection closed before response header")
        data += chunk

    header, extra = data.split(b"\n", 1)
    return header.decode(), extra


def test_health():
    with socket.create_connection((HOST, PORT)) as sock:
        sock.sendall(b"HEALTH\n")

        header, _ = recv_until_newline(sock)

        print("HEALTH:", header)


def test_get(filename):
    with socket.create_connection((HOST, PORT)) as sock:
        sock.sendall(f"GET {filename}\n".encode())

        header, extra = recv_until_newline(sock)

        print("GET response:", header)

        parts = header.split()

        if len(parts) != 2 or parts[0] != "OK":
            print("GET failed")
            return

        size = int(parts[1])
        data = extra

        while len(data) < size:
            chunk = sock.recv(min(8192, size - len(data)))

            if not chunk:
                raise RuntimeError("Connection closed before full GET response")

            data += chunk

        data = data[:size]

        print(f"Received {len(data)}/{size} bytes")

        if len(data) == size:
            print("GET successful")


def test_put(filename, data):
    body = data.encode()

    with socket.create_connection((HOST, PORT)) as sock:
        sock.sendall(f"PUT {filename} {len(body)}\n".encode())

        header, extra = recv_until_newline(sock)

        print("PUT initial response:", header)

        if header != "OK 0":
            print("PUT failed at initial response")
            return

        # Send body
        sock.sendall(body)

        # Final response
        header, _ = recv_until_newline(sock)

        print("PUT final response:", header)

        if header == "OK 0":
            print("PUT successful")


def main():
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python3 test_client.py health")
        print("  python3 test_client.py get <filename>")
        print("  python3 test_client.py put <filename> <data>")
        return

    command = sys.argv[1].lower()

    if command == "health":
        test_health()

    elif command == "get":
        test_get(sys.argv[2])

    elif command == "put":
        test_put(sys.argv[2], sys.argv[3])

    else:
        print("Unknown command")


if __name__ == "__main__":
    main()