#include "objectclass.h"
#include "../vm.h"

void defineObjectClass(VM* vm) {
	vm->internalClasses[INTERNAL_CLASS_OBJECT] = newClass(vm, vm->internalStrings[INTERNAL_STR_OBJECT]);
	tableSet(vm, &vm->globals, vm->internalStrings[INTERNAL_STR_OBJECT], OBJ_VAL(vm->internalClasses[INTERNAL_CLASS_OBJECT]));
}