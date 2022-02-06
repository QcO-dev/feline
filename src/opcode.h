#pragma once
#include <assert.h>
#include "common.h"

typedef enum Opcode {
	// Constants
	OP_USE_CONSTANT,
	OP_NULL,
	OP_TRUE,
	OP_FALSE,
	// Stack Manipulation
	OP_POP,
	// Variables
	OP_DEFINE_GLOBAL,
	OP_ACCESS_GLOBAL,
	OP_ASSIGN_GLOBAL,
	OP_ACCESS_LOCAL,
	OP_ASSIGN_LOCAL,
	OP_ACCESS_UPVALUE,
	OP_ASSIGN_UPVALUE,
	OP_CLOSE_UPVALUE,
	// Jumps
	OP_JUMP,
	OP_JUMP_FALSE,
	OP_JUMP_FALSE_SC,
	OP_JUMP_TRUE_SC,
	OP_LOOP,
	// Operations
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_NEGATE,
	OP_NOT,
	OP_EQUAL,
	OP_NOT_EQUAL,
	OP_LESS,
	OP_LESS_EQUAL,
	OP_GREATER,
	OP_GREATER_EQUAL,
	// Functions
	OP_CLOSURE,
	OP_CALL,
	OP_RETURN,
	// Classes & Objects
	OP_CLASS,
	OP_INHERIT,
	OP_METHOD,
	OP_ACCESS_PROPERTY,
	OP_ASSIGN_PROPERTY,
	OP_ACCESS_SUPER,
	OP_INVOKE,
	OP_SUPER_INVOKE,
	// Lists
	OP_LIST,
	// Misc.
	OP_PRINT,

	OPCODE_COUNT
} Opcode;

static_assert(OPCODE_COUNT < UINT8_COUNT, "Opcode count may not exceed 256");