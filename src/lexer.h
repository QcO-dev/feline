#pragma once
#include "common.h"

typedef struct Lexer {
	const char* start;
	const char* current;
	size_t line;
} Lexer;

typedef enum TokenType {
	TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
	TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
	TOKEN_PLUS, TOKEN_MINUS,
	TOKEN_STAR, TOKEN_SLASH,
	TOKEN_SEMICOLON,
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
	TOKEN_CLASS,
	TOKEN_ELSE,
	TOKEN_FALSE,
	TOKEN_FOR,
	TOKEN_FUNCTION,
	TOKEN_IF,
	TOKEN_NULL,
	TOKEN_PRINT,
	TOKEN_RETURN,
	TOKEN_SUPER,
	TOKEN_THIS,
	TOKEN_TRUE,
	TOKEN_VAR,
	TOKEN_WHILE,

	TOKEN_ERROR, TOKEN_EOF,
	TOKEN__COUNT
} TokenType;

typedef struct Token {
	TokenType type;
	const char* start;
	size_t length;
	size_t line;
} Token;

void initLexer(Lexer* lexer, const char* source);
Token lexToken(Lexer* lexer);