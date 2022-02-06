#include "chunk.h"
#include "memory.h"
#include "vm.h"

DEFINE_DYNAMIC_ARRAY(Line, size_t)

void initChunk(Chunk* chunk) {
	initByteArray(&chunk->bytecode);
	initValueArray(&chunk->constants);
	initLineArray(&chunk->lines);
}

void freeChunk(VM* vm, Chunk* chunk) {
	freeByteArray(vm, &chunk->bytecode);
	freeValueArray(vm, &chunk->constants);
	freeLineArray(vm, &chunk->lines);
}

void addLineToTable(VM* vm, LineArray* table, size_t index, size_t line) {
	if (table->length == 0) {
		table->capacity = 8;
		table->items = GROW_ARRAY(vm, size_t, table->items, 0, table->capacity);
		table->items[0] = index;
		table->items[1] = line;
		table->length += 2;
	}
	else if (table->items[table->length - 1] != line) {
		if (table->capacity < table->length + 2) {
			size_t oldCapacity = table->capacity;
			table->capacity = GROW_CAPACITY(oldCapacity);
			table->items = GROW_ARRAY(vm, size_t, table->items, oldCapacity, table->capacity);
		}
		table->items[table->length] = index;
		table->items[table->length + 1] = line;
		table->length += 2;
	}
}

size_t addConstant(VM* vm, Chunk* chunk, Value constant, size_t line) {
	push(vm, constant);
	addLineToTable(vm, &chunk->lines, chunk->bytecode.length, line);
	writeValueArray(vm, &chunk->constants, constant);
	pop(vm);

	return chunk->constants.length - 1;
}

void writeOperand(VM* vm, Chunk* chunk, Opcode opcode, uint16_t operand, size_t line) {
	addLineToTable(vm, &chunk->lines, chunk->bytecode.length, line);
	writeByteArray(vm, &chunk->bytecode, opcode);
	writeByteArray(vm, &chunk->bytecode, (uint8_t)(operand >> 8));
	writeByteArray(vm, &chunk->bytecode, (uint8_t)(operand & 0xff));
}

void writeOpcode(VM* vm, Chunk* chunk, Opcode opcode, size_t line) {
	addLineToTable(vm, &chunk->lines, chunk->bytecode.length, line);
	writeByteArray(vm, &chunk->bytecode, opcode);
}

size_t getLineOfInstruction(Chunk* chunk, size_t index) {
	size_t offset = 0;

	while (offset < chunk->lines.length && chunk->lines.items[offset] <= index) {
		offset += 2;
	}

	return chunk->lines.items[offset - 1];
}