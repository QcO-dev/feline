#include "natives.h"
#include <time.h>

void defineNative(VM* vm, Table* table, const char* name, NativeFunction function, size_t arity) {
	push(vm, OBJ_VAL(copyString(vm, name, strlen(name))));
	push(vm, OBJ_VAL(newNative(vm, function, arity)));
	tableSet(vm, table, AS_STRING(peek(vm, 1)), peek(vm, 0));
	pop(vm);
	pop(vm);
}

Value clockNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	return NUMBER_VAL((double)clock() / 1000);
}

Value lenNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	if (IS_LIST(args[0])) {
		return NUMBER_VAL((double)AS_LIST(args[0])->items.length);
	}
	else if (IS_STRING(args[0])) {
		return NUMBER_VAL((double)AS_STRING(args[0])->length);
	}

	throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Expected argument to be a list or string");
	return NULL_VAL;
}