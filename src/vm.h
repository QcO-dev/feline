#pragma once
#include "value.h"
#include "chunk.h"
#include "table.h"
#include "object.h"
#include "builtin/exception.h"

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
	INTERNAL_STR_REASON,
	INTERNAL_STR__COUNT
} InternalString;

typedef struct VM {
	ValueArray stack;
	CallFrameArray frames;

	Table globals;
	Table strings;

	Value exception;
	bool hasException;

	ObjString* internalStrings[INTERNAL_STR__COUNT];
	ObjClass* internalExceptions[INTERNAL_EXCEPTION__COUNT];

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
Value pop(VM* vm);

void inheritClasses(VM* vm, ObjClass* subclass, ObjClass* superclass);

InterpreterResult executeVM(VM* vm);
InterpreterResult interpret(VM* vm, const char* source);