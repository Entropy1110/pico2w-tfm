# Documentation

This directory contains comprehensive developer documentation for the Pico 2W TF-M TinyMaix project.

## 📚 Document Index

### Core Architecture
- **[TF-M Architecture](./tfm-architecture.md)** - Trusted Firmware-M dual-world architecture and security model
- **[Secure Partitions](./secure-partitions.md)** - Development guide for custom secure services
- **[PSA API](./psa-api.md)** - Platform Security Architecture API usage patterns

### Machine Learning Integration
- **[TinyMaix Integration](./tinymaix-integration.md)** - Neural network inference in secure partitions
- **[Model Encryption](./model-encryption.md)** - Secure model deployment with HUK-derived keys

### Development Tools
- **[Build System](./build-system.md)** - CMake configuration and build processes
- **[Testing Framework](./testing-framework.md)** - Test architecture and development patterns

### Troubleshooting
- **[Troubleshooting Guide](./troubleshooting.md)** - Common issues and solutions

## 🎯 Quick Start

For immediate development setup, see:
1. [Build System](./build-system.md#quick-start) - Get building immediately
2. [TF-M Architecture](./tfm-architecture.md#development-overview) - Understand the security model
3. [Secure Partitions](./secure-partitions.md#creating-new-partition) - Add your first secure service

## 🔒 Security Notes

This project implements ARM TrustZone security features. Please review the security considerations in each document before development.

**DEV_MODE Warning**: Development mode exposes HUK-derived encryption keys for debugging. Never deploy DEV_MODE builds to production hardware.