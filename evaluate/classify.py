import numpy as np
import tensorflow as tf
from tensorflow.keras.models import Model
from tensorflow.keras.layers import Input, Dense, LSTM, Bidirectional, Layer, Dropout
from sklearn.model_selection import train_test_split
from tensorflow.keras.callbacks import EarlyStopping
from sklearn.preprocessing import LabelEncoder
from sklearn.metrics import confusion_matrix, ConfusionMatrixDisplay, accuracy_score
import matplotlib.pyplot as plt
import os


# ----- Parameters -----
sequence_len = 250
group_size = 400
latency_threshold = 700
websites = [
    'www.weibo.com',
    'www.baidu.com',
    'www.bilibili.com',
    'www.zhihu.com',
    'www.163.com',
    'www.canva.com',
    'www.microsoft.com',
    'www.taobao.com',
    'www.zoom.com',
    'www.apple.com',
    'www.douban.com',
    'www.iqiyi.com',
    'www.aliyun.com',
    'www.alipay.com',
    'www.notion.com',
]
samples_per_class = 200
data_dir = "wfdata/"

# ----- Custom Attention Layer -----
class AttentionLayer(Layer):
    def __init__(self, **kwargs):
        super(AttentionLayer, self).__init__(**kwargs)

    def build(self, input_shape):
        self.W = self.add_weight(name="att_weight", shape=(input_shape[-1], 1),
                                 initializer="random_normal", trainable=True)
        self.b = self.add_weight(name="att_bias", shape=(input_shape[1], 1),
                                 initializer="zeros", trainable=True)
        super(AttentionLayer, self).build(input_shape)

    def call(self, inputs):
        e = tf.keras.backend.tanh(tf.keras.backend.dot(inputs, self.W) + self.b)
        a = tf.keras.backend.softmax(e, axis=1)
        output = inputs * a
        return tf.keras.backend.sum(output, axis=1)

# ----- Model Builder -----
def build_model(input_shape, num_classes):
    inputs = Input(shape=input_shape)
    x = Bidirectional(LSTM(128, return_sequences=True))(inputs)
    x = Dropout(0.35)(x)
    x = Bidirectional(LSTM(128, return_sequences=True))(x)
    x = Dropout(0.3)(x)
    x = AttentionLayer()(x)
    x = Dropout(0.26)(x)
    outputs = Dense(num_classes, activation='softmax')(x)
    model = Model(inputs, outputs)
    return model

# ----- Data Preprocessing Function -----
def preprocess_trace(raw_latencies, group_size=400, threshold=700):
    """Convert raw latency list to cache miss counts per group."""
    raw_latencies = np.array(raw_latencies)
    total_points = group_size * sequence_len
    if len(raw_latencies) < total_points:
        return None  # not enough data
    raw_latencies = raw_latencies[:total_points]
    grouped = raw_latencies.reshape((sequence_len, group_size))
    misses = np.sum(grouped > threshold, axis=1)
    return misses

def clean_website_name(website):
    """Extract clean website name by removing www. prefix and any TLD suffix"""
    name = website
    if name.startswith('www.'):
        name = name[4:]
    if '.' in name:
        name = name.rsplit('.', 1)[0]
    return name

# ----- Data Loading -----
X = []
y = []

for website in websites:
    for i in range(1, samples_per_class + 1):
        filename = f"wf-traces_{website}_{i}.txt"
        filepath = os.path.join(data_dir, filename)
        try:
            with open(filepath, 'r') as f:
                latencies = [float(x) for x in f.read().split()]
                trace = preprocess_trace(latencies, group_size=group_size, threshold=latency_threshold)
                if trace is not None:
                    X.append(trace)
                    y.append(clean_website_name(website))
                else:
                    print(f"Skipping {filename}: not enough data")
        except FileNotFoundError:
            print(f"Missing file: {filename}")

X = np.array(X, dtype=np.float32)
y = np.array(y)
X = X[..., np.newaxis]

# ----- Label Encoding & Split -----
encoder = LabelEncoder()
y_encoded = encoder.fit_transform(y)

X_train, X_test, y_train, y_test = train_test_split(X, y_encoded, test_size=0.2, stratify=y_encoded, random_state=42)

# ----- Train Model -----
model = build_model(input_shape=(sequence_len, 1), num_classes=len(encoder.classes_))
model.compile(optimizer=tf.keras.optimizers.Adam(learning_rate=1e-3),
              loss='sparse_categorical_crossentropy',
              metrics=['accuracy'])

model.summary()

early_stop = EarlyStopping(monitor='val_accuracy', patience=10, restore_best_weights=True, verbose=1)

model.fit(X_train, y_train,
          validation_data=(X_test, y_test),
          epochs=50,
          batch_size=128,
          callbacks=[early_stop])

# ----- Confusion Matrix with Probabilities -----
y_pred_probs = model.predict(X_test)

# Create a probability-based confusion matrix
# For each true class, average the predicted probabilities
num_classes = len(encoder.classes_)
cm_probs = np.zeros((num_classes, num_classes))

for true_class in range(num_classes):
    mask = (y_test == true_class)
    if np.sum(mask) > 0:
        cm_probs[true_class, :] = np.mean(y_pred_probs[mask], axis=0)

# Convert probabilities to percentages
cm_probs_percent = cm_probs * 100

disp = ConfusionMatrixDisplay(confusion_matrix=cm_probs_percent, display_labels=encoder.classes_)

plt.rcParams['font.family'] = 'Optima'
fig, ax = plt.subplots(figsize=(12, 10))
disp.plot(cmap='Blues', ax=ax, values_format=".1f")
ax.set_xticklabels(ax.get_xticklabels(), fontsize=14, rotation=45, ha='right')
ax.set_yticklabels(ax.get_yticklabels(), fontsize=14)
ax.set_xlabel("Predicted Label", fontsize=14)
ax.set_ylabel("True Label", fontsize=14)
plt.grid(False)
plt.savefig('confusion-matrix.pdf', format='pdf', bbox_inches='tight')

# Calculate accuracy using argmax of probabilities
y_pred = np.argmax(y_pred_probs, axis=1)
accuracy = accuracy_score(y_test, y_pred)
print(f"Test Accuracy: {accuracy * 100:.2f}%")