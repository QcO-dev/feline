#pragma once
#include "common.h"
#include "darray.h"
#include "value.h"
#include "opcode.h"

DECLARE_DYNAMIC_ARRAY(Line, size_t)

typedef struct Chunk {
	ByteArray bytecode;
	ValueArray constants;
	LineArray lines;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(VM* vm, Chunk* chunk);
size_t addConstant(VM* vm, Chunk* chunk, Value constant, size_t line);
void writeOperand(VM* vm, Chunk* chunk, Opcode opcode, uint16_t operand, size_t line);
void writeOpcode(VM* vm, Chunk* chunk, Opcode opcode, size_t line);

size_t getLineOfInstruction(Chunk* chunk, size_t index);