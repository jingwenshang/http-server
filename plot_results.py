
import sys
import csv
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict

def load_csv(filepath):
    """Load CSV and return {rate: [rps_values]}, {rate: [latency_values]}"""
    rps_data = defaultdict(list)
    lat_data = defaultdict(list)

    with open(filepath) as f:
        reader = csv.DictReader(f)
        for row in reader:
            rate = int(row['rate'])
            rps  = float(row['replies_per_sec']) if row['replies_per_sec'] else 0
            lat  = float(row['avg_latency_ms']) if row['avg_latency_ms'] else 0
            rps_data[rate].append(rps)
            lat_data[rate].append(lat)

    return rps_data, lat_data

def plot(files):
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))

    for filepath in files:
        label = filepath.replace('.csv', '').replace('results_', '')
        rps_data, lat_data = load_csv(filepath)

        rates    = sorted(rps_data.keys())
        rps_mean = [np.mean(rps_data[r]) for r in rates]
        rps_std  = [np.std(rps_data[r])  for r in rates]
        lat_mean = [np.mean(lat_data[r]) for r in rates]
        lat_std  = [np.std(lat_data[r])  for r in rates]

        ax1.errorbar(rates, rps_mean, yerr=rps_std, marker='o',
                     capsize=3, label=label)
        ax2.errorbar(rates, lat_mean, yerr=lat_std, marker='s',
                     capsize=3, label=label)

    
    ax1.set_xlabel('Offered Request Rate (req/s)')
    ax1.set_ylabel('Achieved Throughput (replies/s)')
    ax1.set_title('Throughput vs. Load')
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    
    ax2.set_xlabel('Offered Request Rate (req/s)')
    ax2.set_ylabel('Mean Response Latency (ms)')
    ax2.set_title('Latency vs. Load')
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig('benchmark_results.png', dpi=150)
    print("Saved: benchmark_results.png")
    plt.show()

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python3 plot_results.py results1.csv [results2.csv ...]")
        sys.exit(1)
    plot(sys.argv[1:])
