import matplotlib.pyplot as plt
import numpy as np

# Transfer sizes in bytes
transfer_sizes = [256, 1024, 4096, 16384, 65536, 262144, 1048576]
# Throughput data - replace with your actual data
cpu_throughput = [2.10,  3.13,  3.33,  4.27, 4.51, 9.07, 9.89]
dsa_throughput = [4.18, 11.93, 29.88, 31.26, 30.79, 30.64, 30.59]
dsa_mitigated  = [3.55, 11.79, 29.74, 31.21, 30.73, 30.56, 30.55]
dto_mitigated  = [3.43, 11.61, 29.06, 30.77, 30.4, 30.78, 29.61]

plt.rcParams['font.family'] = 'Optima'
plt.figure(figsize=(8, 4))

x_labels = []
for size in transfer_sizes:
    if size < 1024:
        x_labels.append(f'{size}B')
    elif size < 1048576:
        x_labels.append(f'{size//1024}KB')
    else:
        x_labels.append(f'{size//1048576}MB')

x = np.arange(len(transfer_sizes))
width = 0.15

bars1 = plt.bar(x - 1.5*width, cpu_throughput, width, label='CPU', color='#1B2951', alpha=0.8)
bars2 = plt.bar(x - 0.5*width, dsa_throughput, width, label='DSA', color='#B48EAD', alpha=0.8)
bars3 = plt.bar(x + 0.5*width, dsa_mitigated, width, label='DSA Mitigated', color='#A2BE8C', alpha=0.8)
bars4 = plt.bar(x + 1.5*width, dto_mitigated, width, label='DTO Mitigated', color='#722F37', alpha=0.8)

plt.xlabel('Transfer Size', fontsize=16)
plt.ylabel('Throughput (GB/s)', fontsize=16)
plt.xticks(x, x_labels)
plt.legend(bbox_to_anchor=(0.5, 1.02), loc='lower center', ncol=4, fontsize=12, frameon=False)
plt.grid(axis='y', alpha=0.3)

ax = plt.gca()
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)

plt.tight_layout()
plt.savefig('data/overhead.pdf', format='pdf', bbox_inches='tight')