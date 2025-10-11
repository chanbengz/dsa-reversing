import matplotlib.pyplot as plt
import numpy as np

environ = "cloud" # native or cloud

x = [2**i for i in range(12, 30)]
y_sub = np.fromfile(f"data/submission-{environ}.txt", dtype=int, sep="\n")
y_wait = np.fromfile(f"data/wait-{environ}.txt", dtype=int, sep="\n")
y_async = np.fromfile(f"data/async-{environ}.txt", dtype=int, sep="\n")

assert len(x) == len(y_sub) == len(y_wait)

plt.rcParams['font.family'] = 'Optima'
plt.figure(figsize=(8, 4))
plt.plot(x, y_sub, color="#0845A8", marker='o', linestyle='-', markersize=5, label='Submission')
plt.plot(x, y_wait, color="#C91100", marker='s', linestyle='-', markersize=5, label='Completion')
plt.plot(x, y_async, color="#18C9C6", marker='^', linestyle='-', markersize=5, label='Async Submission')

plt.xscale('log', base=2)
plt.yscale('log', base=2)

plt.xlabel('Transfer Size (bytes)', fontsize=14)
plt.ylabel('Time (cycles)', fontsize=14)
plt.legend()

ax = plt.gca()
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)
ax.spines['left'].set_color('black')
ax.spines['bottom'].set_color('black')

plt.tick_params(axis='both', which='major', labelsize=12)

plt.savefig(f'data/benchmark-{environ}.pdf', bbox_inches='tight')
plt.close()