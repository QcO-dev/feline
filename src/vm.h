#pragma once
#include "value.h"
#include "chunk.h"
#include "table.h"
#include "object.h"

typedef struct Compiler Compiler;

typedef struct CallFrame {
	ObjClosure* closure;
	uint8_t* ip;
	size_t slotsOffset;
} CallFrame;

DECLARE_DYNAMIC_ARRAY(CallFrame, CallFrame)

typedef struct VM {
	ValueArray stack;
	CallFrameArray frames;

	Table globals;
	Table strings;

	Value exception;
	bool hasException;

	ObjString* newString;

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

InterpreterResult executeVM(VM* vm);
InterpreterResult interpret(VM* vm, const char* source);