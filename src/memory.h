#pragma once
#include "common.h"
#include "vm.h"

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(vm, type, pointer, oldCapacity, newCapacity) (type*) reallocate(vm, pointer, sizeof(type) * (oldCapacity), sizeof(type) * (newCapacity))

#define FREE_ARRAY(vm, type, pointer, oldCapacity) reallocate(vm, pointer, sizeof(type) * (oldCapacity), 0)

#define ALLOCATE(vm, type, count) (type*)reallocate(vm, NULL, 0, sizeof(type) * (count))

#define FREE(vm, type, pointer) reallocate(vm, pointer, sizeof(type), 0)

void* reallocate(VM* vm, void* pointer, size_t oldCapacity, size_t newCapacity);

void markObject(VM* vm, Obj* object);
void markValue(VM* vm, Value value);
void collectGarbage(VM* vm);
void freeObjects(VM* vm);