#include "listnatives.h"
#include "natives.h"

static Value listLengthNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	return NUMBER_VAL(AS_LIST(bound)->items.length);
}

void defineListNativeMethods(VM* vm) {
	defineNative(vm, &vm->listMethods, "length", listLengthNative, 0);
}
