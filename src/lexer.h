#pragma once
#include "common.h"

typedef struct Lexer {
	const char* start;
	const char* current;
	size_t line;
} Lexer;

typedef enum FelineTokenType {
	TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
	TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
	TOKEN_LEFT_SQUARE, TOKEN_RIGHT_SQUARE,
	TOKEN_PLUS, TOKEN_MINUS,
	TOKEN_STAR, TOKEN_SLASH,
	TOKEN_SEMICOLON, TOKEN_COLON,
	TOKEN_COMMA,
	TOKEN_DOT,

	TOKEN_BANG, TOKEN_BANG_EQUAL,
	TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
	TOKEN_LESS, TOKEN_LESS_EQUAL,
	TOKEN_GREATER, TOKEN_GREATER_EQUAL,
	TOKEN_AMP, TOKEN_AMP_AMP,
	TOKEN_BAR, TOKEN_BAR_BAR,

	TOKEN_STRING,
	TOKEN_NUMBER,

	TOKEN_IDENTIFIER,
	TOKEN_AS,
	TOKEN_CATCH,
	TOKEN_CLASS,
	TOKEN_CONTINUE,
	TOKEN_ELSE,
	TOKEN_EXPORT,
	TOKEN_FALSE,
	TOKEN_FINALLY,
	TOKEN_FOR,
	TOKEN_FUNCTION,
	TOKEN_IF,
	TOKEN_IMPORT,
	TOKEN_INSTANCEOF,
	TOKEN_NATIVE,
	TOKEN_NULL,
	TOKEN_PRINT,
	TOKEN_RETURN,
	TOKEN_SUPER,
	TOKEN_THIS,
	TOKEN_THROW,
	TOKEN_TRUE,
	TOKEN_TRY,
	TOKEN_VAR,
	TOKEN_WHILE,

	TOKEN_ERROR, TOKEN_EOF,
	TOKEN__COUNT
} FelineTokenType;

typedef struct Token {
	FelineTokenType type;
	const char* start;
	size_t length;
	size_t line;
} Token;

void initLexer(Lexer* lexer, const char* source);
Token lexToken(Lexer* lexer);