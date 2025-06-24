import matplotlib.pyplot as plt
import numpy as np

x = [2**i for i in range(12, 30)]
y_sub = np.fromfile("data/submission.txt", dtype=int, sep="\n")
y_wait = np.fromfile("data/wait.txt", dtype=int, sep="\n")
y_async = np.fromfile("data/async.txt", dtype=int, sep="\n")

assert len(x) == len(y_sub) == len(y_wait)

plt.rcParams['font.family'] = 'Optima'
plt.figure(figsize=(10, 6))
plt.plot(x, y_sub, color='#4285F4', marker='o', linestyle='-', markersize=4, label='Submission')
plt.plot(x, y_wait, color='#EA4335', marker='s', linestyle='-', markersize=4, label='Completion')
plt.plot(x, y_async, color='#FBBC05', marker='^', linestyle='-', markersize=4, label='Async Submission')

plt.xscale('log', base=2)
plt.yscale('log', base=2)

plt.xlabel('Transfer Size (bytes)', fontsize=16)
plt.ylabel('Time (cycles)', fontsize=16)
plt.legend()

ax = plt.gca()
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)
ax.spines['left'].set_color('black')
ax.spines['bottom'].set_color('black')

plt.tick_params(axis='both', which='major', labelsize=13)

plt.savefig('data/benchmark.pdf', bbox_inches='tight')
plt.close()