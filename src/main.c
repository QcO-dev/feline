#include <stdio.h>
#include <stdlib.h>
#include "memory.h"
#include "opcode.h"
#include "vm.h"

#ifndef _WIN32
#include <string.h>
#include <libgen.h>

void strip_ext(char* fname) {
	char* end = fname + strlen(fname);

	while (end > fname && *end != '.') {
		--end;
	}

	if (end > fname) {
		*end = '\0';
	}
}
#endif


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

static void splitPathToNameAndDirectory(VM* vm, const char* path) {
#ifdef _WIN32
	size_t pathLength = strlen(path);
	char* dir = reallocate(vm, NULL, 0, _MAX_DIR);
	char* fname = reallocate(vm, NULL, 0, _MAX_FNAME);
	char* drive = reallocate(vm, NULL, 0, _MAX_DRIVE);

	_splitpath(path, drive, dir, fname, NULL);

	vm->directory = makeStringf(vm, "%s%s", drive, dir);
	vm->name = takeString(vm, fname, strlen(fname));

	FREE_ARRAY(vm, char, dir, strlen(dir));
	FREE_ARRAY(vm, char, drive, strlen(drive));
#else
	char* dirPathCopy = strdup(path);

	char* directory = dirname(dirPathCopy);
	vm->directory = copyString(vm, directory, strlen(directory));
	
	free(dirPathCopy);

	char* fnamePathCopy = strdup(path);
	char* fname = basename(fnamePathCopy);
	strip_ext(fname);
	vm->name = copyString(vm, fname, strlen(fname));

	free(fnamePathCopy);
#endif
}

static void runFile(const char* path) {
	char* source = readFile(path);
	VM vm;
	initVM(&vm);
	splitPathToNameAndDirectory(&vm, path);
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