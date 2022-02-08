#pragma once
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#define UINT8_COUNT (UINT8_MAX + 1)

#ifdef _DEBUG

#include <stdio.h>

#define ASSERT(condition, message) do { if(!(condition)) { fprintf(stderr, "[%s:%d (in %s)] ASSERTION FAILURE: %s", __FILE__, __LINE__, __func__, (message)); exit(1); } } while(0)
#define TODO(message) do { fprintf(stderr, "[%s:%d (in %s)] TODO: %s\n", __FILE__, __LINE__, __func__, message); exit(1); } while(0)

//#define FELINE_DEBUG_DISASSEMBLE
//#define FELINE_DEBUG_TRACE_INSTRUCTIONS
//#define FELINE_DEBUG_STRESS_GC
//#define FELINE_DEBUG_LOG_GC

#else

#define ASSERT(condition, message)
#define TODO(message)

#endif // _DEBUG