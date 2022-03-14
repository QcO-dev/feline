#include "lexer.h"
#include <string.h>

void initLexer(Lexer* lexer, const char* source) {
	lexer->start = source;
	lexer->current = source;
	lexer->line = 1;
}

// ========= Helper Functions =========

static bool isAtEnd(Lexer* lexer) {
	return *lexer->current == '\0';
}

static char advance(Lexer* lexer) {
	lexer->current++;
	return lexer->current[-1];
}

static char peek(Lexer* lexer) {
	return *lexer->current;
}

// Will return the character *after* the current one without consuming anything.
// E.g., given the string "abc", with the start pointer at the start of the string,
// peek() will return the "a" and peekNext() will return the "b"
static char peekNext(Lexer* lexer) {
	if (isAtEnd(lexer)) return '\0';
	return lexer->current[1];
}

static bool match(Lexer* lexer, char expected) {
	if (isAtEnd(lexer)) return false;
	if (*lexer->current != expected) return false;
	lexer->current++;
	return true;
}

static Token makeToken(Lexer* lexer, FelineTokenType type) {
	Token token;
	token.type = type;
	token.start = lexer->start;
	token.length = lexer->current - lexer->start;
	token.line = lexer->line;
	return token;
}

static Token errorToken(Lexer* lexer, const char* message) {
	Token token;
	token.type = TOKEN_ERROR;
	token.start = message;
	token.length = strlen(message);
	token.line = lexer->line;
	return token;
}

static bool isDigit(char c) {
	return '0' <= c && c <= '9';
}

static bool isAlpha(char c) {
	return ('a' <= c && c <= 'z') ||
		('A' <= c && c <= 'Z') ||
		c == '_';
}

static FelineTokenType checkKeyword(Lexer* lexer, size_t start, size_t length, const char* rest, FelineTokenType type) {
	if (lexer->current - lexer->start == start + length && memcmp(lexer->start + start, rest, length) == 0) {
		return type;
	}
	return TOKEN_IDENTIFIER;
}

// ========= Tokenisation =========

static void skipWhitespaceBefore(Lexer* lexer) {
	for (;;) {
		char c = peek(lexer);
		switch (c) {
			case ' ':
			case '\r':
			case '\t': {
				advance(lexer);
				break;
			}
			case '\n': {
				lexer->line++;
				advance(lexer);
				break;
			}
			case '/': {
				if (peekNext(lexer) == '/') {
					while (peek(lexer) != '\n' && !isAtEnd(lexer)) advance(lexer);
				}
				else {
					return;
				}
				break;
			}
			default: {
				return;
			}
		}
	}
}

static Token string(Lexer* lexer) {
	while (!(peek(lexer) == '"' && lexer->current[-1] != '\\') && !isAtEnd(lexer)) {
		if (peek(lexer) == '\n') lexer->line++;
		advance(lexer);
	}
	if (isAtEnd(lexer)) return errorToken(lexer, "Unterminated string (expected '\"')");

	// Terminating double quote mark
	advance(lexer);
	return makeToken(lexer, TOKEN_STRING);
}

static Token number(Lexer* lexer) {
	while (isDigit(peek(lexer))) advance(lexer);

	// Fractional part
	if (peek(lexer) == '.' && isDigit(peekNext(lexer))) {
		advance(lexer);

		while (isDigit(peek(lexer))) advance(lexer);
	}

	return makeToken(lexer, TOKEN_NUMBER);
}

static FelineTokenType identifierType(Lexer* lexer) {
	switch (lexer->start[0]) {
		case 'a': return checkKeyword(lexer, 1, 1, "s", TOKEN_AS);
		case 'c': {
			if (lexer->current - lexer->start > 1) {
				switch (lexer->start[1]) {
					case 'a': return checkKeyword(lexer, 2, 3, "tch", TOKEN_CATCH);
					case 'l': return checkKeyword(lexer, 2, 3, "ass", TOKEN_CLASS);
					case 'o': return checkKeyword(lexer, 2, 6, "ntinue", TOKEN_CONTINUE);
				}
			}
			break;
		}
		case 'e': {
			if (lexer->current - lexer->start > 1) {
				switch (lexer->start[1]) {
					case 'l': return checkKeyword(lexer, 2, 2, "se", TOKEN_ELSE);
					case 'x': return checkKeyword(lexer, 2, 4, "port", TOKEN_EXPORT);
				}
			}
			break;
		}
		case 'f': {
			if (lexer->current - lexer->start > 1) {
				switch (lexer->start[1]) {
					case 'a': return checkKeyword(lexer, 2, 3, "lse", TOKEN_FALSE);
					case 'i': return checkKeyword(lexer, 2, 5, "nally", TOKEN_FINALLY);
					case 'o': return checkKeyword(lexer, 2, 1, "r", TOKEN_FOR);
					case 'u': return checkKeyword(lexer, 2, 6, "nction", TOKEN_FUNCTION);
				}
			}
			break;
		}
		case 'i': {
			if (lexer->current - lexer->start > 1) {
				switch (lexer->start[1]) {
					case 'f': return checkKeyword(lexer, 2, 0, "", TOKEN_IF);
					case 'm': return checkKeyword(lexer, 2, 4, "port", TOKEN_IMPORT);
					case 'n': return checkKeyword(lexer, 2, 8, "stanceof", TOKEN_INSTANCEOF);
				}
			}
			break;
		}
		case 'n': {
			if (lexer->current - lexer->start > 1) {
				switch (lexer->start[1]) {
					case 'a': return checkKeyword(lexer, 2, 4, "tive", TOKEN_NATIVE);
					case 'u': return checkKeyword(lexer, 2, 2, "ll", TOKEN_NULL);
				}
			}
			break;
		}
		case 'p': return checkKeyword(lexer, 1, 4, "rint", TOKEN_PRINT);
		case 'r': return checkKeyword(lexer, 1, 5, "eturn", TOKEN_RETURN);
		case 's': return checkKeyword(lexer, 1, 4, "uper", TOKEN_SUPER);
		case 't': {
			if (lexer->current - lexer->start > 1) {
				switch (lexer->start[1]) {
					case 'h': {
						if (lexer->current - lexer->start > 2) {
							switch (lexer->start[2]) {
								case 'i': return checkKeyword(lexer, 3, 1, "s", TOKEN_THIS);
								case 'r': return checkKeyword(lexer, 3, 2, "ow", TOKEN_THROW);
							}
						}
						break;
					}
					case 'r': {
						if (lexer->current - lexer->start > 2) {
							switch (lexer->start[2]) {
								case 'u': return checkKeyword(lexer, 3, 1, "e", TOKEN_TRUE);
								case 'y': return checkKeyword(lexer, 3, 0, "", TOKEN_TRY);
							}
						}
						break;
					}
				}
			}
			break;
		}
		case 'v': return checkKeyword(lexer, 1, 2, "ar", TOKEN_VAR);
		case 'w': return checkKeyword(lexer, 1, 4, "hile", TOKEN_WHILE);
	}
	return TOKEN_IDENTIFIER;
}

static Token identifier(Lexer* lexer) {
	while (isAlpha(peek(lexer)) || isDigit(peek(lexer))) advance(lexer);
	return makeToken(lexer, identifierType(lexer));
}

Token lexToken(Lexer* lexer) {
	skipWhitespaceBefore(lexer);
	lexer->start = lexer->current;

	if (isAtEnd(lexer)) return makeToken(lexer, TOKEN_EOF);

	char c = advance(lexer);

	if (isAlpha(c)) return identifier(lexer);
	if (isDigit(c)) return number(lexer);

	switch (c) {
		case '(': return makeToken(lexer, TOKEN_LEFT_PAREN);
		case ')': return makeToken(lexer, TOKEN_RIGHT_PAREN);
		case '{': return makeToken(lexer, TOKEN_LEFT_BRACE);
		case '}': return makeToken(lexer, TOKEN_RIGHT_BRACE);
		case '[': return makeToken(lexer, TOKEN_LEFT_SQUARE);
		case ']': return makeToken(lexer, TOKEN_RIGHT_SQUARE);
		case '+': return makeToken(lexer, TOKEN_PLUS);
		case '-': return makeToken(lexer, TOKEN_MINUS);
		case '*': return makeToken(lexer, TOKEN_STAR);
		case '/': return makeToken(lexer, TOKEN_SLASH);
		case ';': return makeToken(lexer, TOKEN_SEMICOLON);
		case ':': return makeToken(lexer, TOKEN_COLON);
		case ',': return makeToken(lexer, TOKEN_COMMA);
		case '.': return makeToken(lexer, TOKEN_DOT);
		case '!': return match(lexer, '=') ? makeToken(lexer, TOKEN_BANG_EQUAL) : makeToken(lexer, TOKEN_BANG);
		case '=': return match(lexer, '=') ? makeToken(lexer, TOKEN_EQUAL_EQUAL) : makeToken(lexer, TOKEN_EQUAL);
		case '<': return match(lexer, '=') ? makeToken(lexer, TOKEN_LESS_EQUAL) : makeToken(lexer, TOKEN_LESS);
		case '>': return match(lexer, '=') ? makeToken(lexer, TOKEN_GREATER_EQUAL) : makeToken(lexer, TOKEN_GREATER);
		case '&': return match(lexer, '&') ? makeToken(lexer, TOKEN_AMP_AMP) : makeToken(lexer, TOKEN_AMP);
		case '|': return match(lexer, '|') ? makeToken(lexer, TOKEN_BAR_BAR) : makeToken(lexer, TOKEN_BAR);
		case '"': return string(lexer);
	}

	return errorToken(lexer, "Unexpected character");
}