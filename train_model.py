import pandas as pd
import numpy as np
import tensorflow as tf
from sklearn.preprocessing import StandardScaler

# Load data
normal = pd.read_csv("normal.csv", header=None)
tilt = pd.read_csv("tilt.csv", header=None)

normal['label'] = 0
tilt['label'] = 1

data = pd.concat([normal, tilt])
data.columns = ['ax', 'ay', 'az', 'gx', 'gy', 'gz', 'label']

X = data[['ax', 'ay', 'az', 'gx', 'gy', 'gz']]
y = data['label']

# Scale features
scaler = StandardScaler()
X_scaled = scaler.fit_transform(X)

# Build small neural network
model = tf.keras.Sequential([
    tf.keras.layers.Dense(8, activation='relu', input_shape=(6,)),
    tf.keras.layers.Dense(4, activation='relu'),
    tf.keras.layers.Dense(2, activation='softmax')
])
model.compile(optimizer='adam', loss='sparse_categorical_crossentropy', metrics=['accuracy'])
model.fit(X_scaled, y, epochs=30, batch_size=16, verbose=1)

# Save model as TFLite
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()
with open("model.tflite", "wb") as f:
    f.write(tflite_model)

# Save scaler
print(f"Mean: {scaler.mean_}")
print(f"Std: {scaler.scale_}")
