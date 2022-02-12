#include "vm.h"
#include "compiler.h"
#include "object.h"
#include "memory.h"
#include "builtin/objectclass.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#ifdef FELINE_DEBUG_TRACE_INSTRUCTIONS
#include "disassemble.h"
#endif

DEFINE_DYNAMIC_ARRAY(CallFrame, CallFrame)

// TODO: Move the native stuff out of vm.c

#include <time.h>

Value peek(VM* vm, size_t distance);

static void defineNative(VM* vm, const char* name, NativeFunction function, size_t arity) {
	push(vm, OBJ_VAL(copyString(vm, name, strlen(name))));
	push(vm, OBJ_VAL(newNative(vm, function, arity)));
	tableSet(vm, &vm->globals, AS_STRING(peek(vm, 1)), peek(vm, 0));
	pop(vm);
	pop(vm);
}

static Value clockNative(VM* vm, uint8_t argCount, Value* args) {
	return NUMBER_VAL((double)clock() / 1000);
}


//TODO:
//  len() is temporary until we can access methods on lists & strings :-)
static Value lenNative(VM* vm, uint8_t argCount, Value* args) {
	if (IS_LIST(args[0])) {
		return NUMBER_VAL((double)AS_LIST(args[0])->items.length);
	}
	else if (IS_STRING(args[0])) {
		return NUMBER_VAL((double)AS_STRING(args[0])->length);
	}
	
	throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Expected argument to be a list or string");
	return NULL_VAL;
}

// TODO: Remove all above until the over to-do

void buildInternalStrings(VM* vm) {
	vm->internalStrings[INTERNAL_STR_NEW] = copyString(vm, "new", 3);
	vm->internalStrings[INTERNAL_STR_STACKTRACE] = copyString(vm, "stackTrace", 10);
	vm->internalStrings[INTERNAL_STR_EXCEPTION] = copyString(vm, "Exception", 9);

	vm->internalStrings[INTERNAL_STR_TYPE_EXCEPTION] = copyString(vm, "TypeException", 13);
	vm->internalStrings[INTERNAL_STR_ARITY_EXCEPTION] = copyString(vm, "ArityException", 14);
	vm->internalStrings[INTERNAL_STR_PROPERTY_EXCEPTION] = copyString(vm, "PropertyException", 17);
	vm->internalStrings[INTERNAL_STR_INDEX_RANGE_EXCEPTION] = copyString(vm, "IndexRangeException", 19);
	vm->internalStrings[INTERNAL_STR_UNDEFINED_VARIABLE_EXCEPTION] = copyString(vm, "UndefinedVariableException", 26);
	vm->internalStrings[INTERNAL_STR_STACK_OVERFLOW_EXCEPTION] = copyString(vm, "StackOverflowException", 22);
	vm->internalStrings[INTERNAL_STR_LINK_FAILURE_EXCEPTION] = copyString(vm, "LinkFailureException", 20);

	vm->internalStrings[INTERNAL_STR_REASON] = copyString(vm, "reason", 6);

	vm->internalStrings[INTERNAL_STR_OBJECT] = copyString(vm, "Object", 6);
}

void initVM(VM* vm) {
	vm->objects = NULL;

	vm->bytesAllocated = 0;
	vm->nextGC = 1024 * 1024;

	vm->grayCount = 0;
	vm->grayCapacity = 0;
	vm->grayStack = NULL;

	vm->openUpvalues = NULL;
	vm->lowestLevelCompiler = NULL;
	
	for (size_t i = 0; i < INTERNAL_STR__COUNT; i++) vm->internalStrings[i] = NULL;
	for (size_t i = 0; i < INTERNAL_EXCEPTION__COUNT; i++) vm->internalExceptions[i] = NULL;
	for (size_t i = 0; i < INTERNAL_CLASS__COUNT; i++) vm->internalClasses[i] = NULL;

	vm->hasException = false;
	vm->exception = NULL_VAL;
	initValueArray(&vm->stack);
	initCallFrameArray(&vm->frames);
	initTable(&vm->strings);
	initTable(&vm->globals);
	initTable(&vm->nativeLibraries);

	// Forces the stack to resize before anything else is allocated
	// Allows for push() and pop() to be used to stop values being GC'd.
	push(vm, NULL_VAL);
	pop(vm);

	buildInternalStrings(vm);

	defineObjectClass(vm);

	defineExceptionClasses(vm);

	defineNative(vm, "clock", clockNative, 0);
	defineNative(vm, "len", lenNative, 1);
}

void freeVM(VM* vm) {
	freeTable(vm, &vm->strings);
	freeTable(vm, &vm->globals);
	freeTable(vm, &vm->nativeLibraries);
	freeValueArray(vm, &vm->stack);
	freeCallFrameArray(vm, &vm->frames);
	freeObjects(vm);
}

void push(VM* vm, Value value) {
	writeValueArray(vm, &vm->stack, value);
}

Value pop(VM* vm) {
	return vm->stack.items[--vm->stack.length];
}

Value peek(VM* vm, size_t distance) {
	return vm->stack.items[vm->stack.length - 1 - distance];
}

void inheritClasses(VM* vm, ObjClass* subclass, ObjClass* superclass) {
	tableAddAll(vm, &superclass->methods, &subclass->methods);
	subclass->superclass = superclass;
}

bool instanceof(ObjInstance* instance, ObjClass* clazz) {
	ObjClass* parent = instance->clazz;
	while (parent != NULL) {
		if (parent == clazz) {
			return true;
		}
		parent = parent->superclass;
	}
	return false;
}

static void resetStack(VM* vm) {
	vm->stack.length = 0;
	vm->openUpvalues = NULL;
}

void throwException(VM* vm, ObjClass* exceptionType, const char* format, ...) {
	va_list args;
	va_start(args, format);
	ObjString* reason = makeStringvf(vm, format, args);
	va_end(args);

	push(vm, OBJ_VAL(reason));

	//TODO:
	//  Actually call the Exception constructor (currently does not exist)
	ObjInstance* exception = newInstance(vm, exceptionType);

	push(vm, OBJ_VAL(exception));

	tableSet(vm, &exception->fields, vm->internalStrings[INTERNAL_STR_REASON], peek(vm, 1));

	vm->exception = OBJ_VAL(exception);
	vm->hasException = true;
	
	pop(vm);
	pop(vm);
}

static ObjString* concatenate(VM* vm) {
	ObjString* b = AS_STRING(peek(vm, 0));
	ObjString* a = AS_STRING(peek(vm, 1));

	size_t length = a->length + b->length;

	char* str = ALLOCATE(vm, char, length + 1);

	memcpy(str, a->str, a->length);
	memcpy(str + a->length, b->str, b->length);
	str[length] = '\0';

	ObjString* result = takeString(vm, str, length);
	pop(vm);
	pop(vm);
	return result;
}

static bool callClosure(VM* vm, ObjClosure* closure, uint8_t argCount) {

	if (argCount != closure->function->arity) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_ARITY], "Expected %d arguments but got %d.", closure->function->arity, argCount);
		return false;
	}

	// TODO: Move this magic number somewhere else
	// Despite the VM having a dynamic array of frames,
	// We want to have stack overflows happen after a reasonable amount
	if (vm->frames.length == 1024) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_STACK_OVERFLOW], "Stack Overflow (%d frames)", 1024);
		return false;
	}

	// Add a new call frame -> the frame stack may be resized
	writeCallFrameArray(vm, &vm->frames, (CallFrame) { 0 });

	CallFrame* frame = &vm->frames.items[vm->frames.length - 1];
	frame->closure = closure;
	frame->ip = closure->function->chunk.bytecode.items;
	frame->slotsOffset = vm->stack.length - argCount - 1;
	frame->isTryBlock = false;
	frame->catchLocation = NULL;
	frame->tryStackOffset = 0;
	return true;
}

static bool callValue(VM* vm, Value callee, uint8_t argCount) {
	if (IS_OBJ(callee)) {
		switch (OBJ_TYPE(callee)) {
			case OBJ_CLASS: {
				ObjClass* clazz = AS_CLASS(callee);
				vm->stack.items[vm->stack.length - argCount - 1] = OBJ_VAL(newInstance(vm, clazz));

				Value initializer;
				if (tableGet(&clazz->methods, vm->internalStrings[INTERNAL_STR_NEW], &initializer)) {
					return callClosure(vm, AS_CLOSURE(initializer), argCount);
				}
				else if (argCount != 0) {
					throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_ARITY], "Expected 0 arguments but got %d", argCount);
					return false;
				}

				return true;
			}
			case OBJ_BOUND_METHOD: {
				ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
				vm->stack.items[vm->stack.length - argCount - 1] = bound->receiver;
				return callClosure(vm, bound->method, argCount);
			}
			case OBJ_CLOSURE: {
				return callClosure(vm, AS_CLOSURE(callee), argCount);
			}
			case OBJ_NATIVE: {
				ObjNative* nativeObj = AS_NATIVE_OBJ(callee);
				NativeFunction native = AS_NATIVE(callee);

				if (argCount != nativeObj->arity) {
					throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_ARITY], "Expected %d arguments but got %d", nativeObj->arity, argCount);
					return false;
				}

				Value result = native(vm, argCount, &vm->stack.items[vm->stack.length - argCount]);
				vm->stack.length -= (size_t)argCount + 1;
				
				push(vm, result);
				return true;
			}
			default: break; // Not a callable type
		}
	}
	throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Non-callable type");
	return false;
}

static ObjUpvalue* captureUpvalue(VM* vm, Value* local) {
	ObjUpvalue* prevUpvalue = NULL;
	ObjUpvalue* upvalue = vm->openUpvalues;

	while (upvalue != NULL && upvalue->location > local) {
		prevUpvalue = upvalue;
		upvalue = upvalue->next;
	}

	if (upvalue != NULL && upvalue->location == local) {
		return upvalue;
	}

	ObjUpvalue* createdUpvalue = newUpvalue(vm, local);

	createdUpvalue->next = upvalue;

	if (prevUpvalue == NULL) {
		vm->openUpvalues = createdUpvalue;
	}
	else {
		prevUpvalue->next = createdUpvalue;
	}

	return createdUpvalue;
}

static void closeUpvalues(VM* vm, Value* last) {
	while (vm->openUpvalues != NULL && vm->openUpvalues->location >= last) {
		ObjUpvalue* upvalue = vm->openUpvalues;
		upvalue->closed = *upvalue->location;
		upvalue->location = &upvalue->closed;
		vm->openUpvalues = upvalue->next;
	}
}

static void defineMethod(VM* vm, ObjString* name) {
	Value method = peek(vm, 0);
	ObjClass* clazz = AS_CLASS(peek(vm, 1));
	tableSet(vm, &clazz->methods, name, method);
	pop(vm);
}

static bool bindMethod(VM* vm, ObjClass* clazz, ObjString* name) {
	Value method;

	if (!tableGet(&clazz->methods, name, &method)) {
		return false;
	}

	ObjBoundMethod* bound = newBoundMethod(vm, peek(vm, 0), AS_CLOSURE(method));

	pop(vm);
	push(vm, OBJ_VAL(bound));
	return true;
}

static bool invokeFromClass(VM* vm, ObjClass* clazz, ObjString* name, uint8_t argCount) {
	Value method;
	if (!tableGet(&clazz->methods, name, &method)) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_PROPERTY], "Undefined property '%s'", name->str);
		return false;
	}
	return callClosure(vm, AS_CLOSURE(method), argCount);
}

static bool invoke(VM* vm, ObjString* name, uint8_t argCount) {
	Value receiver = peek(vm, argCount);

	if (!IS_INSTANCE(receiver)) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Only instances have methods");
		return false;
	}

	ObjInstance* instance = AS_INSTANCE(receiver);

	Value value;
	if (tableGet(&instance->fields, name, &value)) {
		vm->stack.items[vm->stack.length - argCount - 1] = value;
		return callValue(vm, value, argCount);
	}

	return invokeFromClass(vm, instance->clazz, name, argCount);
}

static bool validateIndex(VM* vm, size_t length, double index, size_t* realIndex) {
	if (floor(index) != index) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_INDEX_RANGE], "List index must be an integer (got %g)", index);
		return false;
	}

	int64_t signedIndex = (int64_t)index;

	size_t absIndex = 0;

	absIndex = signedIndex < 0 ? length - -signedIndex : signedIndex;

	if (absIndex >= length || absIndex < 0) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_INDEX_RANGE], "List index '%" PRId64 "' out of range for list of length '%zu'", signedIndex, length);
		return false;
	}

	*realIndex = absIndex;

	return true;
}

InterpreterResult executeVM(VM* vm) {
	CallFrame* frame = &vm->frames.items[vm->frames.length - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.items[READ_SHORT()])
#define READ_STRING() (AS_STRING(READ_CONSTANT()))

#define BINARY_OP(valueType, op) do { \
	if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Operands must be numbers"); \
		break; \
	} \
	double b = AS_NUMBER(pop(vm)); \
	double a = AS_NUMBER(pop(vm)); \
	push(vm, valueType(a op b)); \
} while(0)

#ifdef FELINE_DEBUG_TRACE_INSTRUCTIONS
	printf("=== EXECUTION ===\n");
#endif

	for (;;) {

		if (vm->hasException) {
			ValueArray stackTraceArray;
			initValueArray(&stackTraceArray);

			ObjList* stackTrace = newList(vm, stackTraceArray);

			push(vm, OBJ_VAL(stackTrace));
			while (vm->hasException) {

				ObjFunction* function = frame->closure->function;
				size_t instruction = frame->ip - function->chunk.bytecode.items - 1;

				ObjString* tracer = makeStringf(vm, "[%zu] in %s", getLineOfInstruction(&function->chunk, instruction), function->name != NULL ? function->name->str : "<script>");
				push(vm, OBJ_VAL(tracer));
				writeValueArray(vm, &stackTrace->items, peek(vm, 0));
				pop(vm);

				if (frame->isTryBlock) {
					frame->ip = frame->catchLocation;
					frame->isTryBlock = false;
					frame->catchLocation = NULL;

					if (IS_INSTANCE(vm->exception) && instanceof(AS_INSTANCE(vm->exception), vm->internalExceptions[INTERNAL_EXCEPTION_BASE])) {
						tableSet(vm, &AS_INSTANCE(vm->exception)->fields, vm->internalStrings[INTERNAL_STR_STACKTRACE], peek(vm, 0));
					}

					pop(vm);
					vm->hasException = false;
					// Reset to have a stack effect of 0
					vm->stack.length = frame->tryStackOffset;
					break;
				}

				closeUpvalues(vm, &vm->stack.items[frame->slotsOffset]);

				vm->frames.length--;

				if (vm->frames.length == 0) {
					pop(vm); // Stack Trace
					pop(vm); // The script function
					
					if (IS_INSTANCE(vm->exception) && instanceof(AS_INSTANCE(vm->exception), vm->internalExceptions[INTERNAL_EXCEPTION_BASE])) {
						ObjInstance* exception = AS_INSTANCE(vm->exception);
						printf("%s: ", exception->clazz->name->str);

						Value reason;
						if (tableGet(&exception->fields, vm->internalStrings[INTERNAL_STR_REASON], &reason)) {
							printValue(vm, reason);
							printf("\n");
						}
						else {
							printf("Exception thrown without reason\n");
						}
					}
					else {
						printf("Exception: ");
						printValue(vm, vm->exception);
						printf("\n");
					}
					
					for (size_t i = 0; i < stackTrace->items.length; i++) {
						printf("%s\n", AS_CSTRING(stackTrace->items.items[i]));
					}

					return INTERPRETER_RUNTIME_ERROR;
				}

				vm->stack.length = frame->slotsOffset;
				// Repush after adjustment to keep on the stack
				push(vm, OBJ_VAL(stackTrace));

				frame = &vm->frames.items[vm->frames.length - 1];
			}
		}

#ifdef FELINE_DEBUG_TRACE_INSTRUCTIONS
		printf(" [ ");
		for (size_t i = 0; i < vm->stack.length; i++) {
			Value v = vm->stack.items[i];
			printValue(vm, v);
			if (i != vm->stack.length - 1) printf(", ");
		}
		printf(" ] ");
		printf("\n");

		disassembleInstruction(vm, &frame->closure->function->chunk, frame->ip - frame->closure->function->chunk.bytecode.items);
		printf("\n");
#endif

		switch (READ_BYTE()) {
			case OP_USE_CONSTANT: {
				Value constant = READ_CONSTANT();
				push(vm, constant);
				break;
			}

			case OP_NULL: push(vm, NULL_VAL); break;
			case OP_TRUE: push(vm, BOOL_VAL(true)); break;
			case OP_FALSE: push(vm, BOOL_VAL(false)); break;

			case OP_POP: pop(vm); break;

			case OP_DEFINE_GLOBAL: {
				ObjString* name = READ_STRING();
				tableSet(vm, &vm->globals, name, peek(vm, 0));
				pop(vm);
				break;
			}

			case OP_ACCESS_GLOBAL: {
				ObjString* name = READ_STRING();
				Value value;
				if (!tableGet(&vm->globals, name, &value)) {
					throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_UNDEFINED_VARIABLE], "Undefined variable '%s'", name->str);
					break;
				}
				push(vm, value);
				break;
			}

			case OP_ASSIGN_GLOBAL: {
				ObjString* name = READ_STRING();

				if (tableSet(vm, &vm->globals, name, peek(vm, 0))) {
					tableDelete(vm, &vm->globals, name);
					throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_UNDEFINED_VARIABLE], "Undefined variable '%s'", name->str);
					break;
				}

				break;
			}

			case OP_ACCESS_LOCAL: {
				uint16_t slot = READ_SHORT();
				push(vm, (vm->stack.items + frame->slotsOffset)[slot]);
				break;
			}

			case OP_ASSIGN_LOCAL: {
				uint16_t slot = READ_SHORT();
				(vm->stack.items + frame->slotsOffset)[slot] = peek(vm, 0);
				break;
			}

			case OP_ACCESS_UPVALUE: {
				uint8_t slot = (uint8_t)READ_SHORT();
				push(vm, *frame->closure->upvalues[slot]->location);
				break;
			}

			case OP_ASSIGN_UPVALUE: {
				uint8_t slot = (uint8_t)READ_SHORT();
				*frame->closure->upvalues[slot]->location = peek(vm, 0);
				break;
			}

			case OP_CLOSE_UPVALUE: {
				closeUpvalues(vm, &vm->stack.items[vm->stack.length - 1]);
				pop(vm);
				break;
			}

			case OP_JUMP: {
				uint16_t jump = READ_SHORT();
				frame->ip += jump;
				break;
			}

			case OP_JUMP_FALSE: {
				uint16_t jump = READ_SHORT();
				if (isFalsey(vm, pop(vm))) frame->ip += jump;
				break;
			}
			
			// JUMP_FALSE removes the condition, whereas JUMP_FALSE_SC leaves it on the stack
			case OP_JUMP_FALSE_SC: {
				uint16_t jump = READ_SHORT();
				if (isFalsey(vm, peek(vm, 0))) frame->ip += jump;
				break;
			}

			case OP_JUMP_TRUE_SC: {
				uint16_t jump = READ_SHORT();
				if (!isFalsey(vm, peek(vm, 0))) frame->ip += jump;
				break;
			}

			case OP_LOOP: {
				uint16_t jump = READ_SHORT();
				frame->ip -= jump;
				break;
			}

			case OP_PRINT: {
				Value value = pop(vm);
				printValue(vm, value);
				printf("\n");
				break;
			}

			case OP_NEGATE: {
				if (!IS_NUMBER(peek(vm, 0))) {
					throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Operand must be a number");
					break;
				}
				push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
				break;
			}

			case OP_NOT: {
				push(vm, BOOL_VAL(isFalsey(vm, pop(vm))));
				break;
			}

			case OP_EQUAL: {
				Value b = pop(vm);
				Value a = pop(vm);
				push(vm, BOOL_VAL(valuesEqual(vm, a, b)));
				break;
			}

			case OP_NOT_EQUAL: {
				Value b = pop(vm);
				Value a = pop(vm);
				push(vm, BOOL_VAL(!valuesEqual(vm, a, b)));
				break;
			}

			case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
			case OP_LESS_EQUAL: BINARY_OP(BOOL_VAL, <=); break;
			case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
			case OP_GREATER_EQUAL: BINARY_OP(BOOL_VAL, >=); break;

			case OP_ADD: {
				if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
					push(vm, OBJ_VAL(concatenate(vm)));
				}
				else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 0))) {
					double b = AS_NUMBER(pop(vm));
					double a = AS_NUMBER(pop(vm));
					push(vm, NUMBER_VAL(a + b));
				}
				else {
					throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Operands must be strings or numbers");
					break;
				}
				break;
			}
			case OP_SUB: BINARY_OP(NUMBER_VAL, -); break;
			case OP_MUL: BINARY_OP(NUMBER_VAL, *); break;
			case OP_DIV: BINARY_OP(NUMBER_VAL, /); break;

			case OP_CLOSURE: {
				ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
				ObjClosure* closure = newClosure(vm, function);
				push(vm, OBJ_VAL(closure));

				for (size_t i = 0; i < closure->upvalueCount; i++) {
					uint8_t isLocal = READ_BYTE();
					uint8_t index = READ_BYTE();
					if (isLocal) {
						closure->upvalues[i] = captureUpvalue(vm, &vm->stack.items[frame->slotsOffset + index]);
					}
					else {
						closure->upvalues[i] = frame->closure->upvalues[index];
					}
				}

				break;
			}

			case OP_CALL: {
				uint8_t argCount = READ_BYTE();
				if (!callValue(vm, peek(vm, argCount), argCount)) {
					break;
				}
				frame = &vm->frames.items[vm->frames.length - 1];
				break;
			}

			case OP_RETURN: {
				Value result = pop(vm);

				closeUpvalues(vm, &vm->stack.items[frame->slotsOffset]);

				vm->frames.length--;

				if (vm->frames.length == 0) {
					pop(vm); // The script function
					return INTERPRETER_OK;
				}

				vm->stack.length = frame->slotsOffset;

				push(vm, result);

				frame = &vm->frames.items[vm->frames.length - 1];
				break;
			}

			case OP_CLASS: {
				push(vm, OBJ_VAL(newClass(vm, READ_STRING())));
				break;
			}

			case OP_INHERIT: {
				Value superclass = peek(vm, 1);

				if (!IS_CLASS(superclass)) {
					throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Superclass must be a class");
					break;
				}

				ObjClass* subclass = AS_CLASS(peek(vm, 0));

				inheritClasses(vm, subclass, AS_CLASS(superclass));
				pop(vm);

				break;
			}

			case OP_METHOD: {
				defineMethod(vm, READ_STRING());
				break;
			}

			case OP_ACCESS_PROPERTY: {
				if (!IS_INSTANCE(peek(vm, 0))) {
					throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Only instances have properties");
					break;
				}

				ObjInstance* instance = AS_INSTANCE(peek(vm, 0));
				ObjString* name = READ_STRING();

				Value value;
				if (tableGet(&instance->fields, name, &value)) {
					pop(vm); // Pop the instance
					push(vm, value);
					break;
				}

				if (!bindMethod(vm, instance->clazz, name)) {
					throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_PROPERTY], "Undefined property '%s'", name->str);
					break;
				}

				break;
			}

			case OP_ASSIGN_PROPERTY: {
				if (!IS_INSTANCE(peek(vm, 1))) {
					throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Only instances have fields");
					break;
				}

				ObjInstance* instance = AS_INSTANCE(peek(vm, 1));
				tableSet(vm, &instance->fields, READ_STRING(), peek(vm, 0));
				Value value = pop(vm);
				pop(vm);
				push(vm, value);
				break;
			}

			case OP_ASSIGN_PROPERTY_KV: {
				if (!IS_INSTANCE(peek(vm, 1))) {
					throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Only instances have fields");
					break;
				}

				ObjInstance* instance = AS_INSTANCE(peek(vm, 1));
				tableSet(vm, &instance->fields, READ_STRING(), peek(vm, 0));
				pop(vm);
				break;
			}

			case OP_ACCESS_SUPER: {
				ObjString* name = READ_STRING();
				ObjClass* superclass = AS_CLASS(pop(vm));

				if (!bindMethod(vm, superclass, name)) {
					break;
				}
				break;
			}

			case OP_INVOKE: {
				ObjString* method = READ_STRING();
				uint8_t argCount = READ_BYTE();

				if (!invoke(vm, method, argCount)) {
					break;
				}
				frame = &vm->frames.items[vm->frames.length - 1];
				break;
			}

			case OP_SUPER_INVOKE: {
				ObjString* method = READ_STRING();
				uint8_t argCount = READ_BYTE();

				ObjClass* superclass = AS_CLASS(pop(vm));

				if (!invokeFromClass(vm, superclass, method, argCount)) {
					break;
				}
				frame = &vm->frames.items[vm->frames.length - 1];
				break;
			}

			case OP_OBJECT: {
				push(vm, OBJ_VAL(vm->internalClasses[INTERNAL_CLASS_OBJECT]));
				break;
			}

			case OP_CREATE_OBJECT: {
				// This could maybe be re-written to do the creation itself
				// Which may result in a speed-up in tight loops
				callValue(vm, OBJ_VAL(vm->internalClasses[INTERNAL_CLASS_OBJECT]), 0);
				frame = &vm->frames.items[vm->frames.length - 1];
				break;
			}

			case OP_INSTANCEOF: {
				Value superclass = pop(vm);
				Value instance = pop(vm);

				if (!IS_INSTANCE(instance)) {
					throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Left-hand-side of instanceof must be an instance");
					break;
				}

				if (!IS_CLASS(superclass)) {
					throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Right-hand-side of instanceof must be a class");
					break;
				}

				push(vm, BOOL_VAL(instanceof(AS_INSTANCE(instance), AS_CLASS(superclass))));
				break;
			}

			case OP_LIST: {
				uint16_t length = READ_SHORT();

				ValueArray items;
				initValueArray(&items);

				ObjList* list = newList(vm, items);

				push(vm, OBJ_VAL(list));

				for (size_t i = 0; i < length; i++) {
					writeValueArray(vm, &list->items, peek(vm, length - i));
				}
				pop(vm);

				vm->stack.length -= length;

				push(vm, OBJ_VAL(list));
				break;
			}

			case OP_ACCESS_SUBSCRIPT: {
				Value index = peek(vm, 0);
				Value indexee = peek(vm, 1);

				if (IS_LIST(indexee)) {
					ObjList* list = AS_LIST(indexee);

					if (!IS_NUMBER(index)) {
						throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_INDEX_RANGE], "List index must be a number");
						break;
					}

					size_t realIndex;
					if (!validateIndex(vm, list->items.length, AS_NUMBER(index), &realIndex)) {
						break;
					}

					pop(vm);
					pop(vm);
					push(vm, list->items.items[realIndex]);

				}
				else if (IS_INSTANCE(indexee)) {
					ObjInstance* instance = AS_INSTANCE(indexee);

					if (!IS_STRING(index)) {
						throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_PROPERTY], "Property name must be a string in subscript");
						break;
					}

					ObjString* propertyName = AS_STRING(index);

					Value value;
					if (tableGet(&instance->fields, propertyName, &value)) {
						pop(vm);
						pop(vm);
						push(vm, value);
						break;
					}

					pop(vm); // Pop the propertyName off
					if (!bindMethod(vm, instance->clazz, propertyName)) {
						push(vm, NULL_VAL);
					}
				}
				else {
					throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Invalid subscript target");
					break;
				}

				break;
			}

			case OP_ASSIGN_SUBSCRIPT: {
				Value value = peek(vm, 0);
				Value index = peek(vm, 1);
				Value indexee = peek(vm, 2);

				if (IS_LIST(indexee)) {
					ObjList* list = AS_LIST(indexee);

					if (!IS_NUMBER(index)) {
						throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "List index must be a number");
						break;
					}

					size_t realIndex;
					if (!validateIndex(vm, list->items.length, AS_NUMBER(index), &realIndex)) {
						break;
					}

					list->items.items[realIndex] = value;

					pop(vm);
					pop(vm);
					pop(vm);
					push(vm, value);
				}
				else if (IS_INSTANCE(indexee)) {
					ObjInstance* instance = AS_INSTANCE(indexee);

					if (!IS_STRING(index)) {
						throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_PROPERTY], "Property name must be a string in subscript");
						break;
					}

					ObjString* propertyName = AS_STRING(index);

					tableSet(vm, &instance->fields, propertyName, value);
					pop(vm);
					pop(vm);
					pop(vm);
					push(vm, value);
				}
				else {
					throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Invalid subscript target");
					break;
				}

				break;
			}

			case OP_THROW: {
				vm->exception = pop(vm);
				vm->hasException = true;
				break;
			}

			case OP_TRY_BEGIN: {
				uint16_t catchJump = READ_SHORT();
				frame->catchLocation = frame->ip + catchJump;
				frame->isTryBlock = true;
				frame->tryStackOffset = vm->stack.length;
				break;
			}

			case OP_TRY_END: {
				frame->catchLocation = NULL;
				frame->isTryBlock = false;
				frame->tryStackOffset = 0;
				break;
			}

			case OP_BOUND_EXCEPTION: {
				push(vm, vm->exception);
				break;
			}
		}

	}

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
}

InterpreterResult interpret(VM* vm, const char* source) {

	ObjFunction* function = compile(vm, source);

	if (function == NULL) return INTERPRETER_COMPILE_ERROR;

	push(vm, OBJ_VAL(function));
	ObjClosure* closure = newClosure(vm, function);
	pop(vm);
	push(vm, OBJ_VAL(closure));

	// Set the script as the execution context
	callClosure(vm, closure, 0);

	return executeVM(vm);
}