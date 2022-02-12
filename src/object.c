#include "object.h"
#include "memory.h"
#include "table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define ALLOCATE_OBJ(vm, type, objectType) (type*)allocateObject(vm, sizeof(type), objectType);

static Obj* allocateObject(VM* vm, size_t size, ObjType type) {
	Obj* object = (Obj*)reallocate(vm, NULL, 0, size);
	object->type = type;
	object->isMarked = false;

	object->next = vm->objects;
	vm->objects = object;

#ifdef FELINE_DEBUG_LOG_GC
	printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

	return object;
}

// ========= Functions =========

ObjFunction* newFunction(VM* vm) {
	ObjFunction* function = ALLOCATE_OBJ(vm, ObjFunction, OBJ_FUNCTION);
	function->arity = 0;
	function->upvalueCount = 0;
	function->name = NULL;
	initChunk(&function->chunk);
	return function;
}

// ========= Closures =========

ObjClosure* newClosure(VM* vm, ObjFunction* function) {
	ObjUpvalue** upvalues = ALLOCATE(vm, ObjUpvalue*, function->upvalueCount);

	for (size_t i = 0; i < function->upvalueCount; i++) {
		upvalues[i] = NULL;
	}

	ObjClosure* closure = ALLOCATE_OBJ(vm, ObjClosure, OBJ_CLOSURE);
	closure->function = function;
	closure->upvalues = upvalues;
	closure->upvalueCount = function->upvalueCount;
	return closure;
}

// ========= Upvalue =========

ObjUpvalue* newUpvalue(VM* vm, Value* slot) {
	ObjUpvalue* upvalue = ALLOCATE_OBJ(vm, ObjUpvalue, OBJ_UPVALUE);
	upvalue->location = slot;
	upvalue->closed = NULL_VAL;
	upvalue->next = NULL;
	return upvalue;
}

// ========= Native Functions =========

ObjNative* newNative(VM* vm, NativeFunction function, size_t arity) {
	ObjNative* native = ALLOCATE_OBJ(vm, ObjNative, OBJ_NATIVE);
	native->function = function;
	native->arity = arity;
	return native;
}

// ========= Class =========

ObjClass* newClass(VM* vm, ObjString* name) {
	ObjClass* clazz = ALLOCATE_OBJ(vm, ObjClass, OBJ_CLASS);
	clazz->name = name;
	clazz->superclass = NULL;
	initTable(&clazz->methods);
	return clazz;
}

// ========= Instance =========

ObjInstance* newInstance(VM* vm, ObjClass* clazz) {
	ObjInstance* instance = ALLOCATE_OBJ(vm, ObjInstance, OBJ_INSTANCE);
	instance->clazz = clazz;
	initTable(&instance->fields);
	return instance;
}

// ========= Bound Method =========

ObjBoundMethod* newBoundMethod(VM* vm, Value receiver, ObjClosure* method) {
	ObjBoundMethod* bound = ALLOCATE_OBJ(vm, ObjBoundMethod, OBJ_BOUND_METHOD);
	bound->receiver = receiver;
	bound->method = method;
	return bound;
}

// ========= Lists =========

ObjList* newList(VM* vm, ValueArray items) {
	ObjList* list = ALLOCATE_OBJ(vm, ObjList, OBJ_LIST);
	list->items = items;
	return list;
}

// ========= Strings =========

static ObjString* allocateString(VM* vm, char* str, size_t length, uint32_t hash) {
	ObjString* string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
	string->length = length;
	string->str = str;
	string->hash = hash;

	push(vm, OBJ_VAL(string));
	tableSet(vm, &vm->strings, string, NULL_VAL);
	pop(vm);
	return string;
}

// FNV-1a Hash Function
static uint32_t hashString(const char* key, size_t length) {
	uint32_t hash = 2166136261u;
	for (size_t i = 0; i < length; i++) {
		hash ^= (uint8_t)key[i];
		hash *= 16777619;
	}
	return hash;
}

ObjString* copyString(VM* vm, const char* str, size_t length) {
	uint32_t hash = hashString(str, length);
	ObjString* interned = tableFindString(&vm->strings, str, length, hash);

	if (interned != NULL) return interned;

	char* heapString = ALLOCATE(vm, char, length + 1);
	memcpy(heapString, str, length);
	heapString[length] = '\0';
	return allocateString(vm, heapString, length, hash);
}

ObjString* takeString(VM* vm, char* str, size_t length) {
	uint32_t hash = hashString(str, length);
	ObjString* interned = tableFindString(&vm->strings, str, length, hash);

	if (interned != NULL) {
		FREE_ARRAY(vm, char, str, length + 1);
		return interned;
	}

	return allocateString(vm, str, length, hash);
}

ObjString* makeStringf(VM* vm, const char* format, ...) {
	va_list args;
	va_start(args, format);
	ObjString* string = makeStringvf(vm, format, args);
	va_end(args);

	return string;
}

ObjString* makeStringvf(VM* vm, const char* format, va_list vsnargs) {
	va_list vsargs;
	va_copy(vsargs, vsnargs);
	size_t length = vsnprintf(NULL, 0, format, vsnargs);
	va_end(vsnargs);
	char* string = ALLOCATE(vm, char, length + 1);

	vsprintf(string, format, vsargs);

	return takeString(vm, string, length);
}

// ========= Misc. =========

void printFunction(VM* vm, ObjFunction* function) {
	if (function->name == NULL) {
		printf("<script function>");
		return;
	}

	printf("<function %s>", function->name->str);
}

void printObject(VM* vm, Value value) {
	switch (OBJ_TYPE(value)) {
		case OBJ_STRING: {
			printf("%s", AS_CSTRING(value));
			break;
		}
		case OBJ_CLASS: {
			printf("<class %s>", AS_CLASS(value)->name->str);
			break;
		}
		case OBJ_INSTANCE: {
			printf("<instance %s>", AS_INSTANCE(value)->clazz->name->str);
			break;
		}
		case OBJ_BOUND_METHOD: {
			printFunction(vm, AS_BOUND_METHOD(value)->method->function);
			break;
		}
		case OBJ_CLOSURE: {
			printFunction(vm, AS_CLOSURE(value)->function);
			break;
		}
		case OBJ_UPVALUE: {
			// Should be unreachable, but some output is useful just in case
			printf("upvalue");
			break;
		}
		case OBJ_NATIVE: {
			printf("<native function>");
			break;
		}
		case OBJ_FUNCTION: {
			printFunction(vm, AS_FUNCTION(value));
			break;
		}

		case OBJ_LIST: {
			ObjList* list = AS_LIST(value);
			printf("[");

			for (size_t i = 0; i < list->items.length; i++) {
				printValue(vm, list->items.items[i]);
				if (i != list->items.length - 1) printf(", ");
			}

			printf("]");
			break;
		}
	}
}