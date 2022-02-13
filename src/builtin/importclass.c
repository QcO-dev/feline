#include "importclass.h"
#include "../vm.h"

void defineImportClass(VM* vm) {
	vm->internalClasses[INTERNAL_CLASS_IMPORT] = newClass(vm, vm->internalStrings[INTERNAL_STR_IMPORT]);
	inheritClasses(vm, vm->internalClasses[INTERNAL_CLASS_IMPORT], vm->internalClasses[INTERNAL_CLASS_OBJECT]);
}

void bindImportClass(VM* vm, Module* mod) {
	tableSet(vm, &mod->globals, vm->internalStrings[INTERNAL_STR_IMPORT], OBJ_VAL(vm->internalClasses[INTERNAL_CLASS_IMPORT]));
}