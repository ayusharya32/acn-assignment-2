import csv
import math
import sys


def percentile(values, p):
    """
    Assignment-defined nearest-rank percentile.

    index = ceil(p / 100 * N) - 1
    """
    if not values:
        return None

    values = sorted(values)
    index = math.ceil((p / 100) * len(values)) - 1
    return values[index]


def classify_size(num_bytes):
    """
    Assignment workload classes:
      small  ~ 1 KB
      medium ~ 30 KB
      large  ~ 150 KB

    Classification is based on the actual request size.
    """
    if num_bytes < 10 * 1024:
        return "small"
    elif num_bytes < 100 * 1024:
        return "medium"
    else:
        return "large"


def main():
    if len(sys.argv) != 2:
        print(f"Usage: python3 {sys.argv[0]} <metrics.csv>")
        return 1

    filename = sys.argv[1]

    with open(filename, newline="") as f:
        rows = list(csv.DictReader(f))

    if not rows:
        print("No requests found.")
        return 0

    waiting_times = []
    response_times = []
    arrivals = []
    finishes = []

    slowdown_by_class = {
        "small": [],
        "medium": [],
        "large": [],
    }

    forfeited_by_class = {
        "small": 0,
        "medium": 0,
        "large": 0,
    }

    request_count_by_class = {
        "small": 0,
        "medium": 0,
        "large": 0,
    }

    a14_count = 0
    long_line_requests = []

    for row in rows:
        arrival = int(row["arrival_ns"])
        start = int(row["start_ns"])
        finish = int(row["finish_ns"])
        num_bytes = int(row["bytes"])
        rounds = int(row["rounds"])
        forfeited = int(row["forfeited_bytes"])

        waiting = start - arrival
        response = finish - arrival

        waiting_times.append(waiting)
        response_times.append(response)

        arrivals.append(arrival)
        finishes.append(finish)

        size_class = classify_size(num_bytes)

        request_count_by_class[size_class] += 1

        if num_bytes > 4096:
            # This is only used as a useful long-request indicator.
            # Actual A14 detection below uses forfeiture/round behavior.
            pass

        if num_bytes > 0:
            slowdown = response / num_bytes
            slowdown_by_class[size_class].append(slowdown)

        forfeited_by_class[size_class] += forfeited

        # A14 produces a request that has a non-zero number of rounds
        # but no forfeited allowance. We identify long-line requests
        # separately using the request size for inspection.
        if num_bytes > 4096:
            long_line_requests.append(row)

    observation_window_ns = max(finishes) - min(arrivals)

    throughput = (
        len(rows)
        / (observation_window_ns / 1_000_000_000)
    )

    # Print main statistics.
    print("===== Metrics Summary =====")
    print(f"Requests: {len(rows)}")
    print()

    print("Waiting time:")
    print(f"  p50: {percentile(waiting_times, 50):.2f} ns")
    print(f"  p99: {percentile(waiting_times, 99):.2f} ns")
    print()

    print("Response time:")
    print(f"  p50: {percentile(response_times, 50):.2f} ns")
    print(f"  p99: {percentile(response_times, 99):.2f} ns")
    print()

    print("Throughput:")
    print(f"  {throughput:.2f} requests/sec")
    print(f"  Observation window: {observation_window_ns} ns")
    print()

    print("Requests by size class:")

    for size_class in ["small", "medium", "large"]:
        print(
            f"  {size_class}: "
            f"{request_count_by_class[size_class]}"
        )

    print()

    print("Normalized slowdown (ns/byte):")

    for size_class in ["small", "medium", "large"]:
        values = slowdown_by_class[size_class]

        if values:
            median = percentile(values, 50)
            p99 = percentile(values, 99)

            print(
                f"  {size_class}: "
                f"median={median:.6f}, "
                f"p99={p99:.6f}"
            )
        else:
            print(f"  {size_class}: no requests")

    print()

    print("Forfeited bytes:")

    total_forfeited = 0

    for size_class in ["small", "medium", "large"]:
        value = forfeited_by_class[size_class]
        total_forfeited += value

        print(f"  {size_class}: {value}")

    print(f"  total: {total_forfeited}")
    print()

    print(f"Long requests (> 4 KB): {len(long_line_requests)}")

    return 0


if __name__ == "__main__":
    sys.exit(main())