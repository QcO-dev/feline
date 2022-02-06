#pragma once
#include "common.h"
#include "vm.h"

ObjFunction* compile(VM* vm, const char* source);
void markCompilerRoots(VM* vm);