#!/usr/bin/env python3
import time
import socket
import threading
import sys

HOST = sys.argv[3] if len(sys.argv) > 3 else '127.0.0.1'
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8888
URI  = sys.argv[2] if len(sys.argv) > 2 else '/index.html'

RATES      = [50, 100, 200, 500, 1000, 2000]
NUM_CONNS  = 1000
TRIALS     = 5
OUTFILE    = f"results_{HOST}_{PORT}.csv"


def send_request(results, index):
    try:
        start = time.time()
        with socket.create_connection((HOST, PORT), timeout=5) as s:
            request = (
                f"GET {URI} HTTP/1.1\r\n"
                f"Host: {HOST}\r\n"
                f"Connection: close\r\n"
                f"\r\n"
            )
            s.sendall(request.encode())
            chunks = []
            while True:
                chunk = s.recv(8192)
                if not chunk:
                    break
                chunks.append(chunk)
        elapsed = time.time() - start
        results[index] = ('ok', elapsed * 1000.0)
    except Exception:
        results[index] = ('err', 0.0)


def run_trial(rate, num_conns):
    interval = 1.0 / rate if rate > 0 else 0
    results = [None] * num_conns
    threads = []
    start_time = time.time()

    for i in range(num_conns):
        t = threading.Thread(target=send_request, args=(results, i))
        t.start()
        threads.append(t)
        if interval > 0:
            next_launch = start_time + (i + 1) * interval
            sleep_time = next_launch - time.time()
            if sleep_time > 0:
                time.sleep(sleep_time)

    for t in threads:
        t.join(timeout=10)

    duration = time.time() - start_time
    latencies = []
    errors = 0

    for r in results:
        if r is None or r[0] == 'err':
            errors += 1
        else:
            latencies.append(r[1])

    successes = len(latencies)
    rps = successes / duration if duration > 0 else 0

    if latencies:
        latencies.sort()
        avg_lat = sum(latencies) / len(latencies)
        p99_idx = int(len(latencies) * 0.99)
        p99_lat = latencies[min(p99_idx, len(latencies) - 1)]
    else:
        avg_lat = 0
        p99_lat = 0

    return rps, avg_lat, p99_lat, errors


def main():
    print(f"Benchmark: {HOST}:{PORT}{URI}")
    print(f"Rates: {RATES}, Conns/trial: {NUM_CONNS}, Trials: {TRIALS}")
    print(f"Output: {OUTFILE}")
    print("-" * 60)

    with open(OUTFILE, 'w') as f:
        f.write("rate,trial,replies_per_sec,avg_latency_ms,p99_latency_ms,errors\n")

        for rate in RATES:
            print(f"\n=== Target Rate: {rate} req/s ===")
            for trial in range(1, TRIALS + 1):
                rps, avg_lat, p99_lat, errors = run_trial(rate, NUM_CONNS)
                line = f"{rate},{trial},{rps:.2f},{avg_lat:.2f},{p99_lat:.2f},{errors}"
                print(f"  Trial {trial}: {rps:.1f} rps | "
                      f"avg {avg_lat:.1f}ms | p99 {p99_lat:.1f}ms | "
                      f"{errors} errors")
                f.write(line + "\n")
                f.flush()
                time.sleep(1)

    print(f"\nDone! Results in {OUTFILE}")


if __name__ == '__main__':
    main()
