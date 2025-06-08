#include "tflm_c_api.h"

// TFLM C++ headers
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include <cstring>

// C++ wrapper structure
struct tflm_interpreter {
    const tflite::Model* model;
    tflite::MicroInterpreter* interpreter;
    tflite::MicroErrorReporter error_reporter;
    tflite::MicroMutableOpResolver<16> op_resolver;
    uint8_t* tensor_arena;
    size_t tensor_arena_size;
    bool initialized;
};

extern "C" {

tflm_status_t tflm_create_interpreter(const uint8_t* model_data, 
                                      size_t model_size,
                                      uint8_t* tensor_arena,
                                      size_t tensor_arena_size,
                                      tflm_interpreter_t** interpreter) {
    if (!model_data || !tensor_arena || !interpreter) {
        return TFLM_ERROR_INVALID_ARGUMENT;
    }

    // Allocate interpreter structure
    tflm_interpreter_t* interp = new(std::nothrow) tflm_interpreter_t();
    if (!interp) {
        return TFLM_ERROR_INSUFFICIENT_MEMORY;
    }

    // Initialize structure
    interp->model = nullptr;
    interp->interpreter = nullptr;
    interp->tensor_arena = tensor_arena;
    interp->tensor_arena_size = tensor_arena_size;
    interp->initialized = false;

    // Parse model
    interp->model = tflite::GetModel(model_data);
    if (!interp->model) {
        delete interp;
        return TFLM_ERROR_INVALID_ARGUMENT;
    }

    // Check schema version
    if (interp->model->version() != TFLITE_SCHEMA_VERSION) {
        delete interp;
        return TFLM_ERROR_NOT_SUPPORTED;
    }

    // Add common operations to resolver
    interp->op_resolver.AddFullyConnected();
    interp->op_resolver.AddQuantize();
    interp->op_resolver.AddDequantize();
    interp->op_resolver.AddReshape();
    interp->op_resolver.AddSoftmax();
    interp->op_resolver.AddConv2D();
    interp->op_resolver.AddDepthwiseConv2D();
    interp->op_resolver.AddAveragePool2D();
    interp->op_resolver.AddMaxPool2D();
    interp->op_resolver.AddAdd();
    interp->op_resolver.AddMul();
    interp->op_resolver.AddLogistic();
    interp->op_resolver.AddTanh();
    interp->op_resolver.AddRelu();
    interp->op_resolver.AddRelu6();
    
    // Add basic operations that are commonly available
    // Note: Only adding operations that are confirmed to be available in the build

    // Create interpreter using placement new for safe allocation
    interp->interpreter = new(std::nothrow) tflite::MicroInterpreter(
        interp->model, interp->op_resolver, interp->tensor_arena,
        interp->tensor_arena_size);
    
    if (!interp->interpreter) {
        delete interp;
        return TFLM_ERROR_INSUFFICIENT_MEMORY;
    }

    // Allocate tensors
    TfLiteStatus allocate_status = interp->interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        delete interp->interpreter;
        delete interp;
        return TFLM_ERROR_INSUFFICIENT_MEMORY;
    }

    interp->initialized = true;
    *interpreter = interp;
    
    return TFLM_OK;
}

tflm_status_t tflm_destroy_interpreter(tflm_interpreter_t* interpreter) {
    if (!interpreter) {
        return TFLM_ERROR_INVALID_ARGUMENT;
    }
    
    // Clean up the MicroInterpreter if it was allocated
    if (interpreter->interpreter) {
        delete interpreter->interpreter;
        interpreter->interpreter = nullptr;
    }
    
    delete interpreter;
    return TFLM_OK;
}

tflm_status_t tflm_get_input_size(tflm_interpreter_t* interpreter, size_t* size) {
    if (!interpreter || !interpreter->initialized || !size) {
        return TFLM_ERROR_INVALID_ARGUMENT;
    }

    TfLiteTensor* input_tensor = interpreter->interpreter->input(0);
    if (!input_tensor) {
        return TFLM_ERROR_GENERIC;
    }

    *size = input_tensor->bytes;
    return TFLM_OK;
}

tflm_status_t tflm_get_output_size(tflm_interpreter_t* interpreter, size_t* size) {
    if (!interpreter || !interpreter->initialized || !size) {
        return TFLM_ERROR_INVALID_ARGUMENT;
    }

    TfLiteTensor* output_tensor = interpreter->interpreter->output(0);
    if (!output_tensor) {
        return TFLM_ERROR_GENERIC;
    }

    *size = output_tensor->bytes;
    return TFLM_OK;
}

tflm_status_t tflm_set_input_data(tflm_interpreter_t* interpreter, 
                                  const uint8_t* input_data, 
                                  size_t input_size) {
    if (!interpreter || !interpreter->initialized || !input_data) {
        return TFLM_ERROR_INVALID_ARGUMENT;
    }

    TfLiteTensor* input_tensor = interpreter->interpreter->input(0);
    if (!input_tensor) {
        return TFLM_ERROR_GENERIC;
    }

    if (input_size != input_tensor->bytes) {
        return TFLM_ERROR_INVALID_ARGUMENT;
    }

    memcpy(input_tensor->data.raw, input_data, input_size);
    return TFLM_OK;
}

tflm_status_t tflm_get_output_data(tflm_interpreter_t* interpreter, 
                                   uint8_t* output_data, 
                                   size_t output_size) {
    if (!interpreter || !interpreter->initialized || !output_data) {
        return TFLM_ERROR_INVALID_ARGUMENT;
    }

    TfLiteTensor* output_tensor = interpreter->interpreter->output(0);
    if (!output_tensor) {
        return TFLM_ERROR_GENERIC;
    }

    size_t copy_size = (output_size < output_tensor->bytes) ? 
                       output_size : output_tensor->bytes;
    
    memcpy(output_data, output_tensor->data.raw, copy_size);
    return TFLM_OK;
}

tflm_status_t tflm_invoke(tflm_interpreter_t* interpreter) {
    if (!interpreter || !interpreter->initialized) {
        return TFLM_ERROR_INVALID_ARGUMENT;
    }

    TfLiteStatus invoke_status = interpreter->interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        return TFLM_ERROR_INFERENCE_FAILED;
    }

    return TFLM_OK;
}

const char* tflm_status_string(tflm_status_t status) {
    switch (status) {
        case TFLM_OK: return "Success";
        case TFLM_ERROR_INVALID_ARGUMENT: return "Invalid argument";
        case TFLM_ERROR_INSUFFICIENT_MEMORY: return "Insufficient memory";
        case TFLM_ERROR_NOT_SUPPORTED: return "Not supported";
        case TFLM_ERROR_GENERIC: return "Generic error";
        case TFLM_ERROR_MODEL_NOT_LOADED: return "Model not loaded";
        case TFLM_ERROR_INFERENCE_FAILED: return "Inference failed";
        default: return "Unknown error";
    }
}

} // extern "C"