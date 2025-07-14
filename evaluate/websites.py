import matplotlib.pyplot as plt
import numpy as np

websites = [
    'www.baidu.com',
    'www.bilibili.com',
    'example.com',
]

threshold = 700
traces_per_bucket = 400
total_traces = 40000
num_buckets = total_traces // traces_per_bucket

# Colors for different websites
colors = ['#1f77b4', '#ff7f0e', '#2ca02c']  # Blue, Orange, Green
website_labels = ['baidu.com', 'bilibili.com', 'example.com']

plt.rcParams['font.family'] = 'Optima'
plt.figure(figsize=(8, 3))

# Process each website
for i, website in enumerate(websites):
    latency_data = np.fromfile(f"data/wf-traces_{website}.txt", dtype=int, sep="\n")
    mask = (latency_data <= 3000) & (latency_data >= 200)
    latency_data = latency_data[mask]
    
    # Group into buckets and count traces over threshold
    bucket_counts = []
    for bucket_idx in range(num_buckets):
        start_idx = bucket_idx * traces_per_bucket
        end_idx = start_idx + traces_per_bucket
        bucket_data = latency_data[start_idx:end_idx]
        
        # Count traces over threshold in this bucket
        over_threshold = np.sum(bucket_data > threshold)
        bucket_counts.append(over_threshold)
    
    # Plot the results
    bucket_numbers = np.arange(num_buckets)
    plt.plot(bucket_numbers, bucket_counts, linewidth=1, alpha=0.8, color=colors[i], label=website_labels[i])

plt.xlabel('Time Slot', fontsize=16)
plt.ylabel('DevTLB Miss', fontsize=16)
plt.tick_params(axis='both', which='major', labelsize=13)
plt.legend(fontsize=12)
plt.grid(True, linestyle='--', linewidth=0.5, alpha=0.7)
plt.margins(x=0, y=0)

ax = plt.gca()
ax.spines['top'].set_color('black')
ax.spines['right'].set_color('black')
ax.spines['left'].set_color('black')
ax.spines['bottom'].set_color('black')

plt.tight_layout()
plt.savefig(f'data/wf-traces.pdf', format='pdf', bbox_inches='tight')