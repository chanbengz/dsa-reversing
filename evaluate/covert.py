import matplotlib.pyplot as plt
import numpy as np
import math

app = "swq" # atc/swq
NUM_TRACES = 32
num_bits = 16 * NUM_TRACES * 8
true_cap = lambda c, e: c * (1 + (1 - e) * math.log2(1 - e) + e * math.log2(e))

# DevTLB
times = [0.21729, 0.56033, 0.4827, 0.40223, 0.57842, 0.45033, 0.6418, 0.818, 0.30257, 0.34938, 0.1776125, 0.162]
errors = [0.0463, 0.0378,  0.0718, 0.0805,  0.0408,  0.0350,  0.1342, 0.0710, 0.1913,  0.0678,  0.3570,  0.4157]

# SWQ
# times = []
# errors = []

sorted_pairs = sorted(zip(times, errors)) # sort by time => by raw capacity
times, errors = zip(*sorted_pairs)
capc = [num_bits / t / 1000 for t in times]
true_capc = [true_cap(c, e) for c, e in zip(capc, errors)]

def eval_error():
    std = open("data/msg", "rb").read()
    bits_err = 0
    for i in range(1, NUM_TRACES + 1):
        recv = open(f"data/recv_{i}", "rb").read()
        bits_err += sum(bin(x ^ y).count('1') for x, y in zip(std, recv))
    return bits_err / num_bits

def draw_graph():
    plt.rcParams['font.family'] = 'Optima'
    fig, ax1 = plt.subplots(figsize=(10, 5))
    ax2 = ax1.twinx()

    ax1.tick_params(axis='x', labelsize=13)
    ax1.set_xlabel('Raw Capcity (kbps)', fontsize=16, color='black')
    
    ax1.plot(capc, true_capc, marker=None, linestyle='-', color='#4285F4', markersize=4, label='True Capacity (kbps)')
    ax1.set_ylabel('True Capacity (kbps)', fontsize=16, color='black')
    ax1.tick_params(axis='y', labelcolor='#4285F4', labelsize=13)
    
    ax2.plot(capc, errors, marker=None, linestyle='--', color='#EA4335', markersize=4, label='Error Rate')
    ax2.set_ylabel('Error Rate', fontsize=16, color='black')
    ax2.tick_params(axis='y', labelcolor='#EA4335', labelsize=13)
    
    ax1.spines['bottom'].set_color('black')
    
    # Add legends
    # lines1, labels1 = ax1.get_legend_handles_labels()
    # lines2, labels2 = ax2.get_legend_handles_labels()
    # ax1.legend(lines1 + lines2, labels1 + labels2, loc='best')
    
    plt.grid(False)
    plt.savefig(f'data/cc-{app}.pdf', bbox_inches='tight')
    plt.close()

draw_graph()
print(f"Highest: {sorted(true_capc)[-1]:.2f}")
# print(f"Error bits: {eval_error()}")