#include "../include/tflm_c_interface.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include <cstring>

// Internal context structure
struct tflm_context {
    tflite::MicroInterpreter* interpreter;
    tflite::MicroMutableOpResolver<10>* resolver;
    const tflite::Model* model;
    uint8_t* tensor_arena;
    size_t arena_size;
};

// Global initialization state
static bool g_tflm_initialized = false;

// Error message strings
static const char* error_strings[] = {
    "Success",
    "Invalid argument",
    "Memory allocation failed",
    "Model loading failed",
    "Interpreter creation failed",
    "Tensor allocation failed",
    "Inference failed",
    "Unknown error"
};

extern "C" {

tflm_status_t tflm_init(void) {
    if (g_tflm_initialized) {
        return TFLM_ERROR_ALREADY_INITIALIZED;
    }
    
    // Initialize any global TFLM state if needed
    g_tflm_initialized = true;
    return TFLM_SUCCESS;
}

tflm_status_t tflm_deinit(void) {
    if (!g_tflm_initialized) {
        return TFLM_ERROR_NOT_INITIALIZED;
    }
    g_tflm_initialized = false;
    return TFLM_SUCCESS;
}

tflm_context_t* tflm_create_context(const unsigned char* model_data, 
                                   size_t model_size,
                                   uint8_t* tensor_arena, 
                                   size_t arena_size) {
    if (!model_data || !tensor_arena || arena_size == 0) {
        // 인자가 유효하지 않음
        return nullptr;
    }

    // Allocate context
    tflm_context_t* context = new(std::nothrow) tflm_context_t();
    if (!context) {
        // 메모리 할당 실패
        return nullptr;
    }

    // Initialize context
    context->tensor_arena = tensor_arena;
    context->arena_size = arena_size;

    // Load model
    context->model = tflite::GetModel(model_data);
    if (context->model->version() != TFLITE_SCHEMA_VERSION) {
        delete context;
        // 모델 버전이 맞지 않음
        return nullptr;
    }

    // Create resolver with common operations
    context->resolver = new(std::nothrow) tflite::MicroMutableOpResolver<10>();
    if (!context->resolver) {
        delete context;
        return nullptr;
    }

    // Add common operations that most models use
    context->resolver->AddFullyConnected();
    context->resolver->AddConv2D();
    context->resolver->AddDepthwiseConv2D();
    context->resolver->AddReshape();
    context->resolver->AddSoftmax();
    context->resolver->AddAdd();
    context->resolver->AddRelu();
    context->resolver->AddMaxPool2D();
    context->resolver->AddAveragePool2D();

    // Create interpreter
    context->interpreter = new(std::nothrow) tflite::MicroInterpreter(
        context->model, *context->resolver, tensor_arena, arena_size);
    
    if (!context->interpreter) {
        delete context->resolver;
        delete context;
        return nullptr;
    }

    // Allocate tensors
    if (context->interpreter->AllocateTensors() != kTfLiteOk) {
        delete context->interpreter;
        delete context->resolver;
        delete context;
        return nullptr;
    }

    return context;
}

void tflm_destroy_context(tflm_context_t* context) {
    if (!context) {
        return;
    }

    delete context->interpreter;
    delete context->resolver;
    delete context;
}

tflm_status_t tflm_get_input_tensor_info(tflm_context_t* context,
                                        int index,
                                        int* dims,
                                        int* num_dims,
                                        size_t* bytes) {
    if (!context || !context->interpreter || !dims || !num_dims || !bytes) {
        return TFLM_ERROR_CONTEXT_NULL;
    }

    TfLiteTensor* tensor = context->interpreter->input(index);
    if (!tensor) {
        return TFLM_ERROR_TENSOR_NULL;
    }

    *num_dims = tensor->dims->size;
    for (int i = 0; i < *num_dims && i < 4; i++) {
        dims[i] = tensor->dims->data[i];
    }
    
    *bytes = tensor->bytes;
    return TFLM_SUCCESS;
}

tflm_status_t tflm_get_output_tensor_info(tflm_context_t* context,
                                         int index,
                                         int* dims,
                                         int* num_dims,
                                         size_t* bytes) {
    if (!context || !context->interpreter || !dims || !num_dims || !bytes) {
        return TFLM_ERROR_CONTEXT_NULL;
    }

    TfLiteTensor* tensor = context->interpreter->output(index);
    if (!tensor) {
        return TFLM_ERROR_TENSOR_NULL;
    }

    *num_dims = tensor->dims->size;
    for (int i = 0; i < *num_dims && i < 4; i++) {
        dims[i] = tensor->dims->data[i];
    }
    
    *bytes = tensor->bytes;
    return TFLM_SUCCESS;
}

float* tflm_get_input_tensor(tflm_context_t* context, int index) {
    if (!context || !context->interpreter) {
        return nullptr;
    }

    TfLiteTensor* tensor = context->interpreter->input(index);
    return tensor ? reinterpret_cast<float*>(tensor->data.data) : nullptr;
}

float* tflm_get_output_tensor(tflm_context_t* context, int index) {
    if (!context || !context->interpreter) {
        return nullptr;
    }

    TfLiteTensor* tensor = context->interpreter->output(index);
    return tensor ? reinterpret_cast<float*>(tensor->data.data) : nullptr;
}

tflm_status_t tflm_invoke(tflm_context_t* context) {
    if (!context || !context->interpreter) {
        return TFLM_ERROR_CONTEXT_NULL;
    }

    TfLiteStatus status = context->interpreter->Invoke();
    return (status == kTfLiteOk) ? TFLM_SUCCESS : TFLM_ERROR_INFERENCE_FAILED;
}

const char* tflm_get_error_string(tflm_status_t status) {
    int index = -static_cast<int>(status);
    if (index >= 0 && index < 8) {
        return error_strings[index];
    }
    return "Unknown error";
}

} // extern "C"