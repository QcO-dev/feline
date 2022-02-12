#pragma once
#include "../common.h"
#include "../value.h"

//TODO: Currently the FFI only supports interfacing with Windows DLLs.
//      It cannot like with .so files on Linux or MacOS.

#ifdef _WIN32

#include <Windows.h>

typedef HINSTANCE NativeLibrary;
#endif

NativeLibrary loadNativeLibrary(VM* vm, ObjString* path);

void freeNativeLibrary(NativeLibrary library);