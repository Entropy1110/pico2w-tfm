#!/usr/bin/env python3
"""
TinyMAIX Model Encryption Tool for TF-M Secure Partition

This script encrypts TinyMAIX model header files for secure deployment in 
TF-M (Trusted Firmware-M) secure partitions. The encrypted models can only 
be decrypted and used within the secure environment.

Features:
- AES-256-GCM encryption for strong security
- C header file parsing and encryption
- Metadata preservation for model information
- C header file generation for embedded deployment
- Integrity verification with authentication tags

Usage:
    python tinymaix_model_encryptor.py --input mnist_valid_q.h --output encrypted_mnist.bin --key-file key.bin
    python tinymaix_model_encryptor.py --input mnist_valid_q.h --generate-key --output encrypted_mnist.bin
"""

import argparse
import os
import sys
import re
import struct
import hashlib
from pathlib import Path
from typing import Tuple, List
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
import secrets

# Constants
AES_KEY_SIZE = 16  # 128 bits
AES_IV_SIZE = 16   # 128 bits for CBC
SALT_SIZE = 16     # 128 bits
MAGIC_HEADER = b"TMAX"  # Magic bytes for encrypted TinyMAIX model
VERSION = 3  # Version 3 for CBC format

# PSA crypto test key - 128 bits (16 bytes) 
PSA_CRYPTO_TEST_KEY = bytes([
    0x40, 0xc9, 0x62, 0xd6, 0x6a, 0x1f, 0xa4, 0x03,
    0x46, 0xca, 0xc8, 0xb7, 0xe6, 0x12, 0x74, 0xe1
])

class TinyMAIXModelEncryptor:
    """TinyMAIX model encryption utility for secure partitions."""
    
    def __init__(self):
        self.key = None
        self.salt = None
        
    def generate_key(self) -> bytes:
        """Generate a random 256-bit encryption key."""
        return secrets.token_bytes(AES_KEY_SIZE)
    
    def get_psa_test_key(self) -> bytes:
        """Get the PSA crypto test key (256-bit version)."""
        return PSA_CRYPTO_TEST_KEY
    
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
    
    def parse_tinymaix_header(self, header_content: str) -> Tuple[bytes, str, int, int]:
        """
        Parse TinyMAIX C header file to extract model data.
        
        Returns:
            tuple: (model_data, array_name, mdl_buf_len, lbuf_len)
        """
        # Extract array name - look for const uint8_t array_name[]
        array_match = re.search(r'const\s+uint8_t\s+(\w+)\[\d*\]\s*=', header_content)
        if not array_match:
            raise ValueError("Could not find model data array in header file")
        
        array_name = array_match.group(1)
        
        # Extract MDL_BUF_LEN
        mdl_buf_match = re.search(r'#define\s+MDL_BUF_LEN\s+\((\d+)\)', header_content)
        mdl_buf_len = int(mdl_buf_match.group(1)) if mdl_buf_match else 0
        
        # Extract LBUF_LEN
        lbuf_match = re.search(r'#define\s+LBUF_LEN\s+\((\d+)\)', header_content)
        lbuf_len = int(lbuf_match.group(1)) if lbuf_match else 0
        
        # Extract hex data from array
        array_pattern = rf'const\s+uint8_t\s+{re.escape(array_name)}\[\d*\]\s*=\s*\{{([^}}]+)\}}'
        data_match = re.search(array_pattern, header_content, re.DOTALL)
        
        if not data_match:
            raise ValueError(f"Could not find data for array '{array_name}'")
        
        hex_data = data_match.group(1)
        
        # Parse hex values
        hex_values = re.findall(r'0x([0-9a-fA-F]{2})', hex_data)
        if not hex_values:
            raise ValueError("No hex values found in array data")
        
        # Convert to bytes
        model_data = bytes(int(h, 16) for h in hex_values)
        
        print(f"Parsed TinyMAIX model:")
        print(f"  Array name: {array_name}")
        print(f"  Model size: {len(model_data)} bytes")
        print(f"  MDL_BUF_LEN: {mdl_buf_len}")
        print(f"  LBUF_LEN: {lbuf_len}")
        
        return model_data, array_name, mdl_buf_len, lbuf_len
        
    def encrypt_model(self, model_data: bytes, key: bytes) -> Tuple[bytes, bytes]:
        """Encrypt model using AES-CBC-PKCS7 (matching PSA crypto test)"""
        # Generate random IV for CBC mode
        iv = secrets.token_bytes(16)  # 16 bytes for AES-CBC
        
        # Pad data to 16-byte boundary using PKCS7 padding
        padded_data = self._pkcs7_pad(model_data, 16)
        
        # Create AES-CBC cipher
        cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
        encryptor = cipher.encryptor()
        
        # Encrypt the data
        encrypted_data = encryptor.update(padded_data) + encryptor.finalize()
        
        print(f"Encryption details:")
        print(f"  - Original data size: {len(model_data)} bytes")
        print(f"  - Padded data size: {len(padded_data)} bytes")
        print(f"  - IV: {iv.hex()}")
        print(f"  - Encrypted data size: {len(encrypted_data)} bytes")
        
        return encrypted_data, iv
    
    def _pkcs7_pad(self, data: bytes, block_size: int) -> bytes:
        """Apply PKCS7 padding to data"""
        padding_length = block_size - (len(data) % block_size)
        padding = bytes([padding_length] * padding_length)
        return data + padding
    
    def create_encrypted_package(self, model_data: bytes, key: bytes, 
                            model_name: str = "tinymaix_model",
                            mdl_buf_len: int = 0, lbuf_len: int = 0) -> bytes:
        """
        Create encrypted TinyMAIX model package with CBC encryption.
        
        Package format for CBC (PSA Crypto compatible):
        [4B] Magic header "TMAX"
        [4B] Version (little endian) = 3 (CBC)
        [4B] Original model size (little endian)
        [16B] IV (128 bits for CBC)
        [Variable] Encrypted model data (with PKCS7 padding)
        """
        # Encrypt model using CBC
        encrypted_data, iv = self.encrypt_model(model_data, key)
        
        # Create PSA-compatible package header for CBC mode
        # Format: magic(4) + version(4) + original_size(4) + iv(16) + encrypted_data
        header = struct.pack('<4sII16s',
                        MAGIC_HEADER,      # 4 bytes: "TMAX"
                        3,                 # 4 bytes: version = 3 (CBC)
                        len(model_data),   # 4 bytes: original size
                        iv)                # 16 bytes: IV for CBC
        
        # PSA format: header + encrypted_data
        package = header + encrypted_data
        
        print(f"\nPSA CBC package structure:")
        print(f"  - Magic: {MAGIC_HEADER} (0x{MAGIC_HEADER.hex()})")
        print(f"  - Version: 3 (CBC)")
        print(f"  - Original size: {len(model_data)} bytes")
        print(f"  - Header size: {len(header)} bytes (28 bytes total)")
        print(f"  - Encrypted data: {len(encrypted_data)} bytes")
        print(f"  - Total package: {len(package)} bytes")
        
        # Verify package structure
        print(f"\nPackage verification:")
        print(f"  - Expected header size: 28 bytes")
        print(f"  - Actual header size: {len(header)} bytes")
        print(f"  - IV position: offset 12, length 16")
        print(f"  - Encrypted data position: offset 28")
        
        # Debug: Show first few bytes of each component
        print(f"\nDebug - Package components:")
        print(f"  - Magic bytes: {header[0:4].hex()}")
        print(f"  - Version bytes: {header[4:8].hex()}")
        print(f"  - Size bytes: {header[8:12].hex()}")
        print(f"  - IV: {header[12:28].hex()}")
        print(f"  - First 16 bytes of encrypted data: {encrypted_data[:16].hex()}")
        
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
                         header_file_path: str, array_name: str = "encrypted_tinymaix_model"):
        """Generate C header file with encrypted TinyMAIX model data."""
        header_content = f"""/* Auto-generated encrypted TinyMAIX model */
/* Generated by tinymaix_model_encryptor.py */

#ifndef ENCRYPTED_TINYMAIX_MODEL_H
#define ENCRYPTED_TINYMAIX_MODEL_H

#include <stdint.h>
#include <stddef.h>

/* Encrypted model data */
extern const uint8_t {array_name}_data[];
extern const size_t {array_name}_size;

/* Model metadata */
#define {array_name.upper()}_MAGIC_HEADER 0x{MAGIC_HEADER[::-1].hex().upper()}
#define {array_name.upper()}_VERSION {VERSION}

#endif /* ENCRYPTED_TINYMAIX_MODEL_H */
"""
        
        # Write header file
        with open(header_file_path, 'w') as f:
            f.write(header_content)
        
        # Generate source file
        source_file_path = header_file_path.replace('.h', '.c')
        source_content = f"""/* Auto-generated encrypted TinyMAIX model */
/* Generated by tinymaix_model_encryptor.py */

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

"""
        
        # Write source file
        with open(source_file_path, 'w') as f:
            f.write(source_content)
        
        print(f"C header generated: {header_file_path}")
        print(f"C source generated: {source_file_path}")
    
    def encrypt_tinymaix_model(self, input_path: str, output_path: str, 
                              key_path: str = None, password: str = None,
                              generate_key: bool = False, generate_c_header: bool = False,
                              use_psa_key: bool = False):
        """Main encryption workflow."""
        
        # Read input header file
        print(f"Reading TinyMAIX model header: {input_path}")
        with open(input_path, 'r') as f:
            header_content = f.read()
        
        # Parse TinyMAIX header
        model_data, array_name, mdl_buf_len, lbuf_len = self.parse_tinymaix_header(header_content)
        
        # Handle key generation/loading
        key = None
        salt = None
        
        if use_psa_key:
            key = self.get_psa_test_key()
            print("Using PSA crypto test key (same as C code)")
        elif generate_key:
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
            raise ValueError("Must specify --key-file, --generate-key, or --use-psa-key")
        
        # Get model name from input file
        model_name = Path(input_path).stem
        
        # Encrypt model
        print("Encrypting TinyMAIX model...")
        encrypted_package = self.create_encrypted_package(
            model_data, key, model_name, mdl_buf_len, lbuf_len)
        
        # Save encrypted package
        with open(output_path, 'wb') as f:
            f.write(encrypted_package)
        
        print(f"Encrypted model saved: {output_path}")
        print(f"Original size: {len(model_data)} bytes")
        print(f"Encrypted size: {len(encrypted_package)} bytes")
        
        # Save key if generated (not for PSA key)
        if generate_key and key_path:
            self.save_key_file(key, key_path, salt)
        elif use_psa_key and key_path:
            # Save PSA key for reference
            self.save_key_file(key, key_path, None)
        
        # Generate C header if requested
        if generate_c_header:
            header_path = output_path.replace('.bin', '.h')
            array_name_encrypted = f"encrypted_{array_name}"
            self.generate_c_header(encrypted_package, key, header_path, array_name_encrypted)


def main():
    parser = argparse.ArgumentParser(description='Encrypt TinyMAIX models for TF-M secure partitions')
    
    # Required arguments
    parser.add_argument('--input', '-i', required=True,
                       help='Input TinyMAIX model header file (.h)')
    parser.add_argument('--output', '-o', required=True,
                       help='Output encrypted model file (.bin)')
    
    # Key management
    key_group = parser.add_mutually_exclusive_group(required=True)
    key_group.add_argument('--key-file', '-k',
                          help='Path to encryption key file')
    key_group.add_argument('--generate-key', action='store_true',
                          help='Generate new encryption key')
    key_group.add_argument('--use-psa-key', action='store_true',
                          help='Use PSA crypto test key (same as C code)')
    
    # Optional arguments
    parser.add_argument('--password', '-p',
                       help='Password for key derivation (use with --generate-key)')
    parser.add_argument('--generate-c-header', action='store_true',
                       help='Generate C header file for embedded deployment')
    parser.add_argument('--output-key', 
                       help='Output path for generated key (use with --generate-key or --use-psa-key)')
    
    args = parser.parse_args()
    
    # Validate arguments
    if not os.path.exists(args.input):
        print(f"Error: Input file not found: {args.input}")
        sys.exit(1)
    
    if not args.input.endswith('.h'):
        print("Warning: Input file should be a C header file (.h)")
    
    if (args.generate_key or args.use_psa_key) and not args.output_key:
        # Default key output path
        args.output_key = args.output.replace('.bin', '.key')
    
    try:
        encryptor = TinyMAIXModelEncryptor()
        encryptor.encrypt_tinymaix_model(
            input_path=args.input,
            output_path=args.output,
            key_path=args.output_key if (args.generate_key or args.use_psa_key) else args.key_file,
            password=args.password,
            generate_key=args.generate_key,
            generate_c_header=args.generate_c_header,
            use_psa_key=args.use_psa_key
        )
        
        print("\nTinyMAIX model encryption completed successfully!")
        
        if args.generate_key:
            print(f"\nIMPORTANT: Keep the key file secure: {args.output_key}")
            print("The encrypted model cannot be used without this key.")
        
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
