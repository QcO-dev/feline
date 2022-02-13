#pragma once
#include "../common.h"
#include "../vm.h"
#include "../builtin/exception.h"
#include "felineffi.h"

FELINE_EXPORT ObjClass* export_getInternalException(VM* vm, InternalExceptionType type);
FELINE_EXPORT bool export_isInstance(Value value);
FELINE_EXPORT ObjInstance* export_asInstance(Value value);
FELINE_EXPORT bool export_getInstanceField(VM* vm, ObjInstance* instance, const char* name, Value* value);
FELINE_EXPORT bool export_setInstanceField(VM* vm, ObjInstance* instance, const char* name, Value value);