import random

NUM_REQUESTS = 1000
OUTPUT_FILE = "tests/workload_requests.txt"

FILE_CLASSES = [
    ("small", "workload_small.txt", 1083),
    ("medium", "workload_medium.txt", 30720),
    ("large", "workload_large.txt", 153637),
]

RANDOM_SEED = 42


def main():
    random.seed(RANDOM_SEED)

    # 1000 requests, balanced across the 6 combinations:
    # GET/PUT × small/medium/large.
    counts = {
        ("GET", "small"): 167,
        ("GET", "medium"): 167,
        ("GET", "large"): 167,
        ("PUT", "small"): 167,
        ("PUT", "medium"): 166,
        ("PUT", "large"): 166,
    }

    requests = []

    for operation, class_name in counts:
        count = counts[(operation, class_name)]

        for _ in range(count):
            for name, filename, size in FILE_CLASSES:
                if name == class_name:
                    break

            if operation == "GET":
                requests.append(
                    f"GET {filename}"
                )
            else:
                request_id = len(requests) + 1
                put_filename = (
                    f"experiment_put_{request_id:04d}_{class_name}.bin"
                )

                requests.append(
                    f"PUT {put_filename} {filename} {size}"
                )

    random.shuffle(requests)

    with open(OUTPUT_FILE, "w") as f:
        for request in requests:
            f.write(request + "\n")

    print(f"Generated {len(requests)} requests")
    print(f"Output: {OUTPUT_FILE}")
    print()

    distribution = {}

    for request in requests:
        parts = request.split()
        operation = parts[0]

        if operation == "GET":
            filename = parts[1]
            key = f"GET {filename}"
        else:
            filename = parts[2]
            key = f"PUT {filename}"

        distribution[key] = distribution.get(key, 0) + 1

    print("Request distribution:")

    for key in sorted(distribution):
        print(f"  {key}: {distribution[key]}")


if __name__ == "__main__":
    main()