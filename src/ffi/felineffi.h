#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
#include <Windows.h>

#ifdef __cplusplus
#define FELINE_EXPORT extern "C" __declspec(dllexport)

#define FELINE_IMPORT extern "C" __declspec(dllimport)

#else
#define FELINE_EXPORT __declspec(dllexport)

#define FELINE_IMPORT  __declspec(dllimport)
#endif

typedef struct InstanceData {
	void(*freeData)(struct InstanceData*);
} InstanceData;

#endif