#include "memory.h"
#include "object.h"
#include "compiler.h"
#include "ffi/ffi.h"
#include <stdlib.h>
#include <stdio.h>

#define GC_HEAP_GROW_FACTOR 2

static void freeObject(VM* vm, Obj* object);

void* reallocate(VM* vm, void* pointer, size_t oldCapacity, size_t newCapacity) {
	vm->bytesAllocated += newCapacity - oldCapacity;

	if (newCapacity > oldCapacity) {
#ifdef FELINE_DEBUG_STRESS_GC
		collectGarbage(vm);
#endif
		if (vm->bytesAllocated > vm->nextGC) {
			collectGarbage(vm);
		}
	}

	if (newCapacity == 0) {
		free(pointer);
		return NULL;
	}

	void* result = realloc(pointer, newCapacity);

	if (result == NULL) {
		fprintf(stderr, "Failed to allocate %zu bytes (from %zu bytes)\n", newCapacity, oldCapacity);
		exit(1);
	}

	return result;
}

void markObject(VM* vm, Obj* object) {
	if (object == NULL) return;
	if (object->isMarked) return;

#ifdef FELINE_DEBUG_LOG_GC
	printf("%p mark ", (void*)object);
	printValue(vm, OBJ_VAL(object));
	printf("\n");
#endif

	object->isMarked = true;

	if (vm->grayCapacity < vm->grayCount + 1) {
		vm->grayCapacity = GROW_CAPACITY(vm->grayCapacity);
		vm->grayStack = (Obj**)realloc(vm->grayStack, sizeof(Obj*) * vm->grayCapacity);

		if (vm->grayStack == NULL) exit(1);
	}

	vm->grayStack[vm->grayCount++] = object;
}

void markValue(VM* vm, Value value) {
	if (IS_OBJ(value)) markObject(vm, AS_OBJ(value));
}

static void markRoots(VM* vm) {
	for (Value* slot = vm->stack.items; slot < &vm->stack.items[vm->stack.length]; slot++) {
		markValue(vm, *slot);
	}

	for (size_t i = 0; i < vm->frames.length; i++) {
		markObject(vm, (Obj*)vm->frames.items[i].closure);
	}

	for (ObjUpvalue* upvalue = vm->openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
		markObject(vm, (Obj*)upvalue);
	}

	
	Module* mod = vm->modules;

	while (mod != NULL) {
		markTable(vm, &mod->globals);
		markObject(vm, (Obj*)mod->name);
		markObject(vm, (Obj*)mod->directory);
		mod = mod->next;
	}

	markTable(vm, &vm->nativeLibraries);
	markObject(vm, (Obj*)&vm->baseDirectory);

	markCompilerRoots(vm);
	
	for (size_t i = 0; i < INTERNAL_STR__COUNT; i++) {
		markObject(vm, (Obj*)vm->internalStrings[i]);
	}

	for (size_t i = 0; i < INTERNAL_EXCEPTION__COUNT; i++) {
		markObject(vm, (Obj*)vm->internalExceptions[i]);
	}

	for (size_t i = 0; i < INTERNAL_CLASS__COUNT; i++) {
		markObject(vm, (Obj*)vm->internalClasses[i]);
	}

	markValue(vm, vm->exception);
}

static void markArray(VM* vm, ValueArray* array) {
	for (size_t i = 0; i < array->length; i++) {
		markValue(vm, array->items[i]);
	}
}

static void blackenObject(VM* vm, Obj* object) {
#ifdef FELINE_DEBUG_LOG_GC
	printf("%p blacken ", (void*)object);
	printValue(vm, OBJ_VAL(object));
	printf("\n");
#endif

	switch (object->type) {
		case OBJ_CLOSURE: {
			ObjClosure* closure = (ObjClosure*)object;
			markObject(vm, (Obj*)closure->function);
			for (size_t i = 0; i < closure->upvalueCount; i++) {
				markObject(vm, (Obj*)closure->upvalues[i]);
			}
			break;
		}
		case OBJ_FUNCTION: {
			ObjFunction* function = (ObjFunction*)object;
			markObject(vm, (Obj*)function->name);
			markArray(vm, &function->chunk.constants);
			break;
		}
		case OBJ_UPVALUE: {
			markValue(vm, ((ObjUpvalue*)object)->closed);
			break;
		}
		case OBJ_CLASS: {
			ObjClass* clazz = (ObjClass*)object;
			markObject(vm, (Obj*)clazz->name);
			markObject(vm, (Obj*)clazz->superclass);
			markTable(vm, &clazz->methods);
			break;
		}
		case OBJ_INSTANCE: {
			ObjInstance* instance = (ObjInstance*)object;
			markObject(vm, (Obj*)instance->clazz);
			markTable(vm, &instance->fields);
			break;
		}
		case OBJ_BOUND_METHOD: {
			ObjBoundMethod* bound = (ObjBoundMethod*)object;
			markValue(vm, bound->receiver);
			markObject(vm, (Obj*)bound->method);
			break;
		}
		case OBJ_LIST: {
			ObjList* list = (ObjList*)object;
			markArray(vm, &list->items);
			break;
		}
		case OBJ_NATIVE:
		case OBJ_NATIVE_LIBRARY:
		case OBJ_STRING: {
			break;
		}
	}
}

static void traceReferences(VM* vm) {
	while (vm->grayCount > 0) {
		Obj* object = vm->grayStack[--vm->grayCount];
		blackenObject(vm, object);
	}
}

static void sweep(VM* vm) {
	Obj* previous = NULL;
	Obj* object = vm->objects;
	while (object != NULL) {
		if (object->isMarked) {
			object->isMarked = false;
			previous = object;
			object = object->next;
		}
		else {
			Obj* unreached = object;
			object = object->next;

			if (previous != NULL) {
				previous->next = object;
			}
			else {
				vm->objects = object;
			}
			freeObject(vm, unreached);
		}
	}
}

void collectGarbage(VM* vm) {
#ifdef FELINE_DEBUG_LOG_GC
	printf("-- GC Begin\n");
	size_t before = vm->bytesAllocated;
#endif

	markRoots(vm);
	traceReferences(vm);
	tableRemoveWhite(vm, &vm->strings);
	sweep(vm);

	vm->nextGC = vm->bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef FELINE_DEBUG_LOG_GC
	printf("-- GC End\n");
	printf("   collected %zu bytes (from %zu to %zu), next at %zu\n", before - vm->bytesAllocated, before, vm->bytesAllocated, vm->nextGC);
#endif
}

static void freeObject(VM* vm, Obj* object) {
#ifdef FELINE_DEBUG_LOG_GC
	printf("%p free type %d\n", (void*)object, object->type);
#endif

	switch (object->type) {
		case OBJ_STRING: {
			ObjString* string = (ObjString*)object;
			FREE_ARRAY(vm, char, string->str, string->length + 1);
			FREE(vm, ObjString, object);
			break;
		}
		case OBJ_FUNCTION: {
			ObjFunction* function = (ObjFunction*)object;
			freeChunk(vm, &function->chunk);
			FREE(vm, ObjFunction, object);
			break;
		}
		case OBJ_CLOSURE: {
			ObjClosure* closure = (ObjClosure*)object;
			FREE_ARRAY(vm, ObjUpvalue*, closure->upvalues, closure->upvalueCount);
			FREE(vm, ObjClosure, object);
			break;
		}
		case OBJ_UPVALUE: {
			FREE(vm, ObjUpvalue, object);
			break;
		}
		case OBJ_NATIVE: {
			FREE(vm, ObjNative, object);
			break;
		}
		case OBJ_CLASS: {
			ObjClass* clazz = (ObjClass*)object;
			freeTable(vm, &clazz->methods);
			FREE(vm, ObjClass, object);
			break;
		}
		case OBJ_INSTANCE: {
			ObjInstance* instance = (ObjInstance*)object;
			freeTable(vm, &instance->fields);
			FREE(vm, ObjInstance, object);
			break;
		}
		case OBJ_BOUND_METHOD: {
			FREE(vm, ObjBoundMethod, object);
			break;
		}
		case OBJ_LIST: {
			ObjList* list = (ObjList*)object;
			freeValueArray(vm, &list->items);
			FREE(vm, ObjList, object);
			break;
		}
		case OBJ_NATIVE_LIBRARY: {
			ObjNativeLibrary* library = (ObjNativeLibrary*)object;
			freeNativeLibrary(library->library);
			FREE(vm, ObjNativeLibrary, object);
			break;
		}
	}
}

void freeObjects(VM* vm) {
	Obj* object = vm->objects;

	while (object != NULL) {
		Obj* next = object->next;
		freeObject(vm, object);
		object = next;
	}

	free(vm->grayStack);
}