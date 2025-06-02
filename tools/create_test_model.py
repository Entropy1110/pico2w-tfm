#!/usr/bin/env python3
"""
Simple TensorFlow Lite model generator for testing encryption.

This script creates a simple neural network model and converts it to TensorFlow Lite format
for testing the model encryption functionality.
"""

import tensorflow as tf
import numpy as np
import os

def create_simple_model():
    """Create a simple neural network model for testing."""
    
    # Create a simple sequential model
    model = tf.keras.Sequential([
        tf.keras.layers.Dense(10, activation='relu', input_shape=(4,)),
        tf.keras.layers.Dense(8, activation='relu'),
        tf.keras.layers.Dense(3, activation='softmax')
    ])
    
    # Compile the model
    model.compile(optimizer='adam',
                  loss='sparse_categorical_crossentropy',
                  metrics=['accuracy'])
    
    # Generate some dummy training data
    X_train = np.random.random((100, 4))
    y_train = np.random.randint(0, 3, (100,))
    
    # Train briefly (just to have trained weights)
    model.fit(X_train, y_train, epochs=5, verbose=0)
    
    return model

def convert_to_tflite(model, output_path):
    """Convert Keras model to TensorFlow Lite format."""
    
    # Convert to TensorFlow Lite
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    
    # Optional optimizations
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    
    # Convert
    tflite_model = converter.convert()
    
    # Save to file
    with open(output_path, 'wb') as f:
        f.write(tflite_model)
    
    print(f"TensorFlow Lite model saved: {output_path}")
    print(f"Model size: {len(tflite_model)} bytes")
    
    return tflite_model

def main():
    # Create models directory if it doesn't exist
    models_dir = "models"
    if not os.path.exists(models_dir):
        os.makedirs(models_dir)
    
    print("Creating simple neural network model...")
    model = create_simple_model()
    
    # Convert to TensorFlow Lite
    tflite_path = os.path.join(models_dir, "simple_model.tflite")
    convert_to_tflite(model, tflite_path)
    
    print(f"\nTest model created: {tflite_path}")
    print("You can now test the encryption with:")
    print(f"python tools/model_encryptor.py --input {tflite_path} --output models/encrypted_model.bin --generate-key --generate-c-header")

if __name__ == '__main__':
    main()
