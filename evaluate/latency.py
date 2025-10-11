import matplotlib.pyplot as plt
import numpy as np

environ = "cloud" # "native" or "cloud"
noisy = "-noisy"
low_bound = 250
upper_bound = 1500

hit = np.fromfile(f"data/devtlb-hit-{environ}{noisy}.txt", dtype=int, sep="\n")
miss = np.fromfile(f"data/devtlb-miss-{environ}{noisy}.txt", dtype=int, sep="\n")

plt.figure(figsize=(4, 3))
plt.rcParams['font.family'] = 'Optima'
plt.hist(hit, bins=200, alpha=0.6, label='DevTLB Hit', range=(low_bound, upper_bound))
plt.hist(miss, bins=200, alpha=0.6, label='DevTLB Miss', range=(low_bound, upper_bound))

plt.xlabel('Latency (cycles)', fontsize=13)
plt.ylabel('Frequency', fontsize=13)
plt.tick_params(axis='both', which='major', labelsize=13)
plt.legend(fontsize=11, loc="upper left")

plt.savefig(f'data/latency-{environ}{noisy}.pdf', format='pdf', bbox_inches='tight')