#include "exports.h"
#include "../object.h"

ObjClass* export_getInternalException(VM* vm, InternalExceptionType type) {
	return vm->internalExceptions[type];
}

bool export_isInstance(Value value) {
	return IS_OBJ(value) && AS_OBJ(value)->type == OBJ_INSTANCE;
}

ObjInstance* export_asInstance(Value value) {
	return AS_INSTANCE(value);
}

bool export_isString(Value value) {
	return IS_OBJ(value) && AS_OBJ(value)->type == OBJ_STRING;
}
ObjString* export_asString(Value value) {
	return AS_STRING(value);
}

bool export_getInstanceField(VM* vm, ObjInstance* instance, const char* name, Value* value) {
	ObjString* field = copyString(vm, name, strlen(name));
	
	return tableGet(&instance->fields, field, value);
}

bool export_setInstanceField(VM* vm, ObjInstance* instance, const char* name, Value value) {
	ObjString* field = copyString(vm, name, strlen(name));
	push(vm, OBJ_VAL(field));
	bool newField = tableSet(vm, &instance->fields, field, value);
	pop(vm);
	return newField;
}

char* export_getStringCharacters(ObjString* string) {
	return string->str;
}

size_t export_getStringLength(ObjString* string) {
	return string->length;
}

void export_setInstanceNativeData(ObjInstance* instance, InstanceData* data) {
	instance->nativeData = data;
}

InstanceData* export_getInstanceNativeData(ObjInstance* instance) {
	return instance->nativeData;
}
