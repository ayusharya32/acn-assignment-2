import os
import random


FILES_DIR = "./files"

# Approximate assignment workload sizes.
SMALL_SIZE = 1 * 1024
MEDIUM_SIZE = 30 * 1024
LARGE_SIZE = 150 * 1024

# Short lines should be around 60-80 bytes.
LINE_MIN = 60
LINE_MAX = 80

# Q in our experiments will be 4096 bytes.
# This line is deliberately larger than Q.
LONG_LINE_SIZE = 5001


def make_line(length):
    """Create a deterministic line of approximately the requested size."""
    prefix = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

    if length < 2:
        return "x\n"

    body_length = length - 1  # reserve one byte for '\n'

    body = (prefix * ((body_length // len(prefix)) + 1))[:body_length]

    return body + "\n"


def generate_normal_file(path, target_size):
    """
    Generate a file containing short 60-80 byte lines.

    The final file may be slightly larger than target_size so that
    every line remains within the required 60-80 byte range.
    """
    with open(path, "wb") as f:
        total = 0

        while total < target_size:
            remaining = target_size - total

            # If the remaining space is smaller than the minimum
            # line size, stop and allow the file to exceed the target
            # slightly on the previous iteration.
            if remaining < LINE_MIN:
                break

            line_size = random.randint(LINE_MIN, LINE_MAX)

            # Don't create a line that would leave an invalid
            # remainder smaller than LINE_MIN.
            if remaining - line_size != 0 and remaining - line_size < LINE_MIN:
                line_size = remaining

                # If remaining is > LINE_MAX, this should not happen,
                # but keep the check for safety.
                if line_size > LINE_MAX:
                    line_size = LINE_MAX

            line = make_line(line_size).encode()

            f.write(line)
            total += len(line)

        # If the target was not reached because the remaining amount
        # was smaller than LINE_MIN, add one final valid line.
        if total < target_size:
            remaining = target_size - total

            if remaining < LINE_MIN:
                line_size = LINE_MIN
                f.write(make_line(line_size).encode())


def generate_long_line_file(path):
    """
    Generate a file whose first line is larger than Q=4096.

    The first line is 5001 bytes including the newline.
    """
    with open(path, "wb") as f:
        f.write(make_line(LONG_LINE_SIZE).encode())

        # Add normal short lines afterwards.
        for _ in range(20):
            f.write(make_line(70).encode())


def report_file(path):
    size = os.path.getsize(path)

    print(f"{path}")
    print(f"  size: {size} bytes")

    with open(path, "rb") as f:
        first_line = f.readline()

    print(f"  first line: {len(first_line)} bytes")


def main():
    os.makedirs(FILES_DIR, exist_ok=True)

    random.seed(42)

    small_path = os.path.join(FILES_DIR, "workload_small.txt")
    medium_path = os.path.join(FILES_DIR, "workload_medium.txt")
    large_path = os.path.join(FILES_DIR, "workload_large.txt")
    long_path = os.path.join(FILES_DIR, "workload_longline.txt")

    generate_normal_file(small_path, SMALL_SIZE)
    generate_normal_file(medium_path, MEDIUM_SIZE)
    generate_normal_file(large_path, LARGE_SIZE)
    generate_long_line_file(long_path)

    print("Generated workload files:")
    print()

    report_file(small_path)
    report_file(medium_path)
    report_file(large_path)
    report_file(long_path)


if __name__ == "__main__":
    main()