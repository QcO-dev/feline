#include "ffi.h"
#include "../vm.h"
#include "../value.h"

#ifdef _WIN32

NativeLibrary loadNativeLibrary(VM* vm, ObjString* path) {
	Value v;
	if (tableGet(&vm->nativeLibraries, path, &v)) {
		return AS_NATIVE_LIBRARY(v);
	}

	HINSTANCE dll;
	dll = LoadLibraryA(path->str);

	if (dll == NULL) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_LINK_FAILURE], "Could not load DLL file '%s'", path->str);
		return NULL;
	}

	push(vm, OBJ_VAL(path));
	ObjNativeLibrary* lib = newNativeLibrary(vm, dll);
	push(vm, OBJ_VAL(lib));
	tableSet(vm, &vm->nativeLibraries, path, OBJ_VAL(lib));
	pop(vm);
	pop(vm);
	return dll;
}

void freeNativeLibrary(NativeLibrary library) {
	FreeLibrary(library);
}

NativeFunction loadNativeFunction(VM* vm, NativeLibrary library, ObjString* name) {
	NativeFunction function;

	function = (NativeFunction)GetProcAddress(library, name->str);

	if (function == NULL) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_LINK_FAILURE], "Could not load native function '%s'", name->str);
		return NULL;
	}

	return function;
}

#endif