import matplotlib.pyplot as plt
import numpy as np

# models = [
#     'stories15M',
#     'stories42M',
#     'stories110M'
# ]
models = [
    'gemma3:4b',
    'qwen3:4b'
]

threshold = 700
traces_per_bucket = 800
total_traces = 400000
num_buckets = total_traces // traces_per_bucket

# Colors for different models
colors = ['#1f77b4', '#ff7f0e', '#2ca02c']
# labels = ['TinyStories 15M', 'TinyStories 42M', 'TinyStories 110M']
labels = ['Gemma 3', 'Qwen 3']

plt.rcParams['font.family'] = 'Optima'
plt.figure(figsize=(4, 3))

# Process each model
for i, model in enumerate(models):
    latency_data = np.fromfile(f"mldata/ml-traces_{model}_3.txt", dtype=int, sep="\n")
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
    plt.plot(bucket_numbers, bucket_counts, linewidth=1, alpha=0.8, color=colors[i], label=labels[i])

plt.xlabel('Time Slot', fontsize=16)
plt.ylabel('DevTLB Miss', fontsize=16)
plt.tick_params(axis='both', which='major', labelsize=13)
plt.legend(fontsize=12, loc='upper right')
plt.grid(True, linestyle='--', linewidth=0.5, alpha=0.7)
plt.margins(x=0, y=0)

ax = plt.gca()
ax.spines['top'].set_color('black')
ax.spines['right'].set_color('black')
ax.spines['left'].set_color('black')
ax.spines['bottom'].set_color('black')

plt.tight_layout()
plt.savefig(f'data/ml-traces-model.pdf', format='pdf', bbox_inches='tight')