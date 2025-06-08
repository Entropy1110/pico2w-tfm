/*
 * Simple XOR Neural Network Model - CMSIS-NN Compatible
 * 
 * This is a simple 2-input, 1-output neural network that learns XOR operation.
 * Input: 2 int8 values (quantized from -1.0 to 1.0)
 * Output: 1 int8 value (quantized from -1.0 to 1.0)
 * 
 * Architecture:
 * - Input layer: 2 neurons
 * - Hidden layer: 4 neurons (ReLU activation)
 * - Output layer: 1 neuron (linear)
 */

#include <stdint.h>
#include <stddef.h>

// Model metadata
const char* MODEL_NAME = "Simple XOR Model";
const char* MODEL_VERSION = "1.0";
#define MODEL_INPUT_SIZE 2
#define MODEL_OUTPUT_SIZE 1
#define MODEL_HIDDEN_SIZE 4

// Quantization parameters
// Input/output range: -1.0 to 1.0 mapped to -128 to 127
const int32_t INPUT_ZERO_POINT = 0;
const float INPUT_SCALE = 1.0f / 127.0f;
const int32_t OUTPUT_ZERO_POINT = 0;
const float OUTPUT_SCALE = 1.0f / 127.0f;

// Hidden layer weights and biases (2x4 matrix)
// These weights are pre-trained to learn XOR operation
const int8_t hidden_weights[MODEL_INPUT_SIZE * MODEL_HIDDEN_SIZE] = {
    // Weights for hidden neuron 0
    100, -100,
    // Weights for hidden neuron 1
    -100, 100,
    // Weights for hidden neuron 2
    100, 100,
    // Weights for hidden neuron 3
    -100, -100
};

const int32_t hidden_biases[MODEL_HIDDEN_SIZE] = {
    -50, -50, -100, 100  // Biases for hidden layer
};

// Output layer weights and biases (4x1 matrix)
const int8_t output_weights[MODEL_HIDDEN_SIZE * MODEL_OUTPUT_SIZE] = {
    80, 80, -60, 60  // Weights from hidden to output
};

const int32_t output_biases[MODEL_OUTPUT_SIZE] = {
    0  // Bias for output neuron
};

// Quantization parameters for layers
const int32_t HIDDEN_INPUT_OFFSET = 0;
const int32_t HIDDEN_OUTPUT_OFFSET = 0;
const int32_t HIDDEN_OUTPUT_MULTIPLIER = 1073741824;  // 2^30
const int HIDDEN_OUTPUT_SHIFT = 7;
const int32_t HIDDEN_ACTIVATION_MIN = -128;
const int32_t HIDDEN_ACTIVATION_MAX = 127;

const int32_t OUTPUT_INPUT_OFFSET = 0;
const int32_t OUTPUT_OUTPUT_OFFSET = 0;
const int32_t OUTPUT_OUTPUT_MULTIPLIER = 1073741824;  // 2^30
const int OUTPUT_OUTPUT_SHIFT = 7;
const int32_t OUTPUT_ACTIVATION_MIN = -128;
const int32_t OUTPUT_ACTIVATION_MAX = 127;

// Model structure for CMSIS-NN
typedef struct {
    // Layer 1: Input to Hidden (Fully Connected + ReLU)
    struct {
        const int8_t* weights;
        const int32_t* biases;
        int input_size;
        int output_size;
        int32_t input_offset;
        int32_t output_offset;
        int32_t output_multiplier;
        int output_shift;
        int32_t activation_min;
        int32_t activation_max;
    } hidden_layer;
    
    // Layer 2: Hidden to Output (Fully Connected)
    struct {
        const int8_t* weights;
        const int32_t* biases;
        int input_size;
        int output_size;
        int32_t input_offset;
        int32_t output_offset;
        int32_t output_multiplier;
        int output_shift;
        int32_t activation_min;
        int32_t activation_max;
    } output_layer;
} simple_xor_model_t;

// Model instance
const simple_xor_model_t simple_xor_model = {
    .hidden_layer = {
        .weights = hidden_weights,
        .biases = hidden_biases,
        .input_size = MODEL_INPUT_SIZE,
        .output_size = MODEL_HIDDEN_SIZE,
        .input_offset = HIDDEN_INPUT_OFFSET,
        .output_offset = HIDDEN_OUTPUT_OFFSET,
        .output_multiplier = HIDDEN_OUTPUT_MULTIPLIER,
        .output_shift = HIDDEN_OUTPUT_SHIFT,
        .activation_min = HIDDEN_ACTIVATION_MIN,
        .activation_max = HIDDEN_ACTIVATION_MAX
    },
    .output_layer = {
        .weights = output_weights,
        .biases = output_biases,
        .input_size = MODEL_HIDDEN_SIZE,
        .output_size = MODEL_OUTPUT_SIZE,
        .input_offset = OUTPUT_INPUT_OFFSET,
        .output_offset = OUTPUT_OUTPUT_OFFSET,
        .output_multiplier = OUTPUT_OUTPUT_MULTIPLIER,
        .output_shift = OUTPUT_OUTPUT_SHIFT,
        .activation_min = OUTPUT_ACTIVATION_MIN,
        .activation_max = OUTPUT_ACTIVATION_MAX
    }
};

// Test data for XOR operation
// Input format: [input1, input2] where -127 = -1.0, 127 = 1.0
const int8_t xor_test_inputs[4][2] = {
    {-127, -127},  // -1, -1 -> should output ~-127 (false)
    {-127,  127},  // -1,  1 -> should output ~ 127 (true)
    { 127, -127},  //  1, -1 -> should output ~ 127 (true)
    { 127,  127}   //  1,  1 -> should output ~-127 (false)
};

// Expected outputs for XOR operation
const int8_t xor_expected_outputs[4] = {
    -127,  // false
     127,  // true
     127,  // true
    -127   // false
};

// Function to get model data as binary blob (for testing)
const uint8_t* get_simple_xor_model_data(size_t* size) {
    *size = sizeof(simple_xor_model);
    return (const uint8_t*)&simple_xor_model;
}

// Function to get test input data
const int8_t* get_xor_test_input(int test_case) {
    if (test_case >= 0 && test_case < 4) {
        return xor_test_inputs[test_case];
    }
    return NULL;
}

// Function to get expected output
int8_t get_xor_expected_output(int test_case) {
    if (test_case >= 0 && test_case < 4) {
        return xor_expected_outputs[test_case];
    }
    return 0;
}

// Function to validate output
int validate_xor_output(int test_case, int8_t actual_output) {
    int8_t expected = get_xor_expected_output(test_case);
    // Allow for some quantization error (±20)
    int diff = actual_output - expected;
    return (diff >= -20 && diff <= 20) ? 1 : 0;
}