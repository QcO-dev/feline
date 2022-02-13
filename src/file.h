#pragma once
#include "vm.h"
#include "module.h"

char* readFile(const char* path);

void splitPathToNameAndDirectory(VM* vm, Module* mod, const char* path);