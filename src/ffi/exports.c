#include "exports.h"

ObjClass* export_getInternalException(VM* vm, InternalExceptionType type) {
	return vm->internalExceptions[type];
}