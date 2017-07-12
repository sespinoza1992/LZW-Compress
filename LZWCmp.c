#include <stdio.h>
#include "SmartAlloc.h"
#include "LZWCmp.h"
#include "MyLib.h"

#define I_BITS 32

static TrieNode *FreeList = NULL;

void Verbose(int code, int bits) {
	printf("Sending code %d in %d bits\n", code, bits);
}

void Dump(LZWCmp *cmp) {	
	TrieNode *temp;
	int i;

	temp = cmp->curLoc;
	for (i = 0; i < NUM_SYMS; i++) {
		if (*(temp->links + i)) {
			cmp->curLoc = *(temp->links + i);
			Dump(cmp);
		}
	}
	// If at head, clear out links pointers
	// and reset values
	if (temp == cmp->head) {
		while (i-- > 0) {
			*(cmp->head->links + i) = NULL;
			//printf("*(cmp->head->codes + i) is: %d\n", *(cmp->head->codes + i));
		}
	}
	// Otherwise, add Trie to FreeList
	else {
		*(temp->links) = FreeList;
		FreeList = temp;
	}
}

// Get a TrieNode with no codes or pointers
void GetTrieNode(TrieNode **link) {
	int i;

	// If there is a FreeList available, 
	// re-use a node
	if (FreeList) {
		*link = FreeList;
		FreeList = *(FreeList->links);
	}

	// Otherwise, allocate new trienode
	else {
		*link = malloc(sizeof(TrieNode));
	}

	// Set codes to -1 (unused), and links to null
	for (i = 0; i < NUM_SYMS; i++) {
		*((*link)->codes + i) = -1;
		*((*link)->links + i) = NULL;
	}
}

/* Initialize a LZWCmp given the CodeSink to which to send completed codes. */
void LZWCmpInit(LZWCmp *cmp, CodeSink sink, void *sinkState, int debugFlags) {
	// Allocate head of trienode
	cmp->head = calloc(sizeof(TrieNode), 1);
	cmp->curLoc = cmp->head;

	// Set initial values
	cmp->sink = sink;
	cmp->sinkState = sinkState;
	cmp->numBits = BITS_PER_BYTE + 1;
	cmp->maxCode = ~(-1 << cmp->numBits);
	cmp->nextInt = 0;
	cmp->bitsUsed = 0;
	cmp->lastSym = -1;
	cmp->nextCode = 0;
	cmp->debugFlags = debugFlags;

	// Initialize dictionary
	while (cmp->nextCode < NUM_SYMS) {
		*(cmp->curLoc->codes + cmp->nextCode) = cmp->nextCode;
		cmp->nextCode++;
	}

	// Skip over EOD code
	cmp->nextCode++;

}


/* Encode "sym" using LZWCmp. Zero or more calls of the code sink
 * may result */
void LZWCmpEncode(LZWCmp *cmp, UChar sym) {
	int lShift, rShift = 0;

	// If in verbose mode, call verbose
	if (cmp->debugFlags) {
		if (*(cmp->curLoc->codes + sym) == -1)
			Verbose(cmp->lastSym, cmp->numBits);
	}

	// If code exists, go to next trie and 
	// store last sym. If trie doesn't exist,
	// allocate one
	if (*(cmp->curLoc->codes + sym) != -1) {
		cmp->lastSym = *(cmp->curLoc->codes + sym);


		if (*(cmp->curLoc->links + sym))
			cmp->curLoc = *(cmp->curLoc->links + sym);

		else {
			GetTrieNode(cmp->curLoc->links + sym);
			cmp->curLoc = *(cmp->curLoc->links + sym);
		}
	}

	// Otherwise, we have a new code to add to the list
	else {
		cmp->bitsUsed += cmp->numBits;

		if (cmp->bitsUsed > I_BITS)
			rShift = cmp->bitsUsed - I_BITS;

		// Get amount nextInt will need to be shifted
		// for incoming bits
		lShift = cmp->numBits - rShift;

		// Pass in bits for nextint
		cmp->nextInt = cmp->nextInt << lShift | cmp->lastSym >> rShift;

		// Check to see if any limits were exceeded

		// If nextCode is greater than maxcode,
		// increase numbits and reset maxCode so
		// nextCode will fit
		if (cmp->nextCode > cmp->maxCode) {
			//printf("maxCode exceeded limit\n");
			cmp->numBits++;
			cmp->maxCode = ~(-1 << cmp->numBits);	
			//printf("maxcode is %d\n", cmp->maxCode);
		}

		// Set next code and increment
		*(cmp->curLoc->codes + sym) = cmp->nextCode;
		cmp->nextCode++;

		// If limit is exceeded (4096),
		// add current space to FreeList
		// and reset maxCode and numBits, and point
		// curLoc back to head
		if (cmp->nextCode > LIMIT) {
			//printf("nextCode exceeded limit\n");
			cmp->curLoc = cmp->head;
			Dump(cmp);
			cmp->numBits = BITS_PER_BYTE + 1;
			cmp->nextCode = EOD + 1;
			cmp->maxCode = ~(-1 << cmp->numBits);
			cmp->curLoc = cmp->head;
		}

		// If bitsUsed is over the limit, call
		// codesink and reset bitsUsed/nextint
		if (cmp->bitsUsed >= I_BITS) {
			//printf("Callling codesink\n");
			(cmp->sink)(cmp->sinkState, cmp->nextInt);
			cmp->bitsUsed = cmp->bitsUsed - I_BITS;
			cmp->nextInt = cmp->lastSym;	
		}

		// Move curLoc back to head
		cmp->curLoc = cmp->head;

		// Set lastSym to the set code
		cmp->lastSym = *(cmp->curLoc->codes + sym);
		
		// Go to next code in tree
		if (*(cmp->curLoc->links + sym))
			cmp->curLoc = *(cmp->curLoc->links + sym);

		else {
			GetTrieNode(cmp->curLoc->links + sym);
			cmp->curLoc = *(cmp->curLoc->links + sym);
		}
		
	}
}


/* Mark end of encoding (send next code value to code sink) */
void LZWCmpStop(LZWCmp *cmp) {
	int lShift, rShift = 0;

	// Check if last code was skipped over.
	// If so, write it to next int
	if (cmp->lastSym != -1) {
		// If in verbose mode, call verbose
		if (cmp->debugFlags)
			Verbose(cmp->lastSym, cmp->numBits);
		cmp->bitsUsed += cmp->numBits;

		// Set right shift
		if (cmp->bitsUsed > I_BITS)
			rShift = cmp->bitsUsed - I_BITS;

		// Set left shift
		lShift = cmp->numBits - rShift;

		// Insert bits from last code into nextint
		cmp->nextInt = cmp->nextInt << lShift | cmp->lastSym >> rShift;
		
		// If bitsUsed is over the limit, reset
		// its value and call codesink
		if (cmp->bitsUsed >= I_BITS) {
			(cmp->sink)(cmp->sinkState, cmp->nextInt);
			cmp->bitsUsed = cmp->bitsUsed - I_BITS;
			cmp->nextInt = cmp->lastSym;
		}
	}
	// If in verbose mode, call verbose
	if (cmp->debugFlags)
		Verbose(EOD, cmp->numBits);

	// Insert EOD code
	// Incremnt bits used
	cmp->bitsUsed += cmp->numBits;

	rShift = 0;
	if (cmp->bitsUsed > I_BITS)
		rShift = cmp->bitsUsed - I_BITS;

	lShift = cmp->numBits - rShift;

	// Insert bits from EOD into nextint
	cmp->nextInt = cmp->nextInt << lShift | EOD >> rShift;

	// If bitsUsed is less than 32
	// shift out extra bits from lastsym
	if (cmp->bitsUsed < I_BITS)
		cmp->nextInt = cmp->nextInt << I_BITS - cmp->bitsUsed;
		
	// Call codesink
	(cmp->sink)(cmp->sinkState, cmp->nextInt);

	// If there are extra bits,
	// call codesink on 0
	if (cmp->bitsUsed > I_BITS)
		(cmp->sink)(cmp->sinkState, 0);
}
/* Free all storage associated with LZWCmp (not the sinkState, though,
 * which is "owned" by the caller */
void LZWCmpDestruct(LZWCmp *cmp) {
	TrieNode *temp = cmp->head;
	int i;
	
	for (i = 0; i < NUM_SYMS; i++) {
		if (*(temp->links + i)) {
			cmp->head = *(temp->links + i);
			LZWCmpDestruct(cmp);
		}
	}
	free(temp);
}
	
void LZWCmpClearFreelist() {
	TrieNode *temp;

	while (FreeList) {
		temp = *(FreeList->links);
		free(FreeList);
		FreeList = temp;
	}
}	



