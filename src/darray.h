#pragma once
#include "common.h"
#include <stdlib.h>

typedef struct VM VM;

#define DECLARE_DYNAMIC_ARRAY(NAME, TYPE) \
typedef struct NAME ## Array { \
	size_t length; \
	size_t capacity; \
	TYPE* items; \
} NAME ## Array; \
\
void init ## NAME ## Array(NAME ## Array* array); \
void write ## NAME ## Array(VM* vm, NAME ## Array* array, TYPE item); \
void free ## NAME ## Array(VM* vm, NAME ## Array* array);

#define DEFINE_DYNAMIC_ARRAY(NAME, TYPE) \
\
void init ## NAME ## Array(NAME ## Array* array) { \
	array->length = 0; \
	array->capacity = 0; \
	array->items = NULL; \
} \
\
void write ## NAME ## Array(VM* vm, NAME ## Array* array, TYPE item) { \
\
	if (array->capacity < array->length + 1) { \
		size_t oldCapacity = array->capacity; \
		array->capacity = GROW_CAPACITY(oldCapacity); \
		array->items = GROW_ARRAY(vm, TYPE, array->items, oldCapacity, array->capacity); \
	} \
\
	array->items[array->length++] = item; \
}\
\
void free ## NAME ## Array(VM* vm, NAME ## Array* array) { \
	FREE_ARRAY(vm, TYPE, array->items, array->capacity); \
	init ## NAME ## Array(array); \
}

DECLARE_DYNAMIC_ARRAY(Byte, uint8_t)