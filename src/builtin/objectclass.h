#pragma once

#include "../common.h"
#include "../module.h"

typedef struct VM VM;

void defineObjectClass(VM* vm);
void bindObjectClass(VM* vm, Module* mod);