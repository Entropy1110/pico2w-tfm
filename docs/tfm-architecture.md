# TF-M Architecture Guide

## Overview

Trusted Firmware-M (TF-M) implements ARM Platform Security Architecture (PSA) on Cortex-M processors, providing a dual-world security model using ARM TrustZone technology.

## Dual-World Architecture

### Secure Processing Environment (SPE)
- **Location**: `tflm_spe/` directory
- **Purpose**: Hosts secure services and sensitive operations
- **Components**:
  - Secure Partition Manager (SPM)
  - Secure services (partitions)
  - PSA Root of Trust services
  - Crypto operations
  - Neural network inference (TinyMaix)

### Non-Secure Processing Environment (NSPE)
- **Location**: `tflm_ns/` and `app_broker/` directories  
- **Purpose**: Hosts application logic and user interfaces
- **Components**:
  - Application code
  - Test suites
  - PSA client API calls
  - RTOS (RTX)

## Security Boundaries

```
┌─────────────────────────────────────────────────────────────┐
│                    Non-Secure World (NSPE)                  │
├─────────────────────────────────────────────────────────────┤
│  • Application Code (tflm_ns/)                              │
│  • Test Applications                                        │
│  • PSA Client APIs                                          │
│  • RTX RTOS                                                 │
└─────────────────────────────────────────────────────────────┘
                               │
                    PSA Interface Boundary
                               │
┌─────────────────────────────────────────────────────────────┐
│                     Secure World (SPE)                      │
├─────────────────────────────────────────────────────────────┤
│  • Secure Partition Manager (SPM)                           │
│  • Crypto Services                                          │
│  • Echo Service Partition                                   │
│  • TinyMaix Inference Partition                             │
│  • Internal Trusted Storage                                 │
│  • Platform Services                                        │
└─────────────────────────────────────────────────────────────┘
                               │
                    Hardware Boundary
                               │
┌─────────────────────────────────────────────────────────────┐
│                Hardware (RP2350)                            │
├─────────────────────────────────────────────────────────────┤
│  • Hardware Unique Key (HUK)                                │
│  • TrustZone Security Extensions                            │
│  • Secure/Non-Secure Memory Regions                         │
│  • Peripheral Protection                                    │
└─────────────────────────────────────────────────────────────┘
```

## Memory Layout

### RP2350 Memory Mapping
```
0x10000000 ┌─────────────────┐
           │  Bootloader     │ BL2 (MCUBoot)
0x10011000 ├─────────────────┤
           │  Secure Image   │ TF-M SPE + Partitions
           │                 │
           ├─────────────────┤
           │ Non-Secure Image│ Application + Tests  
           │                 │
0x1019F000 ├─────────────────┤
           │  Provisioning   │ Keys & Certificates
0x101A0000 └─────────────────┘
```

### Build Artifacts Location
- **SPE Image**: `build/spe/bin/tfm_s.bin`
- **NSPE Image**: `build/nspe/bin/tfm_ns.bin`
- **Combined**: `build/spe/bin/tfm_s_ns_signed.uf2`
- **Bootloader**: `build/spe/bin/bl2.uf2`

## Secure Partitions

### Partition Types
1. **PSA Root of Trust (PSA-ROT)**
   - Platform services
   - Crypto services  
   - Internal Trusted Storage

2. **Application Root of Trust (APP-ROT)**
   - Echo Service (PID: 444, SID: 0x00000105)
   - TinyMaix Inference (PID: 445, SID: 0x00000107)

### Partition Isolation
- Each partition runs in its own memory space
- Stack size defined in manifest files
- Inter-partition communication via PSA API
- Secure Function (SFN) model used

## Communication Flow

### PSA Client-Service Communication
```
Non-Secure Application
         │
         ▼
    psa_connect()
         │
         ▼
   TrustZone Call
         │
         ▼
   Secure Partition
         │
         ▼
   Service Handler
         │
         ▼
    psa_reply()
         │
         ▼
   Return to NS
```

### Key Communication Patterns
1. **Connection-based Services**
   - `psa_connect()` → `psa_call()` → `psa_close()`
   - State maintained between calls
   - Used by TinyMaix and Echo services

2. **Stateless Services**  
   - Direct `psa_call()` without connection
   - No state maintained
   - Used by some crypto operations

## Security Features

### ARM TrustZone Integration
- **Secure/Non-Secure memory separation**
- **Peripheral protection**
- **Secure/Non-Secure state switching**
- **Secure interrupts**

### PSA Security Model
- **Hardware Root of Trust**: HUK-based key derivation
- **Secure Boot**: Verified boot chain with BL2
- **Attestation**: Platform identity verification
- **Cryptographic Services**: Hardware-backed crypto operations

### Key Management
- **Hardware Unique Key (HUK)**: Device-specific root key
- **Key Derivation**: HKDF-SHA256 based derivation
- **Secure Storage**: Keys never exposed to non-secure world
- **DEV_MODE**: Debug key exposure (development only)

## Development Overview

### SPE Development
1. **Configuration**: `tflm_spe/config/config_tflm.cmake`
2. **Partition Manifests**: `partitions/manifest_list.yaml`
3. **Service Implementation**: `partitions/*/`
4. **Build Target**: SPE must build first

### NSPE Development  
1. **Application Code**: `tflm_ns/` and `app_broker/`
2. **PSA Client APIs**: `interface/src/`
3. **Test Applications**: Test suites and runners
4. **Build Dependencies**: Requires SPE build artifacts

### Inter-World API Design
1. **Header Definitions**: `interface/include/`
2. **Client Implementation**: `interface/src/`
3. **Service Implementation**: `partitions/*/`
4. **Message Types**: IPC command constants

## Performance Considerations

### Context Switching Overhead
- TrustZone world switches have latency
- Minimize frequent NS-to-S calls
- Batch operations when possible

### Memory Efficiency
- Secure partitions have limited stack space
- Shared buffers for large data transfers
- Static allocation preferred in secure world

### Crypto Performance
- Hardware crypto acceleration via PSA
- Secure key operations
- Optimized for embedded constraints

## Debugging and Monitoring

### Logging Framework
- **Secure Logging**: `tfm_log_unpriv` for secure partitions
- **Non-Secure Logging**: Standard printf via UART
- **Build Flags**: `TFM_LOG_LEVEL` controls verbosity

### Debug Interfaces
- **Serial Output**: UART/USB for logs and test results
- **DEV_MODE**: Exposes HUK-derived keys for debugging
- **Memory Inspection**: Via debugger with appropriate security settings

### Security Validation
- **Partition Isolation**: Memory protection verification
- **Key Security**: HUK derivation validation  
- **Interface Validation**: PSA API compliance checking

## Configuration Parameters

### Key Build Options
```cmake
# Platform and Board
-DTFM_PLATFORM=rpi/rp2350
-DPICO_BOARD=pico2_w

# Security Profile
-DTFM_PROFILE=profile_medium

# Partition Enablement
-DTFM_PARTITION_ECHO_SERVICE=ON
-DTFM_PARTITION_TINYMAIX_INFERENCE=ON

# Development Mode
-DDEV_MODE=ON  # DEBUG ONLY - exposes HUK keys
```

### Security Profiles
- **profile_small**: Minimal footprint, basic security
- **profile_medium**: Balanced security and performance  
- **profile_large**: Maximum security features

## Next Steps

- **[Secure Partitions Development](./secure-partitions.md)**: Create custom secure services
- **[PSA API Usage](./psa-api.md)**: Implement client-service communication
- **[TinyMaix Integration](./tinymaix-integration.md)**: Neural network in secure partitions