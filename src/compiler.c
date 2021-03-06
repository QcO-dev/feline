#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "lexer.h"
#include "object.h"
#include "memory.h"
#ifdef FELINE_DEBUG_DISASSEMBLE
#include "disassemble.h"
#endif

typedef struct Local {
	Token name;
	int32_t depth;
	bool isCaptured;
} Local;

typedef struct Upvalue {
	uint8_t index;
	bool isLocal;
} Upvalue;

typedef enum FunctionType {
	TYPE_FUNCTION,
	TYPE_METHOD,
	TYPE_CONSTRUCTOR,
	TYPE_SCRIPT
} FunctionType;

typedef struct ClassCompiler {
	struct ClassCompiler* enclosing;
} ClassCompiler;

typedef struct Compiler {
	struct Compiler* enclosing;
	VM* vm;
	Lexer* lexer;
	Token current;
	Token previous;
	bool hasError;
	bool panic;

	ObjFunction* function;
	FunctionType type;

	ClassCompiler* currentClass;

	bool inTryBlock;

	bool isLoop;
	size_t continueJump;
	size_t breakJump;

	// TODO:
	//   Currently only supports 256 locals in the compiler
	//   The runtime environment can support up to 2^16
	//   Perhaps a dynamic array?
	Local locals[UINT8_COUNT];
	int32_t localCount;
	int32_t scopeDepth;

	Upvalue upvalues[UINT8_COUNT];
} Compiler;

typedef enum Precedence {
	PREC_NONE,
	PREC_ASSIGNMENT,  // =
	PREC_OR,          // ||
	PREC_AND,         // &&
	PREC_EQUALITY,    // == !=
	PREC_COMPARISON,  // < > <= >= instanceof
	PREC_TERM,        // + -
	PREC_FACTOR,      // * /
	PREC_UNARY,       // ! -
	PREC_CALL,        // . ()
	PREC_PRIMARY
} Precedence;

typedef void (*ParseFunction)(Compiler* compiler, bool canAssign);

typedef struct ParseRule {
	ParseFunction prefix;
	ParseFunction infix;
	Precedence precedence;
} ParseRule;

// ========= Forward Declarations =========

static void expression(Compiler* compiler);
static void parsePrecedence(Compiler* compiler, Precedence precedence);
static void statement(Compiler* compiler);
static void declaration(Compiler* compiler);
static void varDeclaration(Compiler* compiler);
static void expressionStatement(Compiler* compiler);
static void blockStatement(Compiler* compiler);

// ========= Helper Functions =========

static void initCompiler(VM* vm, Compiler* compiler, FunctionType type) {
	vm->lowestLevelCompiler = compiler;
	compiler->function = NULL;
	compiler->type = type;
	compiler->hasError = false;
	compiler->panic = false;
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	compiler->currentClass = NULL;
	compiler->inTryBlock = false;
	compiler->function = newFunction(compiler->vm);
	compiler->isLoop = false;

	if (type != TYPE_SCRIPT) {
		compiler->function->name = copyString(compiler->vm, compiler->previous.start, compiler->previous.length);
	}

	Local* local = &compiler->locals[compiler->localCount++];
	local->depth = 0;
	local->isCaptured = false;

	if (type == TYPE_METHOD || type == TYPE_CONSTRUCTOR) {
		local->name.start = "this";
		local->name.length = 4;
	}
	else {
		local->name.start = "";
		local->name.length = 0;
	}
}

static void inheritParserState(Compiler* outer, Compiler* inner) {
	inner->current = outer->current;
	inner->previous = outer->previous;
	inner->hasError = outer->hasError;
	inner->panic = outer->panic;
}

static void inheritCompiler(VM* vm, Compiler* outer, Compiler* inner, FunctionType type) {
	inner->vm = outer->vm;
	inner->lexer = outer->lexer;
	inheritParserState(outer, inner);
	inner->enclosing = outer;
	initCompiler(vm, inner, type);
	inner->currentClass = outer->currentClass;
}

static void errorAt(Compiler* compiler, Token* token, const char* message) {
	if (compiler->panic) return;
	compiler->panic = true;
	fprintf(stderr, "[%zu]: Error", token->line);

	if (token->type == TOKEN_EOF) {
		fprintf(stderr, " @ EOF");
	}
	else if (token->type == TOKEN_ERROR) {
		// Skip
	}
	else {
		fprintf(stderr, " @ '%.*s'", (int)token->length, token->start);
	}

	fprintf(stderr, ": %s\n", message);
	compiler->hasError = true;
}

static void error(Compiler* compiler, const char* message) {
	errorAt(compiler, &compiler->previous, message);
}

static void errorAtCurrent(Compiler* compiler, const char* message) {
	errorAt(compiler, &compiler->current, message);
}

static void advance(Compiler* compiler) {
	compiler->previous = compiler->current;

	for (;;) {
		compiler->current = lexToken(compiler->lexer);
		if (compiler->current.type != TOKEN_ERROR) break;

		errorAtCurrent(compiler, compiler->current.start);
	}
}

static void consume(Compiler* compiler, FelineTokenType type, const char* message) {
	if (compiler->current.type == type) {
		advance(compiler);
		return;
	}

	errorAtCurrent(compiler, message);
}

static bool check(Compiler* compiler, FelineTokenType type) {
	return compiler->current.type == type;
}

static bool match(Compiler* compiler, FelineTokenType type) {
	if (!check(compiler, type)) return false;
	advance(compiler);
	return true;
}

static Chunk* currentChunk(Compiler* compiler) {
	return &compiler->function->chunk;
}

static uint16_t makeConstant(Compiler* compiler, Value value) {
	size_t constant = addConstant(compiler->vm, currentChunk(compiler), value, compiler->previous.line);

	if (constant > UINT16_MAX) {
		error(compiler, "Too many constants in one code chunk - max is 65,536");
		return 0;
	}

	return (uint16_t)constant;
}

static Token syntheticToken(const char* text) {
	Token token;
	token.start = text;
	token.length = strlen(text);
	return token;
}

// ==== Emit bytes or Instructions ====

static void emitByte(Compiler* compiler, uint8_t byte) {
	writeOpcode(compiler->vm, currentChunk(compiler), byte, compiler->previous.line);
}

static void emitPair(Compiler* compiler, uint8_t a, uint8_t b) {
	emitByte(compiler, a);
	emitByte(compiler, b);
}

// OO -> Opcode operand -> An instruction, followed by a 16-bit number (e.g., a constant index)
static void emitOOInstruction(Compiler* compiler, Opcode opcode, uint16_t operand) {
	writeOperand(compiler->vm, currentChunk(compiler), opcode, operand, compiler->previous.line);
}

static void emitReturn(Compiler* compiler) {
	if (compiler->type == TYPE_CONSTRUCTOR) {
		emitOOInstruction(compiler, OP_ACCESS_LOCAL, 0);
	}
	else {
		emitByte(compiler, OP_NULL);
	}
	emitByte(compiler, OP_RETURN);
}

static void emitConstant(Compiler* compiler, Value value) {
	emitOOInstruction(compiler, OP_USE_CONSTANT, makeConstant(compiler, value));
}

static ObjFunction* endCompiler(Compiler* compiler, Compiler* outer) {
	emitReturn(compiler);

	ObjFunction* function = compiler->function;

#ifdef FELINE_DEBUG_DISASSEMBLE
	if(!compiler->hasError)
		disassemble(compiler->vm, currentChunk(compiler), function->name != NULL ? function->name->str : "<script>");
#endif

	if (outer != NULL) {
		inheritParserState(compiler, outer);
	}

	if (compiler->enclosing != NULL) {
		compiler->vm->lowestLevelCompiler = compiler->enclosing;
	}

	return function;
}

// ==== Variable Helpers ====

static bool identifiersEqual(Token* a, Token* b) {
	if (a->length != b->length) return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

static void addLocal(Compiler* compiler, Token name) {
	if (compiler->localCount == UINT8_COUNT) {
		error(compiler, "Too many local variables in function");
		return;
	}

	Local* local = &compiler->locals[compiler->localCount++];
	local->name = name;
	local->depth = -1;
	local->isCaptured = false;
}

static int32_t resolveLocal(Compiler* compiler, Token* name) {
	for (int32_t i = compiler->localCount - 1; i >= 0; i--) {
		Local* local = &compiler->locals[i];
		if (identifiersEqual(name, &local->name)) {
			if (local->depth == -1) {
				error(compiler, "Cannot access local variable before initialization");
			}
			return i;
		}
	}

	return -1;
}

static void markInitialized(Compiler* compiler) {
	if (compiler->scopeDepth == 0) return;

	compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;
}

static int32_t addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
	size_t upvalueCount = compiler->function->upvalueCount;

	for (size_t i = 0; i < upvalueCount; i++) {
		Upvalue* upvalue = &compiler->upvalues[i];
		if (upvalue->index == index && upvalue->isLocal == isLocal) {
			return (int32_t)i;
		}
	}

	if (upvalueCount == UINT8_COUNT) {
		error(compiler, "Too many closure variables in function");
		return 0;
	}

	compiler->upvalues[upvalueCount].isLocal = isLocal;
	compiler->upvalues[upvalueCount].index = index;
	return (int32_t)compiler->function->upvalueCount++;
}

static int32_t resolveUpvalue(Compiler* compiler, Token* name) {
	if (compiler->enclosing == NULL) return -1;

	int32_t local = resolveLocal(compiler->enclosing, name);
	if (local != -1) {
		compiler->enclosing->locals[local].isCaptured = true;
		return addUpvalue(compiler, (uint8_t)local, true);
	}

	int32_t upvalue = resolveUpvalue(compiler->enclosing, name);
	if (upvalue != -1) {
		return addUpvalue(compiler, (uint8_t)upvalue, false);
	}

	return -1;
}

static void declareVariable(Compiler* compiler) {
	if (compiler->scopeDepth == 0) return;

	Token* name = &compiler->previous;

	for (int32_t i = compiler->localCount - 1; i >= 0; i--) {
		Local* local = &compiler->locals[i];
		if (local->depth != -1 && local->depth < compiler->scopeDepth) {
			break;
		}

		if (identifiersEqual(name, &local->name)) {
			error(compiler, "Variable already defined in the current scope");
		}
	}

	addLocal(compiler, *name);
}

static void defineVariable(Compiler* compiler, uint16_t global) {
	if (compiler->scopeDepth > 0) {
		markInitialized(compiler);
		return;
	}

	emitOOInstruction(compiler, OP_DEFINE_GLOBAL, global);
}

static uint16_t identifierConstant(Compiler* compiler, Token* name) {
	ObjString* str = copyString(compiler->vm, name->start, name->length);
	return makeConstant(compiler, OBJ_VAL(str));
}

static uint16_t parseVariable(Compiler* compiler, const char* errorMsg) {
	consume(compiler, TOKEN_IDENTIFIER, errorMsg);

	declareVariable(compiler);
	
	if (compiler->scopeDepth > 0) return 0;

	return identifierConstant(compiler, &compiler->previous);
}

static void namedVariable(Compiler* compiler, Token name, bool canAssign) {
	Opcode accessOp, assignOp;

	int32_t arg = resolveLocal(compiler, &name);
	
	if (arg != -1) {
		accessOp = OP_ACCESS_LOCAL;
		assignOp = OP_ASSIGN_LOCAL;
	}
	else if ((arg = resolveUpvalue(compiler, &name)) != -1) {
		accessOp = OP_ACCESS_UPVALUE;
		assignOp = OP_ASSIGN_UPVALUE;
	}
	else {
		arg = identifierConstant(compiler, &name);
		accessOp = OP_ACCESS_GLOBAL;
		assignOp = OP_ASSIGN_GLOBAL;
	}

	if (canAssign && match(compiler, TOKEN_EQUAL)) {
		expression(compiler);
		emitOOInstruction(compiler, assignOp, (uint16_t)arg);
	}
	else {
		emitOOInstruction(compiler, accessOp, (uint16_t)arg);
	}
}

// ==== Scoping ====

static void beginScope(Compiler* compiler) {
	compiler->scopeDepth++;
}

static void endScope(Compiler* compiler) {
	compiler->scopeDepth--;

	while (compiler->localCount > 0 && compiler->locals[compiler->localCount - 1].depth > compiler->scopeDepth) {
		if (compiler->locals[compiler->localCount - 1].isCaptured) {
			emitByte(compiler, OP_CLOSE_UPVALUE);
		}
		else {
			emitByte(compiler, OP_POP);
		}
		compiler->localCount--;
	}
}

static void emitScopeEnd(Compiler* compiler) {
	int32_t depth = compiler->scopeDepth - 1;
	int32_t count = compiler->localCount;

	while (count > 0 && compiler->locals[count - 1].depth > depth) {
		if (compiler->locals[count - 1].isCaptured) {
			emitByte(compiler, OP_CLOSE_UPVALUE);
		}
		else {
			emitByte(compiler, OP_POP);
		}
		count--;
	}
}

// ==== Jumps ====

static size_t emitJump(Compiler* compiler, Opcode opcode) {
	emitOOInstruction(compiler, opcode, 0xffff);
	return currentChunk(compiler)->bytecode.length - 2;
}

static void patchJump(Compiler* compiler, size_t jump) {
	size_t jumpSize = currentChunk(compiler)->bytecode.length - jump - 2;

	if (jumpSize > UINT16_MAX) {
		error(compiler, "Too much code in jump");
	}

	currentChunk(compiler)->bytecode.items[jump] = (jumpSize >> 8) & 0xff;
	currentChunk(compiler)->bytecode.items[jump + 1] = jumpSize & 0xff;
}

static void emitLoop(Compiler* compiler, size_t loopStart) {
	size_t offset = currentChunk(compiler)->bytecode.length - loopStart + 3;

	if (offset > UINT16_MAX) error(compiler, "Too much code in loop");
	
	emitOOInstruction(compiler, OP_LOOP, (uint16_t)offset);
}

// ==== Arguments ====

static uint8_t argumentList(Compiler* compiler) {
	uint8_t argCount = 0;

	if (!check(compiler, TOKEN_RIGHT_PAREN)) {
		do {
			expression(compiler);
			if (argCount == UINT8_MAX) {
				error(compiler, "Cannot have more than 255 arguments");
			}
			argCount++;
		} while (match(compiler, TOKEN_COMMA));
	}

	consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after arguments");
	return argCount;
}

// ========= Compilation =========

static ParseRule* getRule(FelineTokenType type);

// ==== Single-token values -> literals ====

static void number(Compiler* compiler, bool canAssign) {
	double value = strtod(compiler->previous.start, NULL);
	emitConstant(compiler, NUMBER_VAL(value));
}

static ObjString* normString(Compiler* compiler, Token strToken) {
	ByteArray chars;

	initByteArray(&chars);

#define ESCAPE(c, e) case c: writeByteArray(compiler->vm, &chars, e); break

	for (int i = 1; i < strToken.length - 1; i++) {
		if (strToken.start[i] == '\\') {
			switch (strToken.start[++i]) {
				ESCAPE('b', '\b');
				ESCAPE('f', '\f');
				ESCAPE('n', '\n');
				ESCAPE('r', '\r'); 
				ESCAPE('t', '\t');
				ESCAPE('v', '\v');
				ESCAPE('\\', '\\');
				ESCAPE('\'', '\'');
				ESCAPE('"', '"');
				ESCAPE('0', '\0');
				default: {
					error(compiler, "Invalid escape sequence");
				}
			}
		}
		else {
			writeByteArray(compiler->vm, &chars, strToken.start[i]);
		}
	}

#undef ESCAPE

	ObjString* str = copyString(compiler->vm, chars.items, chars.length);
	freeByteArray(compiler->vm, &chars);
	return str;
}

static void string(Compiler* compiler, bool canAssign) {
	// The +1 and - 2 remove the double quotes from the lexeme
	ObjString* str = normString(compiler, compiler->previous);
	emitConstant(compiler, OBJ_VAL(str));
}

static void literal(Compiler* compiler, bool canAssign) {
	switch (compiler->previous.type) {
		case TOKEN_FALSE: emitByte(compiler, OP_FALSE); break;
		case TOKEN_NULL: emitByte(compiler, OP_NULL); break;
		case TOKEN_TRUE: emitByte(compiler, OP_TRUE); break;
		default: ASSERT(0, "Unreachable");
	}
}

static void variable(Compiler* compiler, bool canAssign) {
	namedVariable(compiler, compiler->previous, canAssign);
}

static void this_(Compiler* compiler, bool canAssign) {
	if (compiler->currentClass == NULL) {
		error(compiler, "Cannot use 'this' outside of a class");
		return;
	}

	variable(compiler, false);
}

static void super_(Compiler* compiler, bool canAssign) {
	if (compiler->currentClass == NULL) {
		error(compiler, "Cannot use 'super' outside of a class");
	}

	consume(compiler, TOKEN_DOT, "Expected '.' after 'super'");
	consume(compiler, TOKEN_IDENTIFIER, "Expected superclass method name");
	uint16_t name = identifierConstant(compiler, &compiler->previous);

	namedVariable(compiler, syntheticToken("this"), false);
	
	if (match(compiler, TOKEN_LEFT_PAREN)) {
		uint8_t argCount = argumentList(compiler);
		namedVariable(compiler, syntheticToken("super"), false);
		emitOOInstruction(compiler, OP_SUPER_INVOKE, name);
		emitByte(compiler, argCount);
	}
	else {
		namedVariable(compiler, syntheticToken("super"), false);
		emitOOInstruction(compiler, OP_ACCESS_SUPER, name);
	}
}

// ==== Object Property Assignment & Creation ( { ... } )

static void objectPropertyAssign(Compiler* compiler, bool canAssign) {

	if (!check(compiler, TOKEN_RIGHT_BRACE)) {
		do {
			uint16_t key;
			if (match(compiler, TOKEN_STRING)) {
				ObjString* str = normString(compiler, compiler->previous);
				key = makeConstant(compiler, OBJ_VAL(str));

				consume(compiler, TOKEN_COLON, "Expected ':' between key-value pair");

				expression(compiler);
			}
			else {
				consume(compiler, TOKEN_IDENTIFIER, "Expected identifier key for key-value pair");

				Token keyToken = compiler->previous;
				key = identifierConstant(compiler, &compiler->previous);

				if (match(compiler, TOKEN_COLON)) {
					expression(compiler);
				}
				else {
					namedVariable(compiler, keyToken, false);
				}
			}

			emitOOInstruction(compiler, OP_ASSIGN_PROPERTY_KV, key);
		} while (match(compiler, TOKEN_COMMA));
	}

	consume(compiler, TOKEN_RIGHT_BRACE, "Expected '}' after object body");
}

static void objectCreation(Compiler* compiler, bool canAssign) {
	emitByte(compiler, OP_CREATE_OBJECT);

	objectPropertyAssign(compiler, canAssign);
}

// ==== Expression values ====

static void grouping(Compiler* compiler, bool canAssign) {
	expression(compiler);
	consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

static void call(Compiler* compiler, bool canAssign) {
	uint8_t argCount = argumentList(compiler);
	emitPair(compiler, OP_CALL, argCount);
}

static void dot(Compiler* compiler, bool canAssign) {
	consume(compiler, TOKEN_IDENTIFIER, "Expected property name after '.'");

	uint16_t name = identifierConstant(compiler, &compiler->previous);

	if (canAssign && match(compiler, TOKEN_EQUAL)) {
		expression(compiler);
		emitOOInstruction(compiler, OP_ASSIGN_PROPERTY, name);
	}
	else if (match(compiler, TOKEN_LEFT_PAREN)) {
		uint8_t argCount = argumentList(compiler);
		emitOOInstruction(compiler, OP_INVOKE, name);
		emitByte(compiler, argCount);
	}
	else {
		emitOOInstruction(compiler, OP_ACCESS_PROPERTY, name);
	}
}

static void unary(Compiler* compiler, bool canAssign) {
	FelineTokenType opType = compiler->previous.type;

	parsePrecedence(compiler, PREC_UNARY);

	switch (opType) {
		case TOKEN_MINUS: emitByte(compiler, OP_NEGATE); break;
		case TOKEN_BANG: emitByte(compiler, OP_NOT); break;
		default: {
			ASSERT(0, "Unreachable");
			return;
		}
	}
}

static void binary(Compiler* compiler, bool canAssign) {
	FelineTokenType opType = compiler->previous.type;
	ParseRule* rule = getRule(opType);

	parsePrecedence(compiler, (Precedence)(rule->precedence + 1));

	switch (opType) {
		case TOKEN_PLUS: emitByte(compiler, OP_ADD); break;
		case TOKEN_MINUS: emitByte(compiler, OP_SUB); break;
		case TOKEN_STAR: emitByte(compiler, OP_MUL); break;
		case TOKEN_SLASH: emitByte(compiler, OP_DIV); break;
		case TOKEN_EQUAL_EQUAL: emitByte(compiler, OP_EQUAL); break;
		case TOKEN_BANG_EQUAL: emitByte(compiler, OP_NOT_EQUAL); break;
		case TOKEN_LESS: emitByte(compiler, OP_LESS); break;
		case TOKEN_LESS_EQUAL: emitByte(compiler, OP_LESS_EQUAL); break;
		case TOKEN_GREATER: emitByte(compiler, OP_GREATER); break;
		case TOKEN_GREATER_EQUAL: emitByte(compiler, OP_GREATER_EQUAL); break;
		case TOKEN_INSTANCEOF: emitByte(compiler, OP_INSTANCEOF); break;
		default: {
			ASSERT(0, "Unreachable");
			return;
		}
	}
}

static void logicalAnd(Compiler* compiler, bool canAssign) {
	size_t endJump = emitJump(compiler, OP_JUMP_FALSE_SC);
	emitByte(compiler, OP_POP);

	parsePrecedence(compiler, PREC_AND);
	patchJump(compiler, endJump);
}

static void logicalOr(Compiler* compiler, bool canAssign) {
	size_t endJump = emitJump(compiler, OP_JUMP_TRUE_SC);
	emitByte(compiler, OP_POP);

	parsePrecedence(compiler, PREC_OR);
	patchJump(compiler, endJump);
}

static void list(Compiler* compiler, bool canAssign) {
	uint16_t length = 0;

	if (!check(compiler, TOKEN_RIGHT_SQUARE)) {
		do {
			expression(compiler);
			if (length == UINT16_MAX) {
				error(compiler, "Cannot have more than 65,536 items in list literal");
			}
			length++;
		} while (match(compiler, TOKEN_COMMA));
	}

	consume(compiler, TOKEN_RIGHT_SQUARE, "Expected ')' after arguments");

	emitOOInstruction(compiler, OP_LIST, length);
}

static void subscript(Compiler* compiler, bool canAssign) {
	expression(compiler);

	consume(compiler, TOKEN_RIGHT_SQUARE, "Expected ']' after subscript");

	if (canAssign && match(compiler, TOKEN_EQUAL)) {
		expression(compiler);
		emitByte(compiler, OP_ASSIGN_SUBSCRIPT);
	}
	else {
		emitByte(compiler, OP_ACCESS_SUBSCRIPT);
	}
}

static void parsePrecedence(Compiler* compiler, Precedence precedence) {
	advance(compiler);

	ParseFunction prefixRule = getRule(compiler->previous.type)->prefix;
	
	if (prefixRule == NULL) {
		error(compiler, "Expected expression");
		return;
	}

	bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(compiler, canAssign);

	while (precedence <= getRule(compiler->current.type)->precedence) {
		advance(compiler);
		ParseFunction infixRule = getRule(compiler->previous.type)->infix;
		infixRule(compiler, canAssign);
	}

	if (canAssign && match(compiler, TOKEN_EQUAL)) {
		error(compiler, "Invalid assignment target");
	}
}

static void expression(Compiler* compiler) {
	parsePrecedence(compiler, PREC_ASSIGNMENT);
}

// ========= Statements =========

static void printStatement(Compiler* compiler) {
	expression(compiler);
	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after expression");
	emitByte(compiler, OP_PRINT);
}

static void breakStatement(Compiler* compiler) {
	if (!compiler->isLoop) error(compiler, "Use of 'break' is not permitted outside of loops");
	emitByte(compiler, OP_FALSE);
	emitLoop(compiler, compiler->breakJump - 1);
	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after break");
}

static void continueStatement(Compiler* compiler) {
	if (!compiler->isLoop) error(compiler, "Use of 'continue' is not permitted outside of loops");
	emitLoop(compiler, compiler->continueJump);
	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after continue");
}

static void exportStatement(Compiler* compiler) {
	expression(compiler);

	consume(compiler, TOKEN_AS, "Expected 'as' after export value");

	consume(compiler, TOKEN_IDENTIFIER, "Expected export bind name");

	uint16_t bindName = identifierConstant(compiler, &compiler->previous);

	emitOOInstruction(compiler, OP_EXPORT, bindName);

	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after export name");
}

static void ifStatement(Compiler* compiler) {
	consume(compiler, TOKEN_LEFT_PAREN, "Expected '(' after 'if'");

	expression(compiler);

	consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after condition");

	size_t thenJump = emitJump(compiler, OP_JUMP_FALSE);
	
	statement(compiler);

	size_t elseJump = emitJump(compiler, OP_JUMP);

	patchJump(compiler, thenJump);

	if (match(compiler, TOKEN_ELSE)) statement(compiler);
	
	patchJump(compiler, elseJump);
}

static void forStatement(Compiler* compiler) {
	bool wasLoop = compiler->isLoop;
	size_t prevContinueJump = compiler->continueJump;
	size_t prevBreakJump = compiler->breakJump;
	compiler->isLoop = true;

	beginScope(compiler);
	consume(compiler, TOKEN_LEFT_PAREN, "Expected '(' after 'for'");
	
	if (match(compiler, TOKEN_SEMICOLON)) {
		// Blank initialiser clause
	}
	else if (match(compiler, TOKEN_VAR)) {
		varDeclaration(compiler);
	}
	else {
		expressionStatement(compiler);
	}

	size_t loopStart = currentChunk(compiler)->bytecode.length;

	int64_t exitJump;
	if (!match(compiler, TOKEN_SEMICOLON)) {
		expression(compiler);
		consume(compiler, TOKEN_SEMICOLON, "Expected ';' after for condition");

		exitJump = emitJump(compiler, OP_JUMP_FALSE);
		compiler->breakJump = exitJump;
	}
	else {
		emitByte(compiler, OP_TRUE);
		exitJump = emitJump(compiler, OP_JUMP_FALSE);
		compiler->breakJump = exitJump;
	}

	if (!match(compiler, TOKEN_RIGHT_PAREN)) {
		// Skip over the increment on first run but on loop execute it
		// This is done because the increment appears before the body in the source code
		// But must be executed afterwards.
		size_t bodyJump = emitJump(compiler, OP_JUMP);
		size_t incrementStart = currentChunk(compiler)->bytecode.length;

		expression(compiler);
		emitByte(compiler, OP_POP);

		consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after for clauses");

		emitLoop(compiler, loopStart);
		loopStart = incrementStart;
		patchJump(compiler, bodyJump);
	}
	compiler->continueJump = loopStart;

	statement(compiler);

	emitLoop(compiler, loopStart);

	patchJump(compiler, exitJump);

	endScope(compiler);

	compiler->isLoop = wasLoop;
	compiler->continueJump = prevContinueJump;
	compiler->breakJump = prevBreakJump;
}

static void whileStatement(Compiler* compiler) {
	bool wasLoop = compiler->isLoop;
	size_t prevContinueJump = compiler->continueJump;
	size_t prevBreakJump = compiler->breakJump;
	compiler->isLoop = true;

	size_t loopStart = currentChunk(compiler)->bytecode.length;
	compiler->continueJump = loopStart;

	consume(compiler, TOKEN_LEFT_PAREN, "Expected '(' after 'while'");
	expression(compiler);
	consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after condition");

	size_t exitJump = emitJump(compiler, OP_JUMP_FALSE);
	compiler->breakJump = exitJump;
	
	statement(compiler);

	emitLoop(compiler, loopStart);

	patchJump(compiler, exitJump);

	compiler->isLoop = wasLoop;
	compiler->continueJump = prevContinueJump;
	compiler->breakJump = prevBreakJump;
}

static void returnStatement(Compiler* compiler) {
	if (compiler->type == TYPE_SCRIPT) {
		error(compiler, "Cannot return from top-level code");
	}

	if (match(compiler, TOKEN_SEMICOLON)) {
		emitReturn(compiler);
	}
	else {
		if (compiler->type == TYPE_CONSTRUCTOR) {
			error(compiler, "Cannot return a value from a constructor");
		}

		expression(compiler);
		consume(compiler, TOKEN_SEMICOLON, "Expected ';' after return value");
		emitByte(compiler, OP_RETURN);
	}
}

static void throwStatement(Compiler* compiler) {
	expression(compiler);

	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after throw");
	emitByte(compiler, OP_THROW);
}

static void tryStatement(Compiler* compiler) {
	if (compiler->inTryBlock) {
		error(compiler, "Cannot nest try blocks");
	}

	// try ...
	size_t tryBegin = emitJump(compiler, OP_TRY_BEGIN);

	compiler->inTryBlock = true;

	if (!match(compiler, TOKEN_LEFT_BRACE)) {
		error(compiler, "Expected '{' after 'try'");
	}

	beginScope(compiler);
	blockStatement(compiler);

	emitScopeEnd(compiler);

	compiler->inTryBlock = false;

	size_t catchJump = emitJump(compiler, OP_JUMP);

	emitByte(compiler, OP_TRY_END);

	patchJump(compiler, tryBegin);

	endScope(compiler);
	// catch(e) ...

	consume(compiler, TOKEN_CATCH, "Expected catch after try statement");

	beginScope(compiler);

	if (match(compiler, TOKEN_LEFT_PAREN)) {
		uint16_t boundCatchVariable = parseVariable(compiler, "Expected catch binding name");
		emitByte(compiler, OP_BOUND_EXCEPTION);
		defineVariable(compiler, boundCatchVariable);

		consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after catch variable");
	}

	statement(compiler);

	endScope(compiler);

	patchJump(compiler, catchJump);

	if (match(compiler, TOKEN_FINALLY)) {
		statement(compiler);
	}
}

static void expressionStatement(Compiler* compiler) {
	expression(compiler);
	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after expression");
	emitByte(compiler, OP_POP);
}

static void blockStatement(Compiler* compiler) {
	while (!check(compiler, TOKEN_RIGHT_BRACE) && !check(compiler, TOKEN_EOF)) {
		declaration(compiler);
	}

	consume(compiler, TOKEN_RIGHT_BRACE, "Expected '}' after block");
}

static void statement(Compiler* compiler) {
	if (match(compiler, TOKEN_PRINT)) {
		printStatement(compiler);
	}
	else if (match(compiler, TOKEN_BREAK)) {
		breakStatement(compiler);
	}
	else if (match(compiler, TOKEN_CONTINUE)) {
		continueStatement(compiler);
	}
	else if (match(compiler, TOKEN_EXPORT)) {
		exportStatement(compiler);
	}
	else if (match(compiler, TOKEN_FOR)) {
		forStatement(compiler);
	}
	else if (match(compiler, TOKEN_IF)) {
		ifStatement(compiler);
	}
	else if (match(compiler, TOKEN_RETURN)) {
		returnStatement(compiler);
	}
	else if (match(compiler, TOKEN_THROW)) {
		throwStatement(compiler);
	}
	else if (match(compiler, TOKEN_TRY)) {
		tryStatement(compiler);
	}
	else if (match(compiler, TOKEN_WHILE)) {
		whileStatement(compiler);
	}
	else if (match(compiler, TOKEN_LEFT_BRACE)) {
		beginScope(compiler);
		blockStatement(compiler);
		endScope(compiler);
	}
	else {
		expressionStatement(compiler);
	}
}

// ========= Declarations =========

static void synchronize(Compiler* compiler) {
	compiler->panic = false;

	while (compiler->current.type != TOKEN_EOF) {
		if (compiler->previous.type == TOKEN_SEMICOLON) return;
		switch (compiler->current.type) {
			case TOKEN_PRINT:
			return;

			default:; // Do nothing
		}

		advance(compiler);
	}
}

static void function(Compiler* outerCompiler, FunctionType type) {
	Compiler c;
	Compiler* compiler = &c;
	inheritCompiler(outerCompiler->vm, outerCompiler, compiler, type);

	beginScope(compiler);

	consume(compiler, TOKEN_LEFT_PAREN, "Expected '(' after function name");
	
	if (!check(compiler, TOKEN_RIGHT_PAREN)) {
		do {
			compiler->function->arity++;
			if (compiler->function->arity > UINT8_MAX) {
				errorAtCurrent(compiler, "Function cannot have more than 255 parameters");
			}
			uint16_t constant = parseVariable(compiler, "Expected parameter name");
			defineVariable(compiler, constant);

		} while (match(compiler, TOKEN_COMMA));
	}
	consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after parameters");

	consume(compiler, TOKEN_LEFT_BRACE, "Expected '{' before function body");

	blockStatement(compiler);

	ObjFunction* function = endCompiler(compiler, outerCompiler);

	emitOOInstruction(outerCompiler, OP_CLOSURE, makeConstant(outerCompiler, OBJ_VAL(function)));

	for (size_t i = 0; i < function->upvalueCount; i++) {
		emitByte(outerCompiler, compiler->upvalues[i].isLocal ? 1 : 0);
		emitByte(outerCompiler, compiler->upvalues[i].index);
	}
}

static void method(Compiler* compiler) {
	consume(compiler, TOKEN_IDENTIFIER, "Expected method name");
	uint16_t constant = identifierConstant(compiler, &compiler->previous);

	FunctionType type = TYPE_METHOD;

	if (compiler->previous.length == 3 && memcmp(compiler->previous.start, "new", 3) == 0) {
		type = TYPE_CONSTRUCTOR;
	}

	function(compiler, type);

	emitOOInstruction(compiler, OP_METHOD, constant);
}

static void nativeMethod(Compiler* compiler) {
	consume(compiler, TOKEN_IDENTIFIER, "Expected method name");
	uint16_t name = identifierConstant(compiler, &compiler->previous);

	consume(compiler, TOKEN_LEFT_PAREN, "Expected '(' after function name");

	uint16_t arity = 0;
	if (!check(compiler, TOKEN_RIGHT_PAREN)) {
		do {
			arity++;
			if (arity > UINT8_MAX) {
				errorAtCurrent(compiler, "Function cannot have more than 255 parameters");
			}
			consume(compiler, TOKEN_IDENTIFIER, "Expected parameter name");
		} while (match(compiler, TOKEN_COMMA));
	}
	consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after parameters");

	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after native function definition");

	emitOOInstruction(compiler, OP_CLASS_NATIVE, name);
	emitByte(compiler, (uint8_t)arity);

	emitOOInstruction(compiler, OP_METHOD, name);
}

static void classDeclaration(Compiler* compiler) {
	consume(compiler, TOKEN_IDENTIFIER, "Expected class name");
	Token className = compiler->previous;
	uint16_t nameConstant = identifierConstant(compiler, &compiler->previous);
	declareVariable(compiler);

	emitOOInstruction(compiler, OP_CLASS, nameConstant);
	defineVariable(compiler, nameConstant);

	ClassCompiler classCompiler;
	classCompiler.enclosing = compiler->currentClass;
	compiler->currentClass = &classCompiler;

	if (match(compiler, TOKEN_COLON)) {
		consume(compiler, TOKEN_IDENTIFIER, "Expected superclass name");
		variable(compiler, false);

		if (identifiersEqual(&className, &compiler->previous)) {
			error(compiler, "A class cannot inherit from itself");
		}
	}
	else {
		emitByte(compiler, OP_OBJECT);
	}

	beginScope(compiler);
	addLocal(compiler, syntheticToken("super"));
	defineVariable(compiler, 0);

	namedVariable(compiler, className, false);
	emitByte(compiler, OP_INHERIT);

	namedVariable(compiler, className, false);

	consume(compiler, TOKEN_LEFT_BRACE, "Expected '{' before class body");

	while (!check(compiler, TOKEN_RIGHT_BRACE) && !check(compiler, TOKEN_EOF)) {
		if (match(compiler, TOKEN_NATIVE)) {
			nativeMethod(compiler);
		}
		else {
			method(compiler);
		}
	}

	consume(compiler, TOKEN_RIGHT_BRACE, "Expected '}' after class body");

	emitByte(compiler, OP_POP);

	endScope(compiler);

	compiler->currentClass = compiler->currentClass->enclosing;
}

static void functionDeclaration(Compiler* compiler) {
	uint16_t global = parseVariable(compiler, "Expected function name");

	markInitialized(compiler);

	function(compiler, TYPE_FUNCTION);

	defineVariable(compiler, global);
}

static void importDeclaration(Compiler* compiler) {
	char buffer[1024];
	size_t length = 0;

	do {
		consume(compiler, TOKEN_IDENTIFIER, "Expected import name");

		Token part = compiler->previous;

		if (length + part.length + 1 > 1024) {
			error(compiler, "Import path exceeds maximum length of 1024 characters");
			break;
		}

		memcpy(buffer + length, part.start, part.length);
		length += part.length;
		buffer[length] = '/';
		length += 1;
	} while (match(compiler, TOKEN_DOT));

	ObjString* path = copyString(compiler->vm, buffer, length - 1);
	push(compiler->vm, OBJ_VAL(path));

	if (match(compiler, TOKEN_AS)) {
		consume(compiler, TOKEN_IDENTIFIER, "Expected import name");
	}

	uint16_t name = identifierConstant(compiler, &compiler->previous);

	emitOOInstruction(compiler, OP_IMPORT, makeConstant(compiler, OBJ_VAL(path)));

	pop(compiler->vm);

	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after import declaration");

	defineVariable(compiler, name);
}

static void nativeDeclaration(Compiler* compiler) {
	uint16_t name = parseVariable(compiler, "Expected native function name");

	consume(compiler, TOKEN_LEFT_PAREN, "Expected '(' after function name");

	uint16_t arity = 0;
	if (!check(compiler, TOKEN_RIGHT_PAREN)) {
		do {
			arity++;
			if (arity > UINT8_MAX) {
				errorAtCurrent(compiler, "Function cannot have more than 255 parameters");
			}
			consume(compiler, TOKEN_IDENTIFIER, "Expected parameter name");
		} while (match(compiler, TOKEN_COMMA));
	}
	consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after parameters");

	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after native function definition");

	emitOOInstruction(compiler, OP_NATIVE, name);
	emitByte(compiler, (uint8_t)arity);

	defineVariable(compiler, name);
}

static void varDeclaration(Compiler* compiler) {
	uint16_t global = parseVariable(compiler, "Expected variable name");

	if (match(compiler, TOKEN_EQUAL)) {
		expression(compiler);
	}
	else {
		emitByte(compiler, OP_NULL);
	}
	consume(compiler, TOKEN_SEMICOLON, "Expected ';' after variable declaration");

	defineVariable(compiler, global);
}

static void declaration(Compiler* compiler) {
	if (match(compiler, TOKEN_CLASS)) {
		classDeclaration(compiler);
	}
	else if (match(compiler, TOKEN_FUNCTION)) {
		functionDeclaration(compiler);
	}
	else if (match(compiler, TOKEN_IMPORT)) {
		importDeclaration(compiler);
	}
	else if (match(compiler, TOKEN_NATIVE)) {
		nativeDeclaration(compiler);
	}
	else if (match(compiler, TOKEN_VAR)) {
		varDeclaration(compiler);
	}
	else {
		statement(compiler);
	}

	if (compiler->panic) synchronize(compiler);
}

// ========= Parser Rules =========

ParseRule rules[] = {
	[TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
	[TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
	[TOKEN_LEFT_BRACE] = {objectCreation, objectPropertyAssign, PREC_CALL},
	[TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
	[TOKEN_LEFT_SQUARE] = {list, subscript, PREC_CALL},
	[TOKEN_RIGHT_SQUARE] = {NULL, NULL, PREC_NONE},
	[TOKEN_PLUS] = {NULL, binary, PREC_TERM},
	[TOKEN_MINUS] = {unary, binary, PREC_TERM},
	[TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
	[TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
	[TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
	[TOKEN_COLON] = {NULL, NULL, PREC_NONE},
	[TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
	[TOKEN_DOT] = {NULL, dot, PREC_CALL},
	[TOKEN_BANG] = {unary, NULL, PREC_NONE},
	[TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
	[TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
	[TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
	[TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
	[TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
	[TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
	[TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
	[TOKEN_AMP] = {NULL, NULL, PREC_NONE},
	[TOKEN_AMP_AMP] = {NULL, logicalAnd, PREC_AND},
	[TOKEN_BAR] = {NULL, NULL, PREC_NONE},
	[TOKEN_BAR_BAR] = {NULL, logicalOr, PREC_OR},

	[TOKEN_STRING] = {string, NULL, PREC_NONE},
	[TOKEN_NUMBER] = {number, NULL, PREC_NONE},

	[TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
	[TOKEN_AS] = {NULL, NULL, PREC_NONE},
	[TOKEN_CATCH] = {NULL, NULL, PREC_NONE},
	[TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
	[TOKEN_CONTINUE] = {NULL, NULL, PREC_NONE},
	[TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
	[TOKEN_EXPORT] = {NULL, NULL, PREC_NONE},
	[TOKEN_FALSE] = {literal, NULL, PREC_NONE},
	[TOKEN_FINALLY] = {NULL, NULL, PREC_NONE},
	[TOKEN_FOR] = {NULL, NULL, PREC_NONE},
	[TOKEN_FUNCTION] = {NULL, NULL, PREC_NONE},
	[TOKEN_IF] = {NULL, NULL, PREC_NONE},
	[TOKEN_IMPORT] = {NULL, NULL, PREC_NONE},
	[TOKEN_INSTANCEOF] = {NULL, binary, PREC_COMPARISON},
	[TOKEN_NATIVE] = {NULL, NULL, PREC_NONE},
	[TOKEN_NULL] = {literal, NULL, PREC_NONE},
	[TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
	[TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
	[TOKEN_SUPER] = {super_, NULL, PREC_NONE},
	[TOKEN_THIS] = {this_, NULL, PREC_NONE},
	[TOKEN_THROW] = {NULL, NULL, PREC_NONE},
	[TOKEN_TRUE] = {literal, NULL, PREC_NONE},
	[TOKEN_TRY] = {NULL, NULL, PREC_NONE},
	[TOKEN_VAR] = {NULL, NULL, PREC_NONE},
	[TOKEN_WHILE] = {NULL, NULL, PREC_NONE},

	[TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
	[TOKEN_EOF] = {NULL, NULL, PREC_NONE}
};

static_assert(56 == TOKEN__COUNT, "Handling of tokens in rules[] does not handle all tokens exactly once");

static ParseRule* getRule(FelineTokenType type) {
	return &rules[type];
}

ObjFunction* compile(VM* vm, const char* source) {
	Lexer lexer;
	initLexer(&lexer, source);

	Compiler compiler;
	compiler.vm = vm;
	vm->lowestLevelCompiler = &compiler;
	compiler.enclosing = NULL;
	initCompiler(vm, &compiler, TYPE_SCRIPT);
	compiler.lexer = &lexer;

	advance(&compiler);
	
	while (!match(&compiler, TOKEN_EOF)) {
		declaration(&compiler);
	}

	ObjFunction* function = endCompiler(&compiler, NULL);

	vm->lowestLevelCompiler = NULL;

	return compiler.hasError ? NULL : function;
}

void markCompilerRoots(VM* vm) {
	Compiler* compiler = vm->lowestLevelCompiler;

	while (compiler != NULL) {
		markObject(vm, (Obj*)compiler->function);
		compiler = compiler->enclosing;
	}
}