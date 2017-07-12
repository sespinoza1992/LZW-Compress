#ifndef LZWCMP_H
#define LZWCMP_H

#include "MyLib.h"

#define NUM_SYMS 256

// Debug flags
#define VERBOSE_MODE 0x1

/* Function pointer to method to call when a code is completed and ready for
 * transmission or whatever.  The void * parameter can point to anything,
 * and gives hidden information to the function so that it can know what
 * file, socket, etc. the code is going to.  The UInt is the next 32 bits
 * worth of compressed output. */
typedef void (*CodeSink)(void *, UInt code);

/* One node in a trie representing the current dictionary.  Use symbols
 * to traverse the trie until reaching a point where the link for a
 * symbol is null.  Use the code for the prior link, and add a new code in
 * this case.  Each node has as many links and codes as there are symbols */
typedef struct TrieNode {
   short codes[NUM_SYMS];
   struct TrieNode *links[NUM_SYMS];
} TrieNode;

/* Current state of the LZW compressor. */
typedef struct LZWCmp {
   TrieNode *head;   /* Head pointer to first TrieNode */
   CodeSink sink;    /* Code sink to send bits to */
   void *sinkState;  /* Unknown object to send to sink for state */
   int nextCode;     /* Next code to be assigned */
   int numBits;      /* Number of bits per code currently */
   int maxCode;      /* Max code that fits in numBits bits */
   UInt nextInt;     /* Partially-assembled next int of output */
   int bitsUsed;     /* Number of valid bits in top portion of nextInt */
   TrieNode *curLoc; /* Current position in trie */
   short lastSym;    /* Most recent symbol encoded */
   int debugFlags;   /* Bit flags indicated debug actions to take */
} LZWCmp;

/* Initialize a LZWCmp given the CodeSink to which to send completed codes. */
void LZWCmpInit(LZWCmp *cmp, CodeSink sink, void *sinkState, int debugFlags);

/* Encode "sym" using LZWCmp. Zero or more calls of the code sink
 * may result */
void LZWCmpEncode(LZWCmp *cmp, UChar sym);

/* Mark end of encoding (send next code value to code sink) */
void LZWCmpStop(LZWCmp *cmp);

/* Free all storage associated with LZWCmp (not the sinkState, though,
 * which is "owned" by the caller */
void LZWCmpDestruct(LZWCmp *cmp);

void LZWCmpClearFreelist();

#endif