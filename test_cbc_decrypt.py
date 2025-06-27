#!/usr/bin/env python3

"""
Test script to verify our CBC encryption/decryption works in Python
"""

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
import struct

# Read the encrypted model file
with open('models/encrypted_mnist_model_psa.bin', 'rb') as f:
    package = f.read()

print(f"Package size: {len(package)} bytes")

# Parse the package (CBC format)
magic = struct.unpack('<I', package[0:4])[0]
version = struct.unpack('<I', package[4:8])[0] 
original_size = struct.unpack('<I', package[8:12])[0]
iv = package[12:28]  # 16 bytes
ciphertext = package[28:]

print(f"Magic: 0x{magic:08x}")
print(f"Version: {version}")
print(f"Original size: {original_size}")
print(f"IV: {iv.hex()}")
print(f"Ciphertext size: {len(ciphertext)}")

# Load the key
with open('models/model_key_psa.bin', 'rb') as f:
    key = f.read()

print(f"Key: {key.hex()}")
print(f"Key size: {len(key)} bytes")

# Try to decrypt using Python AES-CBC
try:
    # Create AES-CBC cipher
    cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
    decryptor = cipher.decryptor()
    
    print(f"Decrypting with AES-CBC...")
    print(f"Input size: {len(ciphertext)} bytes")
    
    # Decrypt the data
    padded_data = decryptor.update(ciphertext) + decryptor.finalize()
    
    print(f"✅ Decryption successful!")
    print(f"Padded data size: {len(padded_data)} bytes")
    
    # Remove PKCS7 padding
    padding_length = padded_data[-1]
    print(f"PKCS7 padding length: {padding_length}")
    
    if padding_length > 16 or padding_length == 0:
        print(f"❌ Invalid padding length: {padding_length}")
        exit(0)
    
    # Verify padding
    for i in range(padding_length):
        if padded_data[-(i+1)] != padding_length:
            print(f"❌ Invalid padding at position {-(i+1)}")
            exit(0)
    
    decrypted_data = padded_data[:-padding_length]
    
    print(f"Decrypted size after removing padding: {len(decrypted_data)} bytes")
    print(f"Expected size: {original_size} bytes")
    
    if len(decrypted_data) != original_size:
        print(f"⚠️  Size mismatch: got {len(decrypted_data)}, expected {original_size}")
    
    print(f"Decrypted data (first 16 bytes): {decrypted_data[:16].hex()}")
    
    # Check if it looks like a TinyMaix model (magic header)
    if len(decrypted_data) >= 4:
        model_magic = struct.unpack('<I', decrypted_data[0:4])[0]
        print(f"Model magic: 0x{model_magic:08x}")
        if model_magic == 0x5849414D:  # "MAIX" in little endian
            print("✅ Valid TinyMaix model detected!")
        else:
            print(f"⚠️  Invalid model magic. Expected 0x5849414D, got 0x{model_magic:08x}")
            # Show the actual bytes
            print(f"First 4 bytes as hex: {decrypted_data[:4].hex()}")
            print(f"First 4 bytes as ASCII: {decrypted_data[:4]}")
    
except Exception as e:
    print(f"❌ Decryption failed: {e}")
    import traceback
    traceback.print_exc()
