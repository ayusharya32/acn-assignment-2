import subprocess
import threading


files = [
    "client_files/a.txt",
    "client_files/b.txt",
    "client_files/c.txt",
    "client_files/d.txt",
    "client_files/e.txt",
]


def run_put(file_path):
    print(f"[START] PUT {file_path}")

    result = subprocess.run(
        ["./build/client", "put", file_path],
        capture_output=True,
        text=True
    )

    if result.returncode == 0:
        print(f"[SUCCESS] PUT {file_path}")
    else:
        print(f"[FAILED] PUT {file_path}")
        if result.stdout:
            print(result.stdout.strip())
        if result.stderr:
            print(result.stderr.strip())


threads = []

for file_path in files:
    thread = threading.Thread(target=run_put, args=(file_path,))
    thread.start()
    threads.append(thread)

for thread in threads:
    thread.join()

print("\nAll client PUT processes finished.")