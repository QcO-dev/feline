#include "objectclass.h"
#include "natives.h"
#include "../vm.h"

Value objectKeys(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjInstance* instance = AS_INSTANCE(bound);

	ValueArray keys;
	initValueArray(&keys);

	for (size_t i = 0; i < instance->fields.capacity; i++) {
		Entry* entry = &instance->fields.entries[i];
		if (entry->key != NULL) {
			writeValueArray(vm, &keys, OBJ_VAL(entry->key));
		}
	}

	ObjList* list = newList(vm, keys);
	return OBJ_VAL(list);
}

Value objectValues(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjInstance* instance = AS_INSTANCE(bound);

	ValueArray values;
	initValueArray(&values);

	for (size_t i = 0; i < instance->fields.capacity; i++) {
		Entry* entry = &instance->fields.entries[i];
		if (entry->key != NULL) {
			writeValueArray(vm, &values, entry->value);
		}
	}

	ObjList* list = newList(vm, values);
	return OBJ_VAL(list);
}

void defineObjectClass(VM* vm) {
	vm->internalClasses[INTERNAL_CLASS_OBJECT] = newClass(vm, vm->internalStrings[INTERNAL_STR_OBJECT]);
	defineNative(vm, &vm->internalClasses[INTERNAL_CLASS_OBJECT]->methods, "keys", objectKeys, 0);
	defineNative(vm, &vm->internalClasses[INTERNAL_CLASS_OBJECT]->methods, "values", objectValues, 0);
}

void bindObjectClass(VM* vm, Module* mod) {
	tableSet(vm, &mod->globals, vm->internalStrings[INTERNAL_STR_OBJECT], OBJ_VAL(vm->internalClasses[INTERNAL_CLASS_OBJECT]));
}