import matplotlib.pyplot as plt
import numpy as np

hit = np.fromfile("data/devtlb-hit.txt", dtype=int, sep="\n")
miss = np.fromfile("data/devtlb-miss.txt", dtype=int, sep="\n")

plt.hist(hit, bins=80, alpha=0.5, label='base', range=(200, 1200))
plt.hist(miss, bins=80, alpha=0.5, label='base+offset', range=(200, 1200))

plt.xlabel('Latency (Cycles)', fontsize=16)
plt.ylabel('Frequency', fontsize=16)
plt.tick_params(axis='both', which='major', labelsize=13)
plt.legend()

plt.savefig('data/latency.pdf', format='pdf', bbox_inches='tight')