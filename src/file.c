#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include "memory.h"

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

char* readFile(const char* path) {
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

void splitPathToNameAndDirectory(VM* vm, Module* mod, const char* path) {
#ifdef _WIN32
	size_t pathLength = strlen(path);
	char* dir   = reallocate(vm, NULL, 0, _MAX_DIR);
	char* fname = reallocate(vm, NULL, 0, _MAX_FNAME);
	char* drive = reallocate(vm, NULL, 0, _MAX_DRIVE);

	_splitpath(path, drive, dir, fname, NULL);

	mod->directory = makeStringf(vm, "%s%s", drive, dir);
	mod->name = takeString(vm, fname, strlen(fname));

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