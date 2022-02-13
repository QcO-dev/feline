#pragma once
#include "../common.h"
#include "../module.h"

typedef struct VM VM;

void defineImportClass(VM* vm);
void bindImportClass(VM* vm, Module* mod);