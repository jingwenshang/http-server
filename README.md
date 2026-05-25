# High-Performance Thread-Pool HTTP Server

A multi-threaded HTTP/1.0 and HTTP/1.1 web server implemented in ~600 lines of C, built for the ECE670 Operating Systems course at UMass Amherst.

## Architecture

The server uses a **producer-consumer** pattern with three components:

- **Master Thread** — runs the `accept()` loop, pushes client file descriptors into a task queue
- **Synchronized Task Queue** — bounded circular buffer (capacity 1024) protected by mutex + condition variables, provides backpressure under overload
- **Worker Thread Pool** — 64 pre-allocated threads (configurable) that dequeue connections and handle HTTP requests using blocking I/O

```
Clients → [Master Thread] → [Task Queue] → [Worker 1..N] → Filesystem
                                ↓
                        backpressure when full
```

## Features

- HTTP/1.0 and HTTP/1.1 with persistent connections
- Dynamic keep-alive timeout: `T = T_base / (1 + active_conns / threshold)`
- Status codes: 200, 400, 403, 404
- MIME types: HTML, TXT, CSS, JS, JPEG, GIF, PNG, ICO
- Streaming file transfer in 8KB chunks
- Path traversal protection
- Graceful shutdown on SIGINT/SIGTERM

## Build & Run

```bash
make
./server -port 8888 -document_root ./docroot
```

Optional flags:
```bash
./server -port 8888 -document_root /path/to/files -workers 128
```

## Benchmark

Includes a concurrent Python benchmark client:

```bash
python3 my_benchmark.py 8888 /index.html localhost
```

### Results (CloudLab bare-metal, RS620 nodes)

Benchmarked against Apache 2.4 (prefork MPM) on dedicated client/server machines:

| Metric | Our Server | Apache |
|--------|-----------|--------|
| Peak throughput | ~4,000 rps | ~4,200 rps |
| Mean latency (saturated) | 1.3 ms | 1.6 ms |
| P99 latency (saturated) | 3.0 ms | 3.4 ms |
| Errors at 10x overload | 0 | 0 |

**Key finding:** achieves throughput within 5% of Apache with graceful degradation under overload — zero errors even at 50,000 req/s offered load.

## Project Structure

```
├── include/server.h      # Structures, constants, function declarations
├── src/main.c             # Entry point, socket setup, accept loop
├── src/threadpool.c       # Task queue + worker thread management
├── src/http.c             # HTTP parsing, file serving, response generation
├── Makefile               # Build configuration
├── my_benchmark.py        # Concurrent benchmark client
└── plot_results.py        # Matplotlib visualization for benchmark data
```

## References

- Welsh et al., *SEDA: An Architecture for Well-Conditioned, Scalable Internet Services* (SOSP 2001)
- von Behren et al., *Capriccio: Scalable Threads for Internet Services* (SOSP 2003)
