#include "module.h"
#include "builtin/natives.h"
#include "builtin/exception.h"
#include "builtin/objectclass.h"
#include "builtin/importclass.h"

void initModule(VM* vm, Module* mod) {
	mod->next = vm->modules;
	vm->modules = mod;
	mod->name = NULL;
	mod->directory = NULL;
	initTable(&mod->globals);
	initTable(&mod->exports);

	defineNative(vm, mod, "clock", clockNative, 0);
	defineNative(vm, mod, "len", lenNative, 1);

	bindObjectClass(vm, mod);
	bindImportClass(vm, mod);

	bindExceptionClasses(vm, mod);
}

void freeModule(VM* vm, Module* mod) {
	freeTable(vm, &mod->globals);
	freeTable(vm, &mod->exports);
}