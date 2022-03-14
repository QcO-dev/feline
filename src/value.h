#pragma once
#include "common.h"
#include "darray.h"

typedef struct VM VM;
typedef struct Obj Obj;

typedef struct ObjString ObjString;

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

typedef Value(*NativeFunction)(VM* vm, Value bound, uint8_t argCount, Value* value);

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

bool valuesEqual(VM* vm, Value a, Value b);
void printValue(VM* vm, Value value);
bool isFalsey(VM* vm, Value value);
bool isFunction(Value value);

DECLARE_DYNAMIC_ARRAY(Value, Value)