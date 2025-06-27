/* Copyright 2022 Sipeed Technology Co., Ltd. All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef __TM_PORT_H
#define __TM_PORT_H

#define TM_ARCH_CPU         (0) //default, pure cpu compute
#define TM_ARCH_ARM_SIMD    (1) //ARM Cortex M4/M7, etc.
#define TM_ARCH_ARM_NEON    (2) //ARM Cortex A7, etc.
#define TM_ARCH_ARM_MVEI    (3) //ARMv8.1: M55, etc.
#define TM_ARCH_RV32P       (4) //T-head E907, etc.
#define TM_ARCH_RV64V       (5) //T-head C906,C910, etc.
#define TM_ARCH_CSKYV2      (6) //cskyv2 with dsp core
#define TM_ARCH_X86_SSE2    (7) //x86 sse2

#define TM_OPT0             (0) //default, least code and buf
#define TM_OPT1             (1) //opt for speed, need more code and buf
#define TM_OPT2             (2) //TODO

/******************************* PORT CONFIG FOR TFM  ************************************/
#define TM_ARCH         TM_ARCH_CPU
#define TM_OPT_LEVEL    TM_OPT0
#define TM_MDL_TYPE     TM_MDL_INT8
#define TM_FASTSCALE    (1)         //enable on MCU without FPU for speed
#define TM_LOCAL_MATH   (1)         //use local math func to avoid libm dependencies
#define TM_ENABLE_STAT  (0)         //disable mdl stat functions to save memory
#define TM_MAX_CSIZE    (16)        //max channel num - minimal for MNIST (was 1000)
#define TM_MAX_KSIZE    (9)         //max kernel_size 3x3 (was 5*5)  
#define TM_MAX_KCSIZE   (144)       //max kernel_size*channels 3*3*16 (was 3*3*256)

#define TM_INLINE       __attribute__((always_inline)) static inline
#define TM_WEAK         __attribute__((weak))

// TF-M secure partition: NO dynamic memory allocation!
// Use static buffers only to prevent malloc() calls
#define tm_malloc(x)    NULL  
#define tm_free(x)      do {} while(0)

// Minimal logging for TFM - use TFM logging macros
#define TM_PRINTF(...) 
#define TM_DBG(...)    
#define TM_DBGL()

// Define FLT_MAX to avoid float.h dependency 
#ifndef FLT_MAX
#define FLT_MAX 3.402823466e+38F
#endif

// Prevent inclusion of problematic headers
#define _FLOAT_H_
#define _MATH_H_      

// Define local exp function to avoid libm dependency
static inline float tm_exp_local(float x) {
    // Fast approximate exp function for embedded use
    // Based on: http://www.machinedlearnings.com/2011/06/fast-approximate-logarithm-exponential.html
    float p = 1.442695040f * x;
    uint32_t i = 0;
    uint32_t sign = (i >> 31);
    int w = (int) p;
    float z = p - (float) w + (float) sign;
    union {
        uint32_t i;
        float f;
    } v = {.i = (uint32_t) ((1 << 23) * (p + 121.2740838f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z))};
    return v.f;
}

// Override tm_exp macro to use our local implementation
#define tm_exp tm_exp_local
#define exp tm_exp_local      

/******************************* DBG TIME CONFIG - DISABLED ************************************/
// Disable debug timing features to save memory and dependencies  
#define TM_DBGT_INIT()     do { } while(0)
#define TM_DBGT_START()    do { } while(0)
#define TM_DBGT(...)       do { } while(0)
#define TM_GET_US()        0

/******************************* DBG PERFORMANCE CONFIG - DISABLED ************************************/
// Disable performance monitoring to save memory and dependencies
#define TM_EN_PERF 0
#define TM_GET_TICK(x)
#define TM_TICK_PERUS
#define TM_PERF_REG(x)
#define TM_PERF_EXTREG(x)
#define TM_PERF_INIT(x)
#define TM_PERF_START(x)
#define TM_PERF_ADD(x)
#define TM_PERF_PRINT(x)

#endif
