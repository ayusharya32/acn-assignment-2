# Multithreaded TCP File Server with Link Scheduling (COL724/7524 Assignment 2 - Part A)

A high-performance, multithreaded TCP file transfer server and benchmarking client in C++17 with explicit request scheduling (`FCFS`, `SJF`, `Round Robin`, `Deficit Round Robin`), nanosecond-precision metrics logging, and line-level packetization.

---

## 1. Build Instructions

### Prerequisites
* Linux / macOS environment
* `g++` with C++17 support (`-std=c++17`) and POSIX Threads (`-pthread`)
* `nlohmann/json` (vendored header or installed system package)
* `python3` (for workload generation, experiments, and plotting)

### Compilation
To compile both the server (`build/server`) and client (`build/client`):
```bash
make clean && make
```

To run the test suite:
```bash
make test
```

---

## 2. Configuration File (`config.json`)

Both the server and client read configuration parameters from `config.json`:

```json
{
  "server": {
    "ip": "127.0.0.1",
    "port": 9000,
    "server_threads": 4,
    "client_threads": 8
  },
  "load_balancer": {
    "ip": "127.0.0.1",
    "port": 8000,
    "health_interval_ms": 1000,
    "backends": [
      { "ip": "127.0.0.1", "port": 9001 },
      { "ip": "127.0.0.1", "port": 9002 },
      { "ip": "127.0.0.1", "port": 9003 },
      { "ip": "127.0.0.1", "port": 9004 }
    ]
  }
}
```

### Config Field Definitions:
* `server.ip`: IP address the server listens on (and where the client connects).
* `server.port`: TCP port for the server.
* `server.server_threads`: Number of concurrent worker threads in the server pool.
* `server.client_threads`: Number of concurrent client worker threads used during `load` benchmark runs.
* `load_balancer.ip`: IP address for the Part B load balancer.
* `load_balancer.port`: Listening port for the Part B load balancer.
* `load_balancer.health_interval_ms`: Frequency (in ms) of health check probes sent to backends.
* `load_balancer.backends`: Array of exactly 4 backend server targets (`ip` and `port`).

---

## 3. Command-Line Usage

### Server Invocation
```bash
./build/server --config <config-path> --sched <policy> --file <directory> --metrics-out <csv-path> [--quantum <Q>] [--p <N>]
```

#### Server Flags:
* `--sched <policy>` (**Required**): Scheduling policy. Must be one of `fcfs`, `sjf`, `rr`, `drr`.
* `--file <directory>` (**Required**): Directory of files the server serves (`GET`) and stores uploads into (`PUT`).
* `--quantum <Q>` (**Required for `rr` and `drr`, rejected for `fcfs` and `sjf`**): Service quantum allowance in bytes per round.
* `--config <path>` (Optional, default `config.json`): Path to configuration file.
* `--metrics-out <path>` (Optional, default `metrics.csv`): Path to write the per-request CSV upon graceful shutdown.
* `--p <N>` (Optional, default `1`): Packetization factor: gathers up to $N$ whole lines into a single socket write syscall.

---

### Client Invocation
```bash
./build/client <operation> <target> [--config <config-path>] [--requests <N>]
```

#### Client Operations:
1. **Upload a file (`PUT`)**:
   ```bash
   ./build/client put <local-file-path> [--config config.json]
   ```
2. **Download a file (`GET`)**:
   ```bash
   ./build/client get <server-file-name> [--config config.json]
   ```
3. **Run Workload Benchmark (`LOAD`)**:
   ```bash
   ./build/client load <workload-dir> --requests <N> [--config config.json]
   ```
   * Seeds the server by `PUT`ting every file in `<workload-dir>` once (seeding is uncounted in metrics).
   * Spawns `client_threads` concurrent threads issuing $N$ requests total from a shared atomic counter.
   * 50/50 uniform GET/PUT mix, operating closed-loop (each thread waits for full completion before issuing the next).
   * Silent on success (exits `0` with no stdout clutter).

---

## 4. Key Design Choices & Architecture

### A. Explicit Request Queue & Admission (Rule A5)
* Incoming client connections are accepted by the main listener thread, the header line is read with a socket receive timeout (to prevent slow client DoS attacks), and the request size is validated before enqueueing.
* Requests are queued in an explicit, thread-safe queue (`schedulerQueue`).
* Worker threads pop requests from the queue in the order mandated by the active scheduling policy.

### B. Immediate Health Checks (Rule A5)
* `HEALTH\n` probe requests bypass the scheduler queue and are answered immediately out-of-band with `OK <queue_depth>\n`. Health probes are never recorded in `metrics.csv`.

### C. GET Scheduling & Line Packetization (Rules A6, A8)
* On the `GET` path, scheduling granularity is **one whole line** (up to and including `\n`).
* A round never terminates partway through a line.
* `--p N` batches up to $N$ complete lines in memory before calling `send()`, and flushes any partial batch upon round termination or EOF.

### D. Preemption State Preservation (Rule A17)
* For `RR` and `DRR`, preempted requests are requeued at the tail of the scheduling queue.
* All state is preserved:
  * Byte file offset (`bytesTransferred`)
  * Excess unconsumed socket buffer data (`extra`)
  * DRR Deficit counter (`deficit`)

### E. DRR vs RR Deficit Accounting (Rules A13, A14, A15, A16)
* **Round Robin (`rr`)**: If the next line exceeds the remaining quantum allowance, the round ends and the unused bytes are **forfeited**. If the very first line of a round is larger than $Q$, Rule A14 fires: the line is sent in full to avoid starvation.
* **Deficit Round Robin (`drr`)**: Unused allowance accumulates in the request's `deficit` counter (`deficit += Q`). Lines are only sent when `deficit >= lineBytes`. Rule A14 is omitted as deficit naturally grows until the line fits.

### F. Monotonic Instrumentation & Metrics (Rules A18 - A23)
* Uses `CLOCK_MONOTONIC` to record nanosecond timestamps:
  * `arrival_ns`: Timestamp when request header is parsed and admitted to queue.
  * `start_ns`: Timestamp when a worker thread first picks up the request.
  * `finish_ns`: Timestamp when the final byte is transferred.
* On `SIGINT`/`SIGTERM`, the server drains active requests, flushes `metrics.csv`, and prints a shutdown summary.

---

## 5. Reproducing Experiments (Rules A28 & A29)

### Automated 6-Cell Experiment Runner
To reproduce all 6 experiment configurations automatically:
```bash
# 1. Run full 6-cell benchmark suite (1,000 requests each)
python3 tests/run_experiments.py
```

This script:
1. Generates the 4 workload files in `./workload/` (`small.txt`, `medium.txt`, `large.txt`, `longline.txt`).
2. Runs Cells 1 to 6:
   * **Cell 1**: `fcfs` (4 server threads, 8 client threads)
   * **Cell 2**: `sjf` (4 server threads, 8 client threads)
   * **Cell 3**: `rr` (4 server threads, 8 client threads, $Q=4096$)
   * **Cell 4**: `drr` (4 server threads, 8 client threads, $Q=4096$)
   * **Cell 5**: `fcfs` (1 server thread, 8 client threads)
   * **Cell 6**: `rr` (1 server thread, 8 client threads, $Q=4096$)
3. Collects all CSVs into `experiment_results/metrics_cell_*.csv`.
4. Computes nearest-rank percentiles ($p_{50}, p_{99}$), throughput, size-class normalized slowdowns, forfeited bytes, A14 overrun counts, and long-line slowdowns.

### Generating Report Plots
To generate all 5 publication-ready PNG charts:
```bash
# Install matplotlib if needed
sudo apt install -y python3-matplotlib   # or: pip3 install matplotlib

# Generate all charts
python3 tests/generate_plots.py
```
Generated charts in `./plots/`:
* `waiting_time_comparison.png`
* `throughput_comparison.png`
* `slowdown_comparison.png`
* `forfeited_bytes_comparison.png`
* `longline_slowdown_comparison.png`

---

## 6. Project Structure
```text
.
├── Makefile                     # Build targets for server and client
├── README.md                    # Project documentation & reproduction guide
├── config.json                  # Reference configuration file
├── include/                     # C++ header files
│   ├── cli.hpp
│   ├── config.hpp
│   ├── framing.hpp
│   ├── request.hpp
│   ├── request_handler.hpp
│   ├── response.hpp
│   ├── scheduler.hpp
│   └── util.hpp
├── src/                         # C++ implementation files
│   ├── cli.cpp
│   ├── client.cpp
│   ├── config.cpp
│   ├── framing.cpp
│   ├── request.cpp
│   ├── request_handler.cpp
│   ├── response.cpp
│   ├── scheduler.cpp
│   ├── server.cpp
│   └── util.cpp
├── tests/                       # Unit tests & experiment scripts
│   ├── analyze_metrics.py
│   ├── generate_plots.py
│   ├── generate_workload.py
│   ├── run_experiments.py
│   ├── test_cli.cpp
│   ├── test_framing.cpp
│   ├── test_parser.cpp
│   └── test_response.cpp
├── workload/                    # Generated workload text files
├── experiment_results/          # Exported metrics CSVs and summary JSON
└── plots/                       # Generated comparison charts
```
