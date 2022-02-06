#pragma once

#include "common.h"
#include "value.h"
#include "chunk.h"
#include "table.h"

typedef enum ObjType {
	OBJ_STRING,
	OBJ_FUNCTION,
	OBJ_CLOSURE,
	OBJ_UPVALUE,
	OBJ_NATIVE,
	OBJ_CLASS,
	OBJ_INSTANCE,
	OBJ_BOUND_METHOD,
} ObjType;

struct Obj {
	ObjType type;
	bool isMarked;
	struct Obj* next;
};

typedef struct ObjFunction {
	Obj obj;
	size_t arity;
	size_t upvalueCount;
	Chunk chunk;
	ObjString* name;
} ObjFunction;

typedef struct ObjUpvalue {
	Obj obj;
	Value* location;
	Value closed;
	struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct ObjClosure {
	Obj obj;
	ObjFunction* function;
	ObjUpvalue** upvalues;
	size_t upvalueCount;
} ObjClosure;

typedef Value(*NativeFunction)(VM* vm, uint8_t argCount, Value* args);

typedef struct ObjNative {
	Obj obj;
	NativeFunction function;
} ObjNative;

typedef struct ObjClass {
	Obj obj;
	ObjString* name;
	Table methods;
} ObjClass;

typedef struct ObjInstance {
	Obj obj;
	ObjClass* clazz;
	Table fields;
} ObjInstance;

typedef struct ObjBoundMethod {
	Obj obj;
	Value receiver;
	ObjClosure* method;
} ObjBoundMethod;

struct ObjString {
	Obj obj;
	size_t length;
	char* str;
	uint32_t hash;
};

static inline bool isObjType(Value value, ObjType type) {
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

ObjFunction* newFunction(VM* vm);

ObjClosure* newClosure(VM* vm, ObjFunction* function);

ObjUpvalue* newUpvalue(VM* vm, Value* slot);

ObjNative* newNative(VM* vm, NativeFunction function);

ObjClass* newClass(VM* vm, ObjString* name);

ObjInstance* newInstance(VM* vm, ObjClass* clazz);

ObjBoundMethod* newBoundMethod(VM* vm, Value receiver, ObjClosure* method);

ObjString* copyString(VM* vm, const char* str, size_t length);
ObjString* takeString(VM* vm, char* str, size_t length);

void printObject(VM* vm, Value value);

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_CLASS(value) isObjType(value, OBJ_CLASS)
#define IS_INSTANCE(value) isObjType(value, OBJ_INSTANCE)
#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)

#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->str)
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))
#define AS_NATIVE_OBJ(value) ((ObjNative*)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value))->function)
#define AS_CLASS(value) ((ObjClass*)AS_OBJ(value))
#define AS_INSTANCE(value) ((ObjInstance*)AS_OBJ(value))
#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))