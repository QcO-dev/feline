#pragma once
#include "../common.h"
#include "../vm.h"
#include "../builtin/exception.h"
#include "felineffi.h"

FELINE_EXPORT ObjClass* export_getInternalException(VM* vm, InternalExceptionType type);
FELINE_EXPORT bool export_isInstance(Value value);
FELINE_EXPORT ObjInstance* export_asInstance(Value value);
FELINE_EXPORT bool export_isString(Value value);
FELINE_EXPORT ObjString* export_asString(Value value);
FELINE_EXPORT bool export_getInstanceField(VM* vm, ObjInstance* instance, const char* name, Value* value);
FELINE_EXPORT bool export_setInstanceField(VM* vm, ObjInstance* instance, const char* name, Value value);
FELINE_EXPORT char* export_getStringCharacters(ObjString* string);
FELINE_EXPORT size_t export_getStringLength(ObjString* string);
FELINE_EXPORT void export_setInstanceNativeData(ObjInstance* instance, InstanceData* data);
FELINE_EXPORT InstanceData* export_getInstanceNativeData(ObjInstance* instance);