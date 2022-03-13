#include "value.h"
#include "memory.h"
#include "object.h"
#include <stdio.h>
#include <string.h>

DEFINE_DYNAMIC_ARRAY(Value, Value)

void printValue(VM* vm, Value value) {
	switch (value.type) {
		case VAL_BOOL: {
			printf(AS_BOOL(value) ? "true" : "false");
			break;
		}
		case VAL_NULL: {
			printf("null");
			break;
		}
		case VAL_NUMBER: {
			printf("%g", AS_NUMBER(value));
			break;
		}
		case VAL_OBJ: {
			printObject(vm, value);
			break;
		}
	}
}

// In Feline, null, false, and 0 are considered falsey.
// Everything else is truthy
bool isFalsey(VM* vm, Value value) {
	return IS_NULL(value)
		|| (IS_BOOL(value) && !AS_BOOL(value))
		|| (IS_NUMBER(value) && AS_NUMBER(value) == 0);
}

bool valuesEqual(VM* vm, Value a, Value b) {
	if (a.type != b.type) return false;

	switch (a.type) {
		case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
		case VAL_NULL: return true;
		case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
		case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
		default: {
			ASSERT(0, "Unreachable");
			return false;
		}
	}
}

bool isFunction(Value value) {
	// IS_FUNCTION is not checked because functions are not first class values to the user
	return IS_CLOSURE(value) || IS_BOUND_METHOD(value) || IS_NATIVE(value);
}