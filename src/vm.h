#pragma once
#include "value.h"
#include "chunk.h"
#include "table.h"
#include "object.h"
#include "module.h"
#include "builtin/exception.h"
#include "ffi/felineffi.h"

typedef struct Compiler Compiler;

typedef struct CallFrame {
	ObjClosure* closure;
	uint8_t* ip;
	size_t slotsOffset;

	uint8_t* catchLocation;
	size_t tryStackOffset;
	bool isTryBlock;
} CallFrame;

DECLARE_DYNAMIC_ARRAY(CallFrame, CallFrame)

typedef enum InternalString {
	INTERNAL_STR_NEW,
	INTERNAL_STR_STACKTRACE,
	INTERNAL_STR_EXCEPTION,
	INTERNAL_STR_TYPE_EXCEPTION,
	INTERNAL_STR_ARITY_EXCEPTION,
	INTERNAL_STR_PROPERTY_EXCEPTION,
	INTERNAL_STR_INDEX_RANGE_EXCEPTION,
	INTERNAL_STR_UNDEFINED_VARIABLE_EXCEPTION,
	INTERNAL_STR_STACK_OVERFLOW_EXCEPTION,
	INTERNAL_STR_LINK_FAILURE_EXCEPTION,
	INTERNAL_STR_VALUE_EXCEPTION,
	INTERNAL_STR_REASON,
	INTERNAL_STR_OBJECT,
	INTERNAL_STR_IMPORT,
	INTERNAL_STR_THIS_MODULE,
	INTERNAL_STR__COUNT
} InternalString;

typedef enum InternalClassType {
	INTERNAL_CLASS_OBJECT,
	INTERNAL_CLASS_IMPORT,
	INTERNAL_CLASS__COUNT
} InternalClassType;

typedef struct VM {
	ValueArray stack;
	CallFrameArray frames;

	Table strings;
	Table nativeLibraries;
	Table imports;

	Table listMethods;

	Value exception;
	bool hasException;

	Module* modules;
	ObjString* baseDirectory;

	ObjString* internalStrings[INTERNAL_STR__COUNT];
	ObjClass* internalExceptions[INTERNAL_EXCEPTION__COUNT];
	ObjClass* internalClasses[INTERNAL_CLASS__COUNT];

	Compiler* lowestLevelCompiler;
	ObjUpvalue* openUpvalues;
	
	size_t bytesAllocated;
	size_t nextGC;
	
	Obj* objects;
	size_t grayCount;
	size_t grayCapacity;
	Obj** grayStack;
} VM;

typedef enum InterpreterResult {
	INTERPRETER_OK,
	INTERPRETER_COMPILE_ERROR,
	INTERPRETER_RUNTIME_ERROR
} InterpreterResult;

void initVM(VM* vm);
void freeVM(VM* vm);

void push(VM* vm, Value value);
Value peek(VM* vm, size_t distance);
Value pop(VM* vm);
FELINE_EXPORT void throwException(VM* vm, ObjClass* exceptionType, const char* format, ...);

void inheritClasses(VM* vm, ObjClass* subclass, ObjClass* superclass);

InterpreterResult executeVM(VM* vm, size_t baseFrameIndex);
InterpreterResult interpret(VM* vm, const char* source);