#include <stdio.h>
#include <stdlib.h>
#include "file.h"
#include "memory.h"
#include "opcode.h"
#include "vm.h"
#include "module.h"

static void runFile(const char* path) {
	char* source = readFile(path);
	VM vm;
	initVM(&vm);

	Module* mainModule = ALLOCATE(&vm, Module, 1);
	initModule(&vm, mainModule);

	splitPathToNameAndDirectory(&vm, mainModule, path);
	vm.baseDirectory = mainModule->directory;
	ObjString* mainName = copyString(&vm, "$main", 5);
	push(&vm, OBJ_VAL(mainName));
	tableSet(&vm, &mainModule->globals, vm.internalStrings[INTERNAL_STR_THIS_MODULE], OBJ_VAL(mainName));
	pop(&vm);
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