# TF-M Testing Framework Guide

This document provides a guide to the TF-M (Trusted Firmware-M) testing framework, focusing on how to build, run, and analyze tests for secure applications on the Pico2W platform.

## Overview

The TF-M testing framework is crucial for verifying the correctness and security of PSA (Platform Security Architecture) services and secure partitions. It encompasses various test suites, build configurations, and execution methods.

## Test Types and Structure

TF-M employs several types of tests:

*   **PSA API Tests**: Validate compliance with PSA APIs (Crypto, Storage, Attestation, etc.).
*   **Secure Partition Tests**: Test the functionality of individual secure partitions.
*   **Regression Tests**: Ensure that new changes do not break existing functionality.
*   **IPC (Inter-Process Communication) Tests**: Verify communication mechanisms between secure and non-secure worlds, and between partitions.

### Test Suite Structure
```
tf-m-tests/
├── test
│   ├── framework                   # Core test framework components
│   ├── psa_api_tests               # PSA API compliance tests
│   ├── secure_fw                   # Secure firmware specific tests
│   │   ├── core_test               # Core TF-M functionality tests
│   │   ├── initial_attestation     # Attestation service tests
│   │   ├── internal_trusted_storage # ITS tests
│   │   ├── protected_storage       # PS tests
│   │   └── ...
│   ├── spe_test_app                # Secure Processing Environment test application
│   └── ...
├── CMakeLists.txt                  # Main CMake file for tests
└── ...
```

## Building Tests

Tests are built as part of the TF-M build process. Specific test configurations can be enabled or disabled via CMake options.

### Key CMake Options for Testing

*   `TFM_TEST_PSA_API_CRYPTO`: Enable PSA Crypto API tests.
*   `TFM_TEST_PSA_API_STORAGE`: Enable PSA Storage (ITS/PS) API tests.
*   `TFM_TEST_PSA_API_ATTESTATION`: Enable PSA Attestation API tests.
*   `TFM_REGRESSION_TEST`: Enable regression tests.
*   `TFM_PARTITION_TEST_CORE`: Enable core TF-M tests.
*   `TFM_PARTITION_TEST_SECURE_SERVICES`: Enable tests for various secure services.

### Example Build Command with Tests Enabled

```bash
# Assumes you are in the build directory
cmake -S /path/to/tf-m-source -B . \
      -DTFM_PLATFORM=trustedfirmware/pico2w \
      -DTFM_TOOLCHAIN=GNUARM \
      -DTFM_PROFILE=profile_medium \
      -DTFM_ISOLATION_LEVEL=1 \
      -DCMAKE_BUILD_TYPE=Debug \
      -DTFM_REGRESSION_TEST=ON \
      -DTFM_TEST_PSA_API_CRYPTO=ON \
      -DTFM_TEST_PSA_API_STORAGE=ON \
      -DTFM_TEST_PSA_API_ATTESTATION=ON
make -j$(nproc)
```
This command configures the build for the Pico2W platform with `profile_medium`, isolation level 1, and enables regression tests along with specific PSA API tests.

## Running Tests

After building, the test firmware image (`tfm_s_ns_signed.bin` or similar) needs to be flashed onto the Pico2W.

### Flashing the Test Firmware

Use the `pico_uf2.sh` script or a debugger (e.g., Picoprobe with OpenOCD) to flash the image.

```bash
# Using pico_uf2.sh (assuming Pico2W is in BOOTSEL mode)
./pico_uf2.sh build/spe/tfm_s_ns_signed.bin
```

### Test Execution and Output

Tests typically run automatically after the device boots. Test results are outputted via the serial console (UART). Connect a serial terminal (e.g., minicom, PuTTY, VS Code Serial Monitor) to the Pico2W's UART pins to observe the output.

**Default UART Configuration for Pico2W:**
*   Baud Rate: 115200
*   Data Bits: 8
*   Parity: None
*   Stop Bits: 1

### Example Test Output Snippet
```
[INF] Starting TFM_REGRESSION_TEST_CORE...
[Sec Thread] Regression test suite started
[Sec Thread] Running Test Case: CORE_TEST_POSITIVE
[Sec Thread] Test Case: CORE_TEST_POSITIVE PASSED
[Sec Thread] Running Test Case: CORE_TEST_NEGATIVE_INVALID_PARAM
[Sec Thread] Test Case: CORE_TEST_NEGATIVE_INVALID_PARAM PASSED
...
[INF] TFM_REGRESSION_TEST_CORE: PASSED
[INF] Starting TFM_PSA_API_TEST_CRYPTO...
...
[INF] TFM_PSA_API_TEST_CRYPTO: FAILED (1 test case failed)
```

## Analyzing Test Results

Test results indicate `PASSED` or `FAILED` for each test suite and individual test case.

*   **PASSED**: The test case or suite completed successfully.
*   **FAILED**: One or more assertions within the test case failed. The output usually provides details about the failure.

### Debugging Failed Tests

1.  **Examine Logs**: Carefully review the serial output for error messages or unexpected behavior leading up to the failure.
2.  **Increase Log Verbosity**: TF-M often has different log levels (e.g., `TFM_LOG_LEVEL_DEBUG`). Rebuild with a higher verbosity if necessary.
3.  **Use a Debugger**: Attach a debugger like GDB via Picoprobe. Set breakpoints in the failing test case or relevant TF-M core/service code to step through execution and inspect variables.
4.  **Isolate the Test**: If possible, try to run the failing test in isolation or simplify the test case to pinpoint the issue.
5.  **Check Configurations**: Ensure the build configuration, platform settings, and partition manifests are correct for the feature being tested.

## Adding New Tests

To add new tests, you typically need to:

1.  **Create Test Source Files**: Write C files containing your test logic, using the TF-M test framework APIs (e.g., `TEST_ASSERT`, `tfm_psa_test_common_init`).
2.  **Define Test Suites and Cases**: Organize tests into suites and individual cases.
3.  **Update CMakeLists.txt**: Add your new test source files and any necessary configurations to the relevant `CMakeLists.txt` file within the `tf-m-tests` directory.
4.  **Register the Test Suite**: Ensure your test suite is called by the main test application.

### Example Test Case Snippet (Conceptual)
```c
// in my_custom_test.c
#include "test_framework.h"
#include "psa/crypto.h" // Or other relevant PSA headers

static void tfm_my_custom_test_case_1(struct test_result_t *ret) {
    psa_status_t status;
    // Test logic here
    status = psa_crypto_init();
    TEST_ASSERT_EQUAL(status, PSA_SUCCESS, "psa_crypto_init failed");
    
    // ... more test steps ...
    
    ret->val = TEST_PASSED;
}

static struct test_suite_t my_custom_test_suite = {
    .name = "MY_CUSTOM_TEST_SUITE",
    .test_cases_num = 1,
    .test_cases = (struct test_case_t []){
        {"MY_CUSTOM_TEST_CASE_1", tfm_my_custom_test_case_1, 0, 0}
    }
};

void register_my_custom_test_suite(void) {
    register_testsuite(&my_custom_test_suite);
}
```
Then, call `register_my_custom_test_suite()` from the appropriate test runner.

## Advanced Testing Topics

### Non-Secure Client Tests

Many tests involve a Non-Secure Processing Environment (NSPE) client calling PSA services in the Secure Processing Environment (SPE). These tests verify the full PSA IPC mechanism.

### Testing with Different Isolation Levels

TF-M supports different isolation levels (1, 2, 3). Testing should ideally be performed across all configured isolation levels, as behavior can differ, especially concerning memory access and partition interactions.

### Code Coverage

Tools like `gcov`/`lcov` can be integrated with the build system to measure test code coverage, helping identify untested parts of the codebase. This usually requires specific compiler flags and post-processing of build artifacts.

## Troubleshooting Common Test Issues

*   **Build Failures**:
    *   Check CMake configurations and dependencies.
    *   Ensure the toolchain is correctly set up.
*   **Device Not Booting/Crashing**:
    *   Stack overflows in partitions (increase stack size in manifest).
    *   Memory protection faults (MPU misconfiguration).
    *   Incorrect peripheral initialization.
*   **Tests Hanging**:
    *   Deadlocks in IPC calls.
    *   Infinite loops in test logic or service implementation.
*   **Unexpected Test Failures**:
    *   Race conditions.
    *   Incorrect assumptions about hardware state or service behavior.
    *   Issues in the underlying TF-M core or platform port.

Refer to the main [Troubleshooting Guide](./troubleshooting.md) for more general TF-M issues.