#include "exception.h"
#include "../vm.h"
#include "../table.h"

void defineExceptionClasses(VM* vm) {
	vm->internalExceptions[INTERNAL_EXCEPTION_BASE] = newClass(vm, vm->internalStrings[INTERNAL_STR_EXCEPTION]);
	tableSet(vm, &vm->globals, vm->internalStrings[INTERNAL_STR_EXCEPTION], OBJ_VAL(vm->internalExceptions[INTERNAL_EXCEPTION_BASE]));

}