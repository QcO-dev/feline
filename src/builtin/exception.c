#include "exception.h"
#include "../vm.h"
#include "../table.h"

void defineExceptionSubclass(VM* vm, InternalExceptionType type, InternalString name) {
	vm->internalExceptions[type] = newClass(vm, vm->internalStrings[name]);
	
	inheritClasses(vm, vm->internalExceptions[type], vm->internalExceptions[INTERNAL_EXCEPTION_BASE]);

	tableSet(vm, &vm->globals, vm->internalStrings[name], OBJ_VAL(vm->internalExceptions[type]));
}

void defineExceptionClasses(VM* vm) {
	vm->internalExceptions[INTERNAL_EXCEPTION_BASE] = newClass(vm, vm->internalStrings[INTERNAL_STR_EXCEPTION]);
	inheritClasses(vm, vm->internalExceptions[INTERNAL_EXCEPTION_BASE], vm->internalClasses[INTERNAL_CLASS_OBJECT]);
	tableSet(vm, &vm->globals, vm->internalStrings[INTERNAL_STR_EXCEPTION], OBJ_VAL(vm->internalExceptions[INTERNAL_EXCEPTION_BASE]));

	defineExceptionSubclass(vm, INTERNAL_EXCEPTION_TYPE,               INTERNAL_STR_TYPE_EXCEPTION);
	defineExceptionSubclass(vm, INTERNAL_EXCEPTION_ARITY,              INTERNAL_STR_ARITY_EXCEPTION);
	defineExceptionSubclass(vm, INTERNAL_EXCEPTION_PROPERTY,           INTERNAL_STR_PROPERTY_EXCEPTION);
	defineExceptionSubclass(vm, INTERNAL_EXCEPTION_INDEX_RANGE,        INTERNAL_STR_INDEX_RANGE_EXCEPTION);
	defineExceptionSubclass(vm, INTERNAL_EXCEPTION_UNDEFINED_VARIABLE, INTERNAL_STR_UNDEFINED_VARIABLE_EXCEPTION);
	defineExceptionSubclass(vm, INTERNAL_EXCEPTION_STACK_OVERFLOW,     INTERNAL_STR_STACK_OVERFLOW_EXCEPTION);
	defineExceptionSubclass(vm, INTERNAL_EXCEPTION_LINK_FAILURE,       INTERNAL_STR_LINK_FAILURE_EXCEPTION);
}