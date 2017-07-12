#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SmartAlloc.h"
#include "MyLib.h"
#include "LZWCmp.h"


#define PRINTNUMS 8

void PrintInfo(void *nums, UInt code) {
	(*(int *) nums)--;
	if (*(int *) nums > 0)
		printf("%08X ", code);

	else {
		printf("%08X\n", code);
		*(int *)nums = PRINTNUMS;
	}
}

int main(int argc, char *argv[]) {
	LZWCmp *test = malloc(sizeof(LZWCmp));
	char input;
	CodeSink print = PrintInfo;
	int nums = PRINTNUMS, flag = 0;


	// Check for -v
	if (argc > 1) {
		if (strcmp(argv[1], "-v") == 0)
			flag = 1;
	}

	// Initialize LZWCmp
	LZWCmpInit(test, print, &nums, flag);


	// Read in characters
	while (EOF != scanf("%c", &input)) {
		LZWCmpEncode(test, input);
	}

	LZWCmpStop(test);
	printf("\n");
	LZWCmpDestruct(test);
	LZWCmpClearFreelist();
	free(test);

	return 0;
}