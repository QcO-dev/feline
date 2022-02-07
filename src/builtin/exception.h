#pragma once
#include "../common.h"

typedef struct VM VM;

typedef enum InternalExceptionType {
	INTERNAL_EXCEPTION_BASE,
	INTERNAL_EXCEPTION__COUNT
} InternalExceptionType;

void defineExceptionClasses(VM* vm);