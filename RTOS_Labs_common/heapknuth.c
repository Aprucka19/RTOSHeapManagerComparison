// heapknuth.c
// Alex Prucka
// RTOS Grad Project
// Implementation of Knuths Buddy Algorithm
// for heap management



#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../RTOS_Labs_common/heapknuth.h"
#include "../RTOS_Labs_common/OS.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define HEAPSIZE 1024
//#define HEAPSIZE 256

//Heap Stats object
struct heap_stats knuth_hStats;
long knuth_heap[HEAPSIZE];
int knuth_init = 0;
Sema4Type knuth_HeapMutex;

//******** Heap_Init *************** 
// Initialize the Heap
// input: none
// output: always 0
// notes: Initializes/resets the heap to a clean state where no memory
//  is allocated.
int32_t Knuth_Heap_Init(){
    knuth_hStats.size = HEAPSIZE * 4;
    knuth_hStats.used = 0;
    knuth_hStats.free = (HEAPSIZE - 2) * 4;
    memset(knuth_heap, 0, sizeof(knuth_heap));
    knuth_heap[0] = -(HEAPSIZE - 2);
    knuth_heap[HEAPSIZE-1] = -(HEAPSIZE-2);
    knuth_init = 1;
    knuth_HeapMutex.Value = 1;
	
    return 0;   // replace
}


//Cuts the smallest free block in half
// Returns 1 if no free block exists.
// Else returns 0 on success 
int cutBlock(int desired) {
    int minBlockIndex = -1;
    int minBlockSize = HEAPSIZE; // Larger than any block could be
		OS_bWait(&knuth_HeapMutex);
    // Find the smallest block that's free
    for (int i = 0; i < HEAPSIZE - 1;) {
        //block is free
        if(knuth_heap[i] < 0){
            int blockSize = -knuth_heap[i]; 
            //Is the block free?

            if (blockSize >= desired && blockSize < minBlockSize) { // Ensure block can be split (minimum size 4)
                minBlockSize = blockSize;
                minBlockIndex = i;
            }
        }


        i += abs(knuth_heap[i]) + 2; // Move to the next block, considering the size stored at both ends
    }

    if (minBlockIndex == -1) {
				OS_bSignal(&knuth_HeapMutex);
        return 1; // No block found to split
    }

    // Ensure the block can actually be split
    if (minBlockSize < 4) {
				OS_bSignal(&knuth_HeapMutex);
        return 1; // Block is too small to split, must be at least 4
    }

    // Split the block into two smaller blocks
    int halfSize = (minBlockSize / 2) - 1;
    knuth_heap[minBlockIndex] = -halfSize;
    knuth_heap[minBlockIndex + halfSize + 1] = -halfSize;  // Set the second half as free
    knuth_heap[minBlockIndex + halfSize + 2] = -halfSize;  // New end marker for the first half
    knuth_heap[minBlockIndex + 3 + 2*halfSize] = -halfSize;  // End marker for the second half
		
    //8 bytes lost to overhead
    knuth_hStats.free -= 2 * 4;
    
    OS_bSignal(&knuth_HeapMutex);
    return 0;
}


//******** Heap_Malloc *************** 
// Allocate memory, data not initialized
// input: 
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory or will return NULL
//   if there isn't sufficient space to satisfy allocation request
int knuth_sectorsNeeded;
int knuth_spaceLeft;

void* Knuth_Heap_Malloc(int32_t desiredBytes){
    OS_bWait(&knuth_HeapMutex);

    // Calculate the minimum number of sectors needed
    knuth_sectorsNeeded = (desiredBytes + sizeof(int) - 1) / sizeof(int);  // Adding size for metadata

    // Find the smallest power of two that meets or exceeds the size needed
    int blockSize = 4;  // Start with the smallest block size (as per the smallest n is 2^2)
    while (blockSize < knuth_sectorsNeeded + 2) { // +2 for block metadata
        blockSize <<= 1;
    }
		blockSize -= 2;

    // Try finding a suitable block
    for (int i = 0; i < HEAPSIZE - 1;) {
        if (knuth_heap[i] == -blockSize) {  // Suitable free block found
            knuth_heap[i] = blockSize;  // Mark it as used
            knuth_heap[i + blockSize + 1] = blockSize;  // Set the end size marker
            knuth_hStats.free -= blockSize * sizeof(int);
            knuth_hStats.used += blockSize * sizeof(int);
            OS_bSignal(&knuth_HeapMutex);
            return &knuth_heap[i + 1];  // Return a pointer to the block, skipping the metadata
        }
        i += abs(knuth_heap[i]) + 2;  // Move to the next block
    }
		OS_bSignal(&knuth_HeapMutex);
    // No block is small enough, try to split a larger block
    if (cutBlock(blockSize) == 0) {
        void* ptr = Knuth_Heap_Malloc(desiredBytes);  // Try allocation again after splitting
        return ptr;
    }

    // Unable to find or create a suitable block
    return NULL;
}






//******** Heap_Calloc *************** 
// Allocate memory, data are initialized to 0
// input:
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory block or will return NULL
//   if there isn't sufficient space to satisfy allocation request
//notes: the allocated memory block will be zeroed out
void* Knuth_Heap_Calloc(int32_t desiredBytes) {
    void* ptr = Knuth_Heap_Malloc(desiredBytes);
    if (ptr) memset(ptr, 0, desiredBytes);
    return ptr;
}


//******** Heap_Realloc *************** 
// Reallocate buffer to a new size
//input: 
//  oldBlock: pointer to a block
//  desiredBytes: a desired number of bytes for a new block
// output: void* pointing to the new block or will return NULL
//   if there is any reason the reallocation can't be completed
// notes: the given block may be unallocated and its contents
//   are copied to a new block if growing/shrinking not possible
void* Knuth_Heap_Realloc(void* oldBlock, int32_t desiredBytes){
	    // Calculate the block index from the pointer.
	long* blockPointer = (long*)oldBlock;
	int index = blockPointer - knuth_heap - 1; // Adjust for the size metadata at the start.

	if (index < 0 || index > HEAPSIZE + 1) {
			return 0; // Error: Invalid pointer (out of bounds).
	}

	int blockSize = knuth_heap[index];
	if (blockSize <= 0) {
			return 0; // Error: Double free or corrupted heap (block already free).
	}
	int32_t temp[blockSize];
	memcpy(temp, oldBlock, blockSize);
	Knuth_Heap_Free(oldBlock);
	long* output = Knuth_Heap_Malloc(desiredBytes);
	if(!output) return NULL;

	memcpy(output, temp, MIN(blockSize,desiredBytes));
	
 
  return output;   // NULL
}


//Attempts to merge the block at the given index in the heap with its buddy
//Recursively attempts to merge if a merge is successful
void mergeBlock(int index){

    int blockSize = -knuth_heap[index];
		if(blockSize == HEAPSIZE - 2){
			return;
		}
    //Block is the 2nd in a pair, and its mate is free
    if(index/(blockSize+2) % 2 && knuth_heap[index - 1] == -blockSize){
        int prevBlockSize = -knuth_heap[index - 1];
				knuth_heap[index - 1] = 0;
				knuth_heap[index] = 0;
        knuth_heap[index - prevBlockSize - 2] = -(prevBlockSize + blockSize + 2);
        knuth_heap[index + blockSize + 1] = -(prevBlockSize + blockSize + 2);
        blockSize += prevBlockSize + 2; // Include the merged block size plus metadata.
        index -= (prevBlockSize + 2); // Update index to the start of the merged block.
        knuth_hStats.free += 8;
        mergeBlock(index);
    }
    //Block is first in a pair
    else{
        //Its mate is free
        if(knuth_heap[index + blockSize + 2] == -blockSize){
        int nextBlockSize = -knuth_heap[index + blockSize + 2];
				knuth_heap[index + blockSize + 2] = 0;
				knuth_heap[index + blockSize + 1] = 0;	
        knuth_heap[index] = -(blockSize + nextBlockSize + 2);
        knuth_heap[index + blockSize + nextBlockSize + 3] = -(blockSize + nextBlockSize + 2);
        knuth_hStats.free += 8;
        mergeBlock(index);
        }
    }


}



//******** Heap_Free *************** 
// return a block to the heap
// input: pointer to memory to unallocate
// output: 0 if everything is ok, non-zero in case of error (e.g. invalid pointer
//     or trying to unallocate memory that has already been unallocated
int32_t Knuth_Heap_Free(void* pointer){
    // Calculate the block index from the pointer.

    long* blockPointer = (long*)pointer;
    int index = blockPointer - knuth_heap - 1; // Adjust for the size metadata at the start.

    if (index < 0 || index > HEAPSIZE - 1) {
        return 1; // Error: Invalid pointer (out of bounds).
    }

    int blockSize = knuth_heap[index];
    if (blockSize <= 0) {
        return 2; // Error: Double free or corrupted heap (block already free).
    }

	OS_bWait(&knuth_HeapMutex);
    // Mark the block as free.
    knuth_heap[index] = -blockSize;
    knuth_heap[index + blockSize + 1] = -blockSize;
		
    knuth_hStats.used -= blockSize*4;
    knuth_hStats.free += blockSize*4;


    mergeBlock(index);
 
	OS_bSignal(&knuth_HeapMutex);
    return 0; // Success.
}

//******** Heap_Stats *************** 
// return the current status of the heap
// input: reference to a heap_stats_t that returns the current usage of the heap
// output: 0 in case of success, non-zeror in case of error (e.g. corrupted heap)
int32_t Knuth_Heap_Stats(heap_stats_t *stats){
	if(!knuth_init) return 1;
	stats->free = knuth_hStats.free;
	stats->used = knuth_hStats.used;
	stats->size = knuth_hStats.size;
  return 0;   // replace
}