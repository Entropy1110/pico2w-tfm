#!/usr/bin/env python3
"""
TensorFlow Lite Model Encryption Tool for TF-M Secure Partition

This script encrypts TensorFlow Lite model files for secure deployment in 
TF-M (Trusted Firmware-M) secure partitions. The encrypted models can only 
be decrypted and used within the secure environment.

Features:
- AES-256-GCM encryption for strong security
- Metadata preservation for model information
- C header file generation for embedded deployment
- Integrity verification with authentication tags

Usage:
    python model_encryptor.py --input model.tflite --output encrypted_model.bin --key-file key.bin
    python model_encryptor.py --input model.tflite --generate-key --output encrypted_model.bin
"""

import argparse
import os
import sys
import struct
import hashlib
from pathlib import Path
from typing import Tuple
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
import secrets

# Constants
AES_KEY_SIZE = 32  # 256 bits
AES_NONCE_SIZE = 12  # 96 bits for GCM
AUTH_TAG_SIZE = 16  # 128 bits
SALT_SIZE = 16  # 128 bits
MAGIC_HEADER = b"TFLM"  # Magic bytes for encrypted model
VERSION = 1

class ModelEncryptor:
    """TensorFlow Lite model encryption utility for secure partitions."""
    
    def __init__(self):
        self.key = None
        self.salt = None
        
    def generate_key(self) -> bytes:
        """Generate a random 256-bit encryption key."""
        return secrets.token_bytes(AES_KEY_SIZE)
    
    def derive_key_from_password(self, password: str, salt: bytes = None) -> Tuple[bytes, bytes]:
        """Derive encryption key from password using PBKDF2."""
        if salt is None:
            salt = secrets.token_bytes(SALT_SIZE)
        
        kdf = PBKDF2HMAC(
            algorithm=hashes.SHA256(),
            length=AES_KEY_SIZE,
            salt=salt,
            iterations=100000,  # NIST recommended minimum
        )
        key = kdf.derive(password.encode('utf-8'))
        return key, salt
    
    def encrypt_model(self, model_data: bytes, key: bytes) -> Tuple[bytes, bytes, bytes]:
        """
        Encrypt model data using AES-256-GCM.
        
        Returns:
            tuple: (encrypted_data, nonce, auth_tag)
        """
        # Generate random nonce
        nonce = secrets.token_bytes(AES_NONCE_SIZE)
        
        # Create AESGCM cipher
        aesgcm = AESGCM(key)
        
        # Encrypt data
        ciphertext = aesgcm.encrypt(nonce, model_data, None)
        
        # Split ciphertext and auth tag (last 16 bytes)
        encrypted_data = ciphertext[:-AUTH_TAG_SIZE]
        auth_tag = ciphertext[-AUTH_TAG_SIZE:]
        
        return encrypted_data, nonce, auth_tag
    
    def create_encrypted_package(self, model_data: bytes, key: bytes, 
                                model_name: str = "model") -> bytes:
        """
        Create encrypted model package with metadata.
        
        Package format:
        [4B] Magic header "TFLM"
        [4B] Version (little endian)
        [4B] Original model size (little endian)
        [4B] Encrypted data size (little endian)
        [32B] Model hash (SHA-256)
        [12B] Nonce
        [16B] Auth tag
        [32B] Model name (null-padded)
        [Variable] Encrypted model data
        """
        # Calculate model hash
        model_hash = hashlib.sha256(model_data).digest()
        
        # Encrypt model
        encrypted_data, nonce, auth_tag = self.encrypt_model(model_data, key)
        
        # Prepare model name (32 bytes, null-padded)
        model_name_bytes = model_name.encode('utf-8')[:31]  # Max 31 chars + null
        model_name_padded = model_name_bytes.ljust(32, b'\x00')
        
        # Create package header
        header = struct.pack('<4sIII32s12s16s32s',
                           MAGIC_HEADER,
                           VERSION,
                           len(model_data),
                           len(encrypted_data),
                           model_hash,
                           nonce,
                           auth_tag,
                           model_name_padded)
        
        # Combine header and encrypted data
        package = header + encrypted_data
        
        return package
    
    def save_key_file(self, key: bytes, key_file_path: str, salt: bytes = None):
        """Save encryption key to file."""
        key_data = key
        if salt is not None:
            # Prepend salt if available (for password-derived keys)
            key_data = salt + key
            
        with open(key_file_path, 'wb') as f:
            f.write(key_data)
        
        # Set restrictive permissions
        os.chmod(key_file_path, 0o600)
        print(f"Key saved to: {key_file_path}")
    
    def load_key_file(self, key_file_path: str) -> Tuple[bytes, bytes]:
        """Load encryption key from file."""
        with open(key_file_path, 'rb') as f:
            key_data = f.read()
        
        if len(key_data) == AES_KEY_SIZE:
            # Key only
            return key_data, None
        elif len(key_data) == SALT_SIZE + AES_KEY_SIZE:
            # Salt + key
            salt = key_data[:SALT_SIZE]
            key = key_data[SALT_SIZE:]
            return key, salt
        else:
            raise ValueError(f"Invalid key file format. Expected {AES_KEY_SIZE} or {SALT_SIZE + AES_KEY_SIZE} bytes, got {len(key_data)}")
    
    def generate_c_header(self, encrypted_package: bytes, key: bytes, 
                         header_file_path: str, array_name: str = "encrypted_model"):
        """Generate C header file with encrypted model data."""
        header_content = f"""/* Auto-generated encrypted TensorFlow Lite model */
/* Generated by model_encryptor.py */

#ifndef ENCRYPTED_MODEL_H
#define ENCRYPTED_MODEL_H

#include <stdint.h>
#include <stddef.h>

/* Encrypted model data */
extern const uint8_t {array_name}_data[];
extern const size_t {array_name}_size;

/* Encryption key (should be stored securely) */
extern const uint8_t {array_name}_key[];
extern const size_t {array_name}_key_size;

/* Model metadata */
#define {array_name.upper()}_MAGIC_HEADER 0x{MAGIC_HEADER[::-1].hex().upper()}
#define {array_name.upper()}_VERSION {VERSION}

#endif /* ENCRYPTED_MODEL_H */
"""
        
        # Write header file
        with open(header_file_path, 'w') as f:
            f.write(header_content)
        
        # Generate source file
        source_file_path = header_file_path.replace('.h', '.c')
        source_content = f"""/* Auto-generated encrypted TensorFlow Lite model */
/* Generated by model_encryptor.py */

#include "{os.path.basename(header_file_path)}"

/* Encrypted model data ({len(encrypted_package)} bytes) */
const uint8_t {array_name}_data[] = {{
"""
        
        # Add encrypted data as hex array
        for i in range(0, len(encrypted_package), 16):
            chunk = encrypted_package[i:i+16]
            hex_values = ', '.join(f'0x{b:02x}' for b in chunk)
            source_content += f"    {hex_values},\n"
        
        source_content += f"""}};\n
const size_t {array_name}_size = sizeof({array_name}_data);

/* Encryption key ({len(key)} bytes) */
const uint8_t {array_name}_key[] = {{
"""
        
        # Add key as hex array
        for i in range(0, len(key), 16):
            chunk = key[i:i+16]
            hex_values = ', '.join(f'0x{b:02x}' for b in chunk)
            source_content += f"    {hex_values},\n"
        
        source_content += f"""}};\n
const size_t {array_name}_key_size = sizeof({array_name}_key);
"""
        
        # Write source file
        with open(source_file_path, 'w') as f:
            f.write(source_content)
        
        print(f"C header generated: {header_file_path}")
        print(f"C source generated: {source_file_path}")
    
    def encrypt_tflite_model(self, input_path: str, output_path: str, 
                           key_path: str = None, password: str = None,
                           generate_key: bool = False, generate_c_header: bool = False):
        """Main encryption workflow."""
        
        # Read input model
        print(f"Reading model: {input_path}")
        with open(input_path, 'rb') as f:
            model_data = f.read()
        
        print(f"Model size: {len(model_data)} bytes")
        
        # Handle key generation/loading
        key = None
        salt = None
        
        if generate_key:
            if password:
                key, salt = self.derive_key_from_password(password)
                print("Generated key from password")
            else:
                key = self.generate_key()
                print("Generated random key")
        elif key_path:
            key, salt = self.load_key_file(key_path)
            print(f"Loaded key from: {key_path}")
        else:
            raise ValueError("Must specify --key-file or --generate-key")
        
        # Get model name from input file
        model_name = Path(input_path).stem
        
        # Encrypt model
        print("Encrypting model...")
        encrypted_package = self.create_encrypted_package(model_data, key, model_name)
        
        # Save encrypted package
        with open(output_path, 'wb') as f:
            f.write(encrypted_package)
        
        print(f"Encrypted model saved: {output_path}")
        print(f"Encrypted size: {len(encrypted_package)} bytes")
        
        # Save key if generated
        if generate_key and key_path:
            self.save_key_file(key, key_path, salt)
        
        # Generate C header if requested
        if generate_c_header:
            header_path = output_path.replace('.bin', '.h')
            array_name = f"encrypted_{model_name}"
            self.generate_c_header(encrypted_package, key, header_path, array_name)


def main():
    parser = argparse.ArgumentParser(description='Encrypt TensorFlow Lite models for TF-M secure partitions')
    
    # Required arguments
    parser.add_argument('--input', '-i', required=True,
                       help='Input TensorFlow Lite model file (.tflite)')
    parser.add_argument('--output', '-o', required=True,
                       help='Output encrypted model file (.bin)')
    
    # Key management
    key_group = parser.add_mutually_exclusive_group(required=True)
    key_group.add_argument('--key-file', '-k',
                          help='Path to encryption key file')
    key_group.add_argument('--generate-key', action='store_true',
                          help='Generate new encryption key')
    
    # Optional arguments
    parser.add_argument('--password', '-p',
                       help='Password for key derivation (use with --generate-key)')
    parser.add_argument('--generate-c-header', action='store_true',
                       help='Generate C header file for embedded deployment')
    parser.add_argument('--output-key', 
                       help='Output path for generated key (use with --generate-key)')
    
    args = parser.parse_args()
    
    # Validate arguments
    if not os.path.exists(args.input):
        print(f"Error: Input file not found: {args.input}")
        sys.exit(1)
    
    if args.generate_key and not args.output_key:
        # Default key output path
        args.output_key = args.output.replace('.bin', '.key')
    
    try:
        encryptor = ModelEncryptor()
        encryptor.encrypt_tflite_model(
            input_path=args.input,
            output_path=args.output,
            key_path=args.output_key if args.generate_key else args.key_file,
            password=args.password,
            generate_key=args.generate_key,
            generate_c_header=args.generate_c_header
        )
        
        print("\nEncryption completed successfully!")
        
        if args.generate_key:
            print(f"\nIMPORTANT: Keep the key file secure: {args.output_key}")
            print("The encrypted model cannot be used without this key.")
        
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
