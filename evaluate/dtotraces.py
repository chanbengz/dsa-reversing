import matplotlib.pyplot as plt
import numpy as np

app_name = "atc" # atc/swq
latency_data = np.fromfile(f"data/{app_name}-ssh-traces.txt", dtype=int, sep="\n")

# Filter out latency values higher than 6000
mask = (latency_data <= 2000) & (latency_data >= 200)
latency_data = latency_data[mask]

plt.rcParams['font.family'] = 'Optima'
plt.figure(figsize=(8, 4))
slot_numbers = np.arange(len(latency_data))

plt.plot(slot_numbers, latency_data, linewidth=0.8, alpha=0.7, label='Traces')
# plt.plot(slot_numbers, latency_data, color='#34A853', linewidth=1.5, alpha=1, label='Traces')

plt.axhline(y=869, color='red', linestyle='--', linewidth=1, label='Threshold')

plt.xlabel('Time Slot', fontsize=16)

# plt.ylabel('Contention', fontsize=16)
plt.ylabel('Latency (Cycles)', fontsize=16)
# plt.yticks(range(0, 2, 1))

plt.tick_params(axis='both', which='major', labelsize=13)
plt.legend()
ax = plt.gca()
ax.spines['top'].set_color('black')
ax.spines['right'].set_color('black')
ax.spines['left'].set_color('black')
ax.spines['bottom'].set_color('black')

plt.savefig(f'data/{app_name}-ssh-traces.pdf', format='pdf', bbox_inches='tight')