#pragma once
#include "../vm.h"
#include "../value.h"

void defineNative(VM* vm, Module* mod, const char* name, NativeFunction function, size_t arity);

Value clockNative(VM* vm, uint8_t argCount, Value* args);

//TODO:
//  len() is temporary until we can access methods on lists & strings :-)
Value lenNative(VM* vm, uint8_t argCount, Value* args);