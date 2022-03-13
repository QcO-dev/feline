#include "listnatives.h"
#include "natives.h"

static Value listLengthNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	return NUMBER_VAL(AS_LIST(bound)->items.length);
}

static Value listMapNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);

	Value callback = args[0];

	if (!isFunction(callback)) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Expected function as callback");
		return NULL_VAL;
	}

	ValueArray items;
	initValueArray(&items);

	ObjList* mappedList = newList(vm, items);

	push(vm, OBJ_VAL(mappedList));

	for (size_t i = 0; i < list->items.length; i++) {
		push(vm, list->items.items[i]);
		push(vm, NUMBER_VAL((double)i));
		push(vm, OBJ_VAL(list));
		Value mapped = callFromNative(vm, callback, 3);
		if (vm->hasException) {
			return NULL_VAL;
		}
		writeValueArray(vm, &mappedList->items, mapped);
	}

	pop(vm);

	return OBJ_VAL(mappedList);
}

void defineListNativeMethods(VM* vm) {
	defineNative(vm, &vm->listMethods, "length", listLengthNative, 0);
	defineNative(vm, &vm->listMethods, "map", listMapNative, 1);
}
