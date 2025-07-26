import numpy as np
import tensorflow as tf
from tensorflow.keras.models import Model
from tensorflow.keras.layers import Input, Dense, LSTM, Bidirectional, Layer, Dropout
from sklearn.model_selection import train_test_split
from tensorflow.keras.callbacks import EarlyStopping
from sklearn.preprocessing import LabelEncoder
from sklearn.metrics import accuracy_score
import matplotlib.pyplot as plt
import os


# ----- Parameters -----
sequence_len = 1600
group_size = 250
latency_threshold = 700
modelsUT = [
    'stories15M',
    'stories42M',
    'stories110M',
    'llama2-7b',
    'gemma3:1b',
    'gemma3:4b',
    'qwen3:4b',
    'qwen3:1.7b'
]
samples_per_class = 50
data_dir = "mldata/"

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
    x = Dropout(0.4)(x)
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

# ----- Data Loading -----
X = []
y = []

for model in modelsUT:
    for i in range(1, samples_per_class + 1):
        filename = f"ml-traces_{model}_{i}.txt"
        filepath = os.path.join(data_dir, filename)
        try:
            with open(filepath, 'r') as f:
                latencies = [float(x) for x in f.read().split()]
                trace = preprocess_trace(latencies, group_size=group_size, threshold=latency_threshold)
                if trace is not None:
                    X.append(trace)
                    y.append(model)
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
          epochs=30,
          batch_size=32,
          callbacks=[early_stop])

# ----- Confusion Matrix with Probabilities -----
y_pred_probs = model.predict(X_test)

# Calculate accuracy using argmax of probabilities
y_pred = np.argmax(y_pred_probs, axis=1)
accuracy = accuracy_score(y_test, y_pred)
print(f"Test Accuracy: {accuracy * 100:.2f}%")