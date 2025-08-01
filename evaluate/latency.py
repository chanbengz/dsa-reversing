import matplotlib.pyplot as plt
import numpy as np

hit = np.fromfile("data/devtlb-hit.txt", dtype=int, sep="\n")
miss = np.fromfile("data/devtlb-miss.txt", dtype=int, sep="\n")

plt.figure(figsize=(4, 3))
plt.rcParams['font.family'] = 'Optima'
plt.hist(hit, bins=200, alpha=0.6, label='DevTLB Hit', range=(250, 1200))
plt.hist(miss, bins=200, alpha=0.6, label='DevTLB Miss', range=(250, 1200))

plt.xlabel('Latency (cycles)', fontsize=13)
plt.ylabel('Frequency', fontsize=13)
plt.tick_params(axis='both', which='major', labelsize=13)
plt.legend(fontsize=11, loc="upper left")

plt.savefig('data/latency.pdf', format='pdf', bbox_inches='tight')