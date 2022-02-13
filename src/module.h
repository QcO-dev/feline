#pragma once
#include "common.h"
#include "table.h"

typedef struct Module {
	Table globals;
	Table exports;
	ObjString* name;
	ObjString* directory;
	struct Module* next;
} Module;

void initModule(VM* vm, Module* mod);
void freeModule(VM* vm, Module* mod);