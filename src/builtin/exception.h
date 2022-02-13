#pragma once
#include "../common.h"
#include "../module.h"

typedef struct VM VM;

typedef enum InternalExceptionType {
	INTERNAL_EXCEPTION_BASE,
	INTERNAL_EXCEPTION_TYPE,
	INTERNAL_EXCEPTION_ARITY,
	INTERNAL_EXCEPTION_PROPERTY,
	INTERNAL_EXCEPTION_INDEX_RANGE,
	INTERNAL_EXCEPTION_UNDEFINED_VARIABLE,
	INTERNAL_EXCEPTION_STACK_OVERFLOW,
	INTERNAL_EXCEPTION_LINK_FAILURE,
	INTERNAL_EXCEPTION__COUNT
} InternalExceptionType;

void defineExceptionClasses(VM* vm);
void bindExceptionClasses(VM* vm, Module* mod);