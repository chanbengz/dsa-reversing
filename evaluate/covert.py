import math
import argparse
import scipy.stats as stats
import numpy as np


parser = argparse.ArgumentParser(description='DSA Covert Channel Analysis')
parser.add_argument('-a', choices=['atc', 'swq'], required=True,
    help='attack target: atc (Address Translation Cache) or swq (Shared Work Queue)')
parser.add_argument('-f', choices=['draw', 'eval'], required=True,
                    help='analysis: draw_graph or eval_error')
args =  parser.parse_args()

app = args.a
NUM_TRACES = 50
NUM_BITS_PER_SAMPLE = 16 * 8
num_bits = NUM_TRACES * NUM_BITS_PER_SAMPLE
true_cap = lambda c, e: c * (1 + (1 - e) * math.log2(1 - e) + e * math.log2(e))

def eval_error():
    std = open("ccdata/msg", "rb").read()
    bits_err = 0
    for i in range(1, NUM_TRACES + 1):
        recv = open(f"ccdata/recv_{i}", "rb").read()
        bits_err += sum(bin(x ^ y).count('1') for x, y in zip(std, recv))
    return bits_err / num_bits

def eval_confidence_interval(times: list):
    std = open("ccdata/msg", "rb").read()
    true_capacities = []
    avg_time = sum(times) / len(times)
    
    for i in range(1, NUM_TRACES + 1):
        recv = open(f"ccdata/recv_{i}", "rb").read()
        bits_err = sum(bin(x ^ y).count('1') for x, y in zip(std, recv))
        sample_error_rate = bits_err / NUM_BITS_PER_SAMPLE
        raw_capacity = NUM_BITS_PER_SAMPLE / avg_time
        if sample_error_rate == 0:
            sample_true_capacity = raw_capacity
        else:
            sample_true_capacity = true_cap(raw_capacity, sample_error_rate) 
        true_capacities.append(sample_true_capacity)
    
    true_capacities = np.array(true_capacities)
    
    mean_capacity = np.mean(true_capacities)
    std_capacity = np.std(true_capacities, ddof=1)
    n = len(true_capacities)
    confidence_level = 0.95
    alpha = 1 - confidence_level
    t_critical = stats.t.ppf(1 - alpha / 2, df=n-1)
    margin_error = t_critical * (std_capacity / np.sqrt(n))
    ci_lower = mean_capacity - margin_error
    ci_upper = mean_capacity + margin_error
    
    return {
        'mean': mean_capacity,
        'std': std_capacity,
        'ci_lower': ci_lower,
        'ci_upper': ci_upper,
        'margin_error': margin_error,
        'avg_time_used': avg_time
    }

def draw_graph(times, errors):
    sorted_pairs = sorted(zip(times, errors)) # sort by time => by raw capacity
    times, errors = zip(*sorted_pairs)
    capc = [num_bits / t / 1000 for t in times]
    true_capc = [true_cap(c, e) for c, e in zip(capc, errors)]

    import matplotlib.pyplot as plt

    plt.rcParams['font.family'] = 'Optima'
    fig, ax1 = plt.subplots(figsize=(5, 4))
    ax2 = ax1.twinx()

    ax1.tick_params(axis='x', labelsize=16)
    ax1.set_xlabel('Raw Capacity (kbps)', fontsize=20, color='black')

    ax1.plot(capc, true_capc, marker=None, linestyle='-', color='#4285F4', linewidth=3, label='True Capacity (kbps)')
    ax1.set_ylabel('True Capacity (kbps)', fontsize=20, color='black')
    ax1.tick_params(axis='y', labelcolor="#1765E2", labelsize=16)

    ax2.plot(capc, errors, marker=None, linestyle='--', color='#EA4335', linewidth=3, label='Error Rate')
    ax2.set_ylabel('Error Rate', fontsize=20, color='black')
    ax2.tick_params(axis='y', labelcolor='#EA4335', labelsize=16)
    
    ax1.spines['bottom'].set_color('black')
    
    plt.grid(False)
    plt.savefig(f'data/cc-{app}.pdf', bbox_inches='tight')
    plt.close()
    print(f"Graph saved as cc-{app}.pdf")
    print(f"Highest true capacity: {sorted(true_capc)[-1]:.2f} kbps")

if args.f == 'draw':
    # DevTLB
    times, errors = [], []
    if app == "atc":
        times  = [0.21729, 0.56033, 0.4827, 0.40223, 0.57842, 0.45033, 0.6418, 0.8180, 0.30257, 0.34938, 0.1776125, 0.1620]
        errors = [0.04630, 0.03780, 0.0718, 0.08050, 0.04080, 0.03500, 0.1342, 0.0710, 0.19130, 0.06780, 0.3570000, 0.4157]
    else:
    # SWQ
        times  = [0.2682, 0.2055, 0.1615, 0.1080, 0.0795, 0.0544, 0.0440, 0.0285, 0.0193, 0.0139, 0.0086, 0.0062]
        errors = [0.1201, 0.1105, 0.1293, 0.1340, 0.1590, 0.1450, 0.1508, 0.1513, 0.1314, 0.1311, 0.2680, 0.2586]
        times  = [t * 50 for t in times]
    draw_graph(times, errors)
elif args.f == 'eval':
    error_rate = eval_error()
    print(f"Overall error rate: {error_rate:.4f}")
    times_t = []
    results = eval_confidence_interval(times_t)
    print(f"Mean true capacity: {results['mean']:.2f} bps")
    print(f"Standard deviation: {results['std']:.2f} bps")
    print(f"Margin of error: {results['margin_error']:.2f} bps")
    print(f"95% Confidence Interval: [{results['ci_lower']:.2f}, {results['ci_upper']:.2f}] bps")