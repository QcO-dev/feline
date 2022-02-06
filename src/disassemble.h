#pragma once
#include "common.h"
#include "vm.h"
#include "chunk.h"


void disassemble(VM* vm, Chunk* chunk, const char* name);
size_t disassembleInstruction(VM* vm, Chunk* chunk, size_t offset);