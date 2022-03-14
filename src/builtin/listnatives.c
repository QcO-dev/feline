#include "listnatives.h"
#include "natives.h"
#include <math.h>

/*
  Utility
*/

static size_t findMinrun(size_t n) {
	size_t r = 0;
	while (n >= 32) {
		r |= n & 1;
		n >>= 1;
	}
	return n + r;
}

static double compare(VM* vm, Value a, Value b, Value comparator) {
	push(vm, a);
	push(vm, b);

	Value result = callFromNative(vm, comparator, 2);

	if (vm->hasException) {
		return 0;
	}

	if (!IS_NUMBER(result)) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_VALUE], "Expected comparator to return a number");
		return 0;
	}

	return AS_NUMBER(result);
}

static ObjList* insertionSort(VM* vm, ObjList* list, intmax_t left, intmax_t right, Value comparator) {
	ValueArray arr = list->items;

	for (intmax_t i = left + 1; i <= right; i++) {
		Value element = arr.items[i];
		intmax_t j = i - 1;

		while (j >= left && compare(vm, element, arr.items[j], comparator) < 0) {
			if (vm->hasException) return list;
			arr.items[j + 1] = arr.items[j];
			j--;
		}
		arr.items[j + 1] = element;
	}

	return list;
}

static void mergeSort(VM* vm, ObjList* list, size_t l, size_t m, size_t r, Value comparator) {
	ValueArray array = list->items;
	size_t arrLen1 = m - l + 1;
	size_t arrLen2 = r - m;

	ValueArray left;
	ValueArray right;
	initValueArray(&left);
	initValueArray(&right);

	for (size_t i = 0; i < arrLen1; i++) {
		writeValueArray(vm, &left, array.items[l + 1]);
	}

	for (size_t i = 0; i < arrLen2; i++) {
		writeValueArray(vm, &right, array.items[m + 1 + i]);
	}

	size_t i = 0;
	size_t j = 0;
	size_t k = l;

	while (j < arrLen2 && i < arrLen1) {
		if (compare(vm, left.items[i], right.items[j], comparator) <= 0) {
			array.items[k] = left.items[i];
			i++;
		}
		else {
			array.items[k] = right.items[j];
			j++;
		}
		k++;
	}

	while (j < arrLen1) {
		array.items[k] = array.items[i];
		k++;
		i++;
	}

	while (j < arrLen2) {
		array.items[k] = array.items[j];
		k++;
		j++;
	}
}

/*
  Natives
*/

static Value listAnyNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);

	Value callback = args[0];

	if (!isFunction(callback)) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Expected function as filter");
		return NULL_VAL;
	}

	for (size_t i = 0; i < list->items.length; i++) {
		push(vm, list->items.items[i]);
		push(vm, NUMBER_VAL((double)i));
		push(vm, OBJ_VAL(list));
		Value pass = callFromNative(vm, callback, 3);
		if (vm->hasException) {
			return NULL_VAL;
		}

		if (!isFalsey(vm, pass)) {
			return BOOL_VAL(true);
		}
	}
	return BOOL_VAL(false);
}

static Value listClearNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	AS_LIST(bound)->items.length = 0;
	return NULL_VAL;
}

static Value listConcatNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);

	Value arg = args[0];

	if (!IS_LIST(arg)) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Expected list to concat");
		return NULL_VAL;
	}

	ObjList* listB = AS_LIST(arg);

	ValueArray items;
	initValueArray(&items);

	ObjList* nList = newList(vm, items);

	push(vm, OBJ_VAL(nList));

	for (size_t i = 0; i < list->items.length; i++) {
		writeValueArray(vm, &nList->items, list->items.items[i]);
	}

	for (size_t i = 0; i < listB->items.length; i++) {
		writeValueArray(vm, &nList->items, listB->items.items[i]);
	}

	pop(vm);

	return OBJ_VAL(nList);
}

static Value listEveryNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);

	Value callback = args[0];

	if (!isFunction(callback)) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Expected function as filter");
		return NULL_VAL;
	}

	for (size_t i = 0; i < list->items.length; i++) {
		push(vm, list->items.items[i]);
		push(vm, NUMBER_VAL((double)i));
		push(vm, OBJ_VAL(list));
		Value pass = callFromNative(vm, callback, 3);
		if (vm->hasException) {
			return NULL_VAL;
		}

		if (isFalsey(vm, pass)) {
			return BOOL_VAL(false);
		}
	}
	return BOOL_VAL(true);
}

static Value listExtendNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);

	Value arg = args[0];

	if (!IS_LIST(arg)) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Expected list to extend from");
		return NULL_VAL;
	}

	ObjList* listB = AS_LIST(arg);

	for (size_t i = 0; i < listB->items.length; i++) {
		writeValueArray(vm, &list->items, listB->items.items[i]);
	}

	return NULL_VAL;
}

static Value listFillNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);

	Value filler = args[0];
	for (size_t i = 0; i < list->items.length; i++) {
		list->items.items[i] = filler;
	}
	return bound;
}

static Value listFilterNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);

	Value callback = args[0];

	if (!isFunction(callback)) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Expected function as callback");
		return NULL_VAL;
	}

	ValueArray items;
	initValueArray(&items);

	ObjList* filteredList = newList(vm, items);

	push(vm, OBJ_VAL(filteredList));

	for (size_t i = 0; i < list->items.length; i++) {
		push(vm, list->items.items[i]);
		push(vm, NUMBER_VAL((double)i));
		push(vm, OBJ_VAL(list));
		Value pass = callFromNative(vm, callback, 3);
		if (vm->hasException) {
			return NULL_VAL;
		}
		if(!isFalsey(vm, pass))
			writeValueArray(vm, &filteredList->items, list->items.items[i]);
	}

	pop(vm);

	return OBJ_VAL(filteredList);
}

static Value listForEachNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);

	Value callback = args[0];

	if (!isFunction(callback)) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Expected function as callback");
		return NULL_VAL;
	}

	for (size_t i = 0; i < list->items.length; i++) {
		push(vm, list->items.items[i]);
		push(vm, NUMBER_VAL((double)i));
		push(vm, OBJ_VAL(list));
		callFromNative(vm, callback, 3);

		if (vm->hasException) {
			return NULL_VAL;
		}
	}

	return NULL_VAL;
}

static Value listIndexOfNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);

	for (size_t i = 0; i < list->items.length; i++) {
		if (valuesEqual(vm, args[0], list->items.items[i])) return NUMBER_VAL((double)i);
	}
	return NUMBER_VAL(-1);
}

static Value listLastIndexOfNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);

	for (size_t i = list->items.length; i > 0; i--) {
		if (valuesEqual(vm, args[0], list->items.items[i - 1])) return NUMBER_VAL((double)i - 1);
	}
	return NUMBER_VAL(-1);
}

static Value listLengthNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	return NUMBER_VAL((double)AS_LIST(bound)->items.length);
}

static Value listMapNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);

	Value callback = args[0];

	if (!isFunction(callback)) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Expected function as callback");
		return NULL_VAL;
	}

	ValueArray items;
	initValueArray(&items);

	ObjList* mappedList = newList(vm, items);

	push(vm, OBJ_VAL(mappedList));

	for (size_t i = 0; i < list->items.length; i++) {
		push(vm, list->items.items[i]);
		push(vm, NUMBER_VAL((double)i));
		push(vm, OBJ_VAL(list));
		Value mapped = callFromNative(vm, callback, 3);
		if (vm->hasException) {
			return NULL_VAL;
		}
		writeValueArray(vm, &mappedList->items, mapped);
	}

	pop(vm);

	return OBJ_VAL(mappedList);
}

static Value listOfLengthNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);

	if (!IS_NUMBER(args[0])) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Expected number as first argument in ofLength.");
		return NULL_VAL;
	}

	if (floor(AS_NUMBER(args[0])) != AS_NUMBER(args[0])) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_VALUE], "Expected integer as first argument in ofLength.");
		return NULL_VAL;
	}

	intmax_t size = (intmax_t)AS_NUMBER(args[0]);

	if (size < 0) {
		size = list->items.length + size;
		size = max(0, size);
	}

	ValueArray items;
	initValueArray(&items);

	ObjList* ofLength = newList(vm, items);

	push(vm, OBJ_VAL(ofLength));

	for (intmax_t i = 0; i < size; i++) {
		if (i < (intmax_t)list->items.length) {
			writeValueArray(vm, &ofLength->items, list->items.items[i]);
		}
		else {
			writeValueArray(vm, &ofLength->items, NULL_VAL);
		}
	}

	pop(vm);

	return OBJ_VAL(ofLength);
}

static Value listPopNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);
	return list->items.items[--list->items.length];
}

static Value listPushNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	writeValueArray(vm, &AS_LIST(bound)->items, args[0]);
	return args[0];
}

static Value listReduceNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);

	Value callback = args[0];

	if (!isFunction(callback)) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Expected function as callback");
		return NULL_VAL;
	}

	if (list->items.length == 0) return NULL_VAL;
	if (list->items.length == 1) return list->items.items[0];

	Value previousValue = list->items.items[0];
	
	for (size_t i = 1; i < list->items.length; i++) {
		push(vm, previousValue);
		push(vm, list->items.items[i]);
		push(vm, NUMBER_VAL((double)i));
		push(vm, OBJ_VAL(list));

		previousValue = callFromNative(vm, callback, 4);

		if (vm->hasException) {
			return NULL_VAL;
		}
	}

	return previousValue;
}

static Value listReverseNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);

	ValueArray items;
	initValueArray(&items);

	ObjList* reversedList = newList(vm, items);

	push(vm, OBJ_VAL(reversedList));

	for (size_t i = list->items.length; i > 0; i--) {
		writeValueArray(vm, &reversedList->items, list->items.items[i - 1]);
	}

	pop(vm);

	return OBJ_VAL(reversedList);
}

static Value listSortNative(VM* vm, Value bound, uint8_t argCount, Value* args) {
	ObjList* list = AS_LIST(bound);

	ValueArray items;
	initValueArray(&items);

	ObjList* sortedList = newList(vm, items);

	push(vm, OBJ_VAL(sortedList));

	for (size_t i = 0; i < list->items.length; i++) {
		writeValueArray(vm, &sortedList->items, list->items.items[i]);
	}

	Value comparator = args[0];

	if (!isFunction(comparator)) {
		throwException(vm, vm->internalExceptions[INTERNAL_EXCEPTION_TYPE], "Expected function as comparator");
		return NULL_VAL;
	}

	size_t n = sortedList->items.length;

	size_t minrun = findMinrun(n);

	for (size_t start = 0; start < n; start += minrun) {
		size_t end = min(start + minrun - 1, n - 1);
		insertionSort(vm, sortedList, start, end, comparator);
	}

	size_t size = minrun;

	while (size < n) {
		for (size_t left = 0; left < n; left += 2 * size) {
			size_t mid = min(n - 1, left + size - 1);
			size_t right = min(left + 2 * size - 1, n - 1);
			mergeSort(vm, sortedList, left, mid, right, comparator);
		}
		size *= 2;
	}

	pop(vm);

	return OBJ_VAL(sortedList);
}

void defineListNativeMethods(VM* vm) {
	defineNative(vm, &vm->listMethods, "any", listAnyNative, 1);
	defineNative(vm, &vm->listMethods, "clear", listClearNative, 0);
	defineNative(vm, &vm->listMethods, "concat", listConcatNative, 1);
	defineNative(vm, &vm->listMethods, "every", listEveryNative, 1);
	defineNative(vm, &vm->listMethods, "extend", listExtendNative, 1);
	defineNative(vm, &vm->listMethods, "fill", listFillNative, 1);
	defineNative(vm, &vm->listMethods, "filter", listFilterNative, 1);
	defineNative(vm, &vm->listMethods, "forEach", listForEachNative, 1);
	defineNative(vm, &vm->listMethods, "indexOf", listIndexOfNative, 1);
	defineNative(vm, &vm->listMethods, "lastIndexOf", listLastIndexOfNative, 1);
	defineNative(vm, &vm->listMethods, "length", listLengthNative, 0);
	defineNative(vm, &vm->listMethods, "map", listMapNative, 1);
	defineNative(vm, &vm->listMethods, "ofLength", listOfLengthNative, 1);
	defineNative(vm, &vm->listMethods, "pop", listPopNative, 0);
	defineNative(vm, &vm->listMethods, "push", listPushNative, 1);
	defineNative(vm, &vm->listMethods, "reduce", listReduceNative, 1);
	defineNative(vm, &vm->listMethods, "reverse", listReverseNative, 0);
	defineNative(vm, &vm->listMethods, "sort", listSortNative, 1);
}
