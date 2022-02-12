#pragma once
#include "../common.h"
#include "../vm.h"
#include "../builtin/exception.h"
#include "felineffi.h"

FELINE_EXPORT ObjClass* export_getInternalException(VM* vm, InternalExceptionType type);