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

#endif

typedef struct VM VM;
typedef struct Obj Obj;
typedef struct ObjClass ObjClass;
typedef struct ObjInstance ObjInstance;

typedef enum ValueType {
	VAL_BOOL,
	VAL_NULL,
	VAL_NUMBER,
	VAL_OBJ
} ValueType;

typedef struct Value {
	ValueType type;
	union {
		bool boolean;
		double number;
		Obj* obj;
	} as;
} Value;

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

typedef Value(*NativeFunction)(VM* vm, uint8_t argCount, Value* value);

#define BOOL_VAL(value) ((Value){VAL_BOOL, { .boolean = value }})
#define NULL_VAL ((Value){VAL_NULL, { .number = 0 }})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, { .number = value }})
#define OBJ_VAL(object) ((Value){VAL_OBJ, { .obj = (Obj*)object }})

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value) ((value).as.obj)

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NULL(value) ((value).type == VAL_NULL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJ(value) ((value).type == VAL_OBJ)

ObjClass* (*feline_getInternalException)(VM* vm, InternalExceptionType type);
void (*feline_throwException)(VM* vm, ObjClass* exceptionType, const char* format, ...);
bool (*feline_isInstance)(Value value);
ObjInstance* (*feline_asInstance)(Value value);
bool (*feline_getInstanceField)(VM* vm, ObjInstance* instance, const char* name, Value* value);
bool (*feline_setInstanceField)(VM* vm, ObjInstance* instance, const char* name, Value value);

#ifdef _WIN32
HINSTANCE hostFelineApp;

#ifndef FELINE_IMPLEMENT_ONCE
#define FELINE_IMPLEMENT_ONCE

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
	if (fdwReason == DLL_PROCESS_ATTACH) {
		hostFelineApp = GetModuleHandle(NULL);

		feline_getInternalException = (ObjClass * (*)(VM*, InternalExceptionType))GetProcAddress(hostFelineApp, "export_getInternalException");
		feline_throwException = (void (*)(VM*, ObjClass*, const char*, ...))GetProcAddress(hostFelineApp, "throwException");
		feline_isInstance = (bool (*)(Value))GetProcAddress(hostFelineApp, "export_isInstance");
		feline_asInstance = (ObjInstance * (*)(Value))GetProcAddress(hostFelineApp, "export_asInstance");
		feline_getInstanceField = (bool (*)(VM*, ObjInstance*, const char*, Value*))GetProcAddress(hostFelineApp, "export_getInstanceField");
		feline_setInstanceField = (bool (*)(VM*, ObjInstance*, const char*, Value))GetProcAddress(hostFelineApp, "export_setInstanceField");
	}
	return TRUE;
}

#endif

#endif