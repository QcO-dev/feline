#include <stdio.h>
#include <stdlib.h>
#include "chunk.h"
#include "opcode.h"
#include "vm.h"

static char* readFile(const char* path) {
	FILE* file = fopen(path, "rb");
	
	if (file == NULL) {
		fprintf(stderr, "Could not open file \"%s\"\n", path);
		exit(1);
	}

	fseek(file, 0L, SEEK_END);

	size_t fileSize = ftell(file);
	rewind(file);

	char* charBuffer = (char*)malloc(fileSize + 1);
	if (charBuffer == NULL) {
		fprintf(stderr, "Not enough memory to read \"%s\"\n", path);
		exit(1);
	}
	size_t bytesRead = fread(charBuffer, sizeof(char), fileSize, file);
	if (bytesRead < fileSize) {
		fprintf(stderr, "Could not read file \"%s\"\n", path);
		exit(1);
	}

	charBuffer[bytesRead] = '\0';

	fclose(file);
	return charBuffer;
}

static void runFile(const char* path) {
	char* source = readFile(path);
	VM vm;
	initVM(&vm);
	InterpreterResult result = interpret(&vm, source);
	freeVM(&vm);

	free(source);

	if (result == INTERPRETER_COMPILE_ERROR) exit(2);
	if (result == INTERPRETER_RUNTIME_ERROR) exit(4);
}

int main(int argc, const char** argv) {
	
	if (argc == 1) {
		TODO("Implement REPL when no arguments are passed");
	}
	else if (argc == 2) {
		runFile(argv[1]);
	}
	else {
		fprintf(stderr, "Usage:\nfeline [path]\n");
		exit(1);
	}

	return 0;
}