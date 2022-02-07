#include "disassemble.h"
#include "object.h"
#include <stdio.h>

static size_t constantInstruction(const char* name, VM* vm, Chunk* chunk, size_t offset) {
	uint16_t index = ((chunk->bytecode.items[offset + 1] << 8) | (chunk->bytecode.items[offset + 2]));

	Value value = chunk->constants.items[index];

	printf("%-15s %4d '", name, index);
	printValue(vm, value);
	printf("'");

	return offset + 3;
}

static size_t shortInstruction(const char* name, VM* vm, Chunk* chunk, size_t offset) {
	uint16_t index = ((chunk->bytecode.items[offset + 1] << 8) | (chunk->bytecode.items[offset + 2]));

	printf("%-15s %4d", name, index);

	return offset + 3;
}

static size_t jumpInstruction(const char* name, int64_t sign, VM* vm, Chunk* chunk, size_t offset) {
	uint16_t jump = ((chunk->bytecode.items[offset + 1] << 8) | (chunk->bytecode.items[offset + 2]));

	printf("%-15s %4zu -> %zu", name, offset, offset + 3 + sign * jump);

	return offset + 3;
}

static size_t byteInstruction(const char* name, VM* vm, Chunk* chunk, size_t offset) {
	uint8_t byte = chunk->bytecode.items[offset + 1];

	printf("%-15s %4d", name, byte);

	return offset + 2;
}

static size_t simpleInstruction(const char* name, VM* vm, Chunk* chunk, size_t offset) {
	printf("%-15s", name);
	return offset + 1;
}

size_t disassembleInstruction(VM* vm, Chunk* chunk, size_t offset) {
#define SIMPLE(x) case OP_##x: return simpleInstruction(#x, vm, chunk, offset);
#define CONSTANT(x) case OP_##x: return constantInstruction(#x, vm, chunk, offset);
#define SHORT(x) case OP_##x: return shortInstruction(#x, vm, chunk, offset);
#define JUMP(x, sign) case OP_##x: return jumpInstruction(#x, sign, vm, chunk, offset);
#define BYTE(x) case OP_##x: return byteInstruction(#x, vm, chunk, offset);

	printf("     %04X %4zu ", (int)offset, getLineOfInstruction(chunk, offset));

	Opcode opcode = chunk->bytecode.items[offset];

	switch (opcode) {
		CONSTANT(USE_CONSTANT)
		SIMPLE(NULL)
		SIMPLE(TRUE)
		SIMPLE(FALSE)
		SIMPLE(POP)
		CONSTANT(DEFINE_GLOBAL)
		CONSTANT(ACCESS_GLOBAL)
		CONSTANT(ASSIGN_GLOBAL)
		SHORT(ACCESS_LOCAL)
		SHORT(ASSIGN_LOCAL)
		SHORT(ACCESS_UPVALUE)
		SHORT(ASSIGN_UPVALUE)
		SIMPLE(CLOSE_UPVALUE)
		JUMP(JUMP, 1)
		JUMP(JUMP_FALSE, 1)
		JUMP(JUMP_FALSE_SC, 1)
		JUMP(JUMP_TRUE_SC, 1)
		JUMP(LOOP, -1)
		SIMPLE(ADD)
		SIMPLE(SUB)
		SIMPLE(MUL)
		SIMPLE(DIV)
		SIMPLE(NEGATE)
		SIMPLE(NOT)
		SIMPLE(EQUAL)
		SIMPLE(NOT_EQUAL)
		SIMPLE(LESS)
		SIMPLE(LESS_EQUAL)
		SIMPLE(GREATER)
		SIMPLE(GREATER_EQUAL)
		
		case OP_CLOSURE: {
			offset++;
			uint16_t constant = chunk->bytecode.items[offset++] << 8;
			constant |= chunk->bytecode.items[offset++];
			printf("%-15s %4d ", "CLOSURE", constant);
			printValue(vm, chunk->constants.items[constant]);

			ObjFunction* function = AS_FUNCTION(chunk->constants.items[constant]);

			for (size_t i = 0; i < function->upvalueCount; i++) {
				bool isLocal = chunk->bytecode.items[offset++];
				uint8_t index = chunk->bytecode.items[offset++];
				printf("\n     %04X      |                  %s %d", (int)offset - 2, isLocal ? "local" : "upvalue", index);
			}

			return offset;
		}

		BYTE(CALL)
		SIMPLE(RETURN)
		CONSTANT(CLASS)
		SIMPLE(INHERIT)
		CONSTANT(METHOD)
		CONSTANT(ACCESS_PROPERTY)
		CONSTANT(ASSIGN_PROPERTY)
		CONSTANT(ACCESS_SUPER)

		case OP_INVOKE: {
			uint16_t constant = ((chunk->bytecode.items[offset + 1] << 8) | (chunk->bytecode.items[offset + 2]));
			uint8_t argCount = chunk->bytecode.items[offset + 3];
			printf("%-15s (%d args) %4d '", "INVOKE", argCount, constant);
			printValue(vm, chunk->constants.items[constant]);
			printf("'");
			return offset + 4;
		}

		CONSTANT(SUPER_INVOKE)

		SHORT(LIST)
		SIMPLE(ACCESS_SUBSCRIPT)
		SIMPLE(ASSIGN_SUBSCRIPT)

		SIMPLE(THROW)
		JUMP(TRY_BEGIN, 1)
		SIMPLE(TRY_END)

		SIMPLE(PRINT)
		default: {
			printf("Unknown opcode: %2X", opcode);
			return offset + 1;
		}
	}
#undef SIMPLE
#undef CONSTANT
#undef SHORT
#undef JUMP
}

void disassemble(VM* vm, Chunk* chunk, const char* name) {
	printf("==== %s ====\n", name);
	size_t offset = 0;

	while (offset < chunk->bytecode.length) {
		offset = disassembleInstruction(vm, chunk, offset);
		printf("\n");
	}
}