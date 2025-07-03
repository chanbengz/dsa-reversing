import numpy as np

dev = "swq"
ground_truth =  np.fromfile(f"data/{dev}-ground-ts.txt", dtype=float, sep="\n")
collected = np.fromfile(f"data/{dev}-ssh-ts.txt", dtype=float, sep="\n")

false_positives = 0
false_negatives = 0
true_positives = 0

diff = np.array([], dtype=float)
for i in range(len(ground_truth)):
    # around 200ms tolerance
    filtered = collected[(collected >= ground_truth[i] - 0.2) & (collected <= ground_truth[i] + 0.2)]
    if len(filtered) > 0:
        diff = np.append(diff, filtered[0] - ground_truth[i])
        if len(filtered) > 1:
            false_positives += len(filtered) - 1
        else:
            true_positives += 1
    else:
        # no match found
        false_negatives += 1

print("Std Dev:", np.std(diff))
print("Ground Truth", len(ground_truth))
print("Collected", len(collected))
print("True Positives", true_positives)
print("False Positives", false_positives)
print("False Negatives", false_negatives)
recall = true_positives / (true_positives + false_negatives)
print("Recall:", recall)
precision = true_positives / (true_positives + false_positives)
print("Precision:", precision)
print("F1 Score:", 2 * (precision * recall) / (precision + recall))