#include "ffi.h"
#include "../vm.h"

#ifdef _WIN32

NativeLibrary loadNativeLibrary(VM* vm, ObjString* path) {
	HINSTANCE dll;
	dll = LoadLibraryA(path->str);

	if (dll == NULL) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_LINK_FAILURE], "Could not load DLL file '%s'", path->str);
		return NULL;
	}
	return dll;
}

void freeNativeLibrary(NativeLibrary library) {
	FreeLibrary(library);
}

#endif