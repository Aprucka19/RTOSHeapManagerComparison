// heapbestfit.c
// Alex Prucka
// RTOS Grad Project
// Implementation of best fit
// for heap management


#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../RTOS_Labs_common/heapbestfit.h"
#include "../RTOS_Labs_common/OS.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define HEAPSIZE 1022


//Heap Stats object
struct heap_stats best_hStats;
long best_heap[HEAPSIZE+2];
int best_init = 0;
Sema4Type best_HeapMutex;

//******** Heap_Init *************** 
// Initialize the Heap
// input: none
// output: always 0
// notes: Initializes/resets the heap to a clean state where no memory
//  is allocated.
int32_t Best_Heap_Init(){
	best_hStats.size = (HEAPSIZE + 2) * 4;
	best_hStats.used = 0;
	best_hStats.free = HEAPSIZE * 4;
	memset(best_heap, 0, sizeof(best_heap));
	best_heap[0] = -HEAPSIZE;
	best_heap[HEAPSIZE+1] = -HEAPSIZE;
	best_init = 1;
	best_HeapMutex.Value = 1;
	
  return 0;   // replace
}


//******** Heap_Malloc *************** 
// Allocate memory, data not initialized
// input: 
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory or will return NULL
//   if there isn't sufficient space to satisfy allocation request
int best_sectorsNeeded;
int best_spaceLeft;


void* Best_Heap_Malloc(int32_t desiredBytes){
    OS_bWait(&best_HeapMutex);

    best_sectorsNeeded = (desiredBytes + 3) / 4;
    int best_index = -1; // Track the index of the best fit block
    int best_fit_size = HEAPSIZE; //

    // Search the heap
    for (int i = 0; i < HEAPSIZE - 1;) {
        // Check if current block is free and big enough
        if (-best_heap[i] >= best_sectorsNeeded) {
            int current_block_size = -best_heap[i];
            if (current_block_size <= best_fit_size && current_block_size >= best_sectorsNeeded) {
                best_fit_size = current_block_size;
                best_index = i;
                // Break if exact match is found (optional for further optimization)
                if (best_fit_size == best_sectorsNeeded) break;
            }
        }
        // Move to next block
        i += (abs(best_heap[i]) + 2);
    }

    // If a valid block has been found
    if (best_index != -1) {
        int spaceLeft = best_fit_size - best_sectorsNeeded;
        // Check if the space left after allocation is too small to be useful
        if (spaceLeft < 3) {
            // Use the entire block
            best_heap[best_index] = best_fit_size;
            best_heap[best_index + best_fit_size + 1] = best_fit_size;
            best_hStats.free -= best_fit_size * 4;
            best_hStats.used += best_fit_size * 4;
        } else {
            // Allocate only the space needed and update the remaining block
            best_heap[best_index] = best_sectorsNeeded;
            best_heap[best_index + best_sectorsNeeded + 1] = best_sectorsNeeded;
            best_heap[best_index + best_sectorsNeeded + 2] = -(spaceLeft - 2);
            best_heap[best_index + best_sectorsNeeded + spaceLeft + 1] = -(spaceLeft - 2);
            best_hStats.free -= (best_sectorsNeeded + 2) * 4;
            best_hStats.used += best_sectorsNeeded * 4;
        }
        OS_bSignal(&best_HeapMutex);
        return &best_heap[best_index + 1];
    }

    // If no suitable block is found
    OS_bSignal(&best_HeapMutex);
    return NULL; // No block large enough was found
}


//******** Heap_Calloc *************** 
// Allocate memory, data are initialized to 0
// input:
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory block or will return NULL
//   if there isn't sufficient space to satisfy allocation request
//notes: the allocated memory block will be zeroed out
void* Best_Heap_Calloc(int32_t desiredBytes){
    void* ptr = Best_Heap_Malloc(desiredBytes);
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
void* Best_Heap_Realloc(void* oldBlock, int32_t desiredBytes){
	    // Calculate the block index from the pointer.
	long* blockPointer = (long*)oldBlock;
	int index = blockPointer - best_heap - 1; // Adjust for the size metadata at the start.

	if (index < 0 || index > HEAPSIZE + 1) {
			return 0; // Error: Invalid pointer (out of bounds).
	}

	int blockSize = best_heap[index];
	if (blockSize <= 0) {
			return 0; // Error: Double free or corrupted heap (block already free).
	}
	int32_t temp[blockSize];
	memcpy(temp, oldBlock, blockSize);
	Best_Heap_Free(oldBlock);
	long* output = Best_Heap_Malloc(desiredBytes);
	if(!output) return NULL;

	memcpy(output, temp, MIN(blockSize,desiredBytes));
	
 
  return output;   // NULL
}


//******** Heap_Free *************** 
// return a block to the heap
// input: pointer to memory to unallocate
// output: 0 if everything is ok, non-zero in case of error (e.g. invalid pointer
//     or trying to unallocate memory that has already been unallocated
int32_t Best_Heap_Free(void* pointer){
    // Calculate the block index from the pointer.

    long* blockPointer = (long*)pointer;
    int index = blockPointer - best_heap - 1; // Adjust for the size metadata at the start.

    if (index < 0 || index > HEAPSIZE + 1) {
        return 1; // Error: Invalid pointer (out of bounds).
    }

    int blockSize = best_heap[index];
    if (blockSize <= 0) {
        return 2; // Error: Double free or corrupted heap (block already free).
    }
		OS_bWait(&best_HeapMutex);
    // Mark the block as free.
    best_heap[index] = -blockSize;
    best_heap[index + blockSize + 1] = -blockSize;
		
		best_hStats.used -= blockSize*4;
		best_hStats.free += blockSize*4;

    // Coalesce with previous block if free.
    if (index > 2 && best_heap[index - 1] < 0) {
        int prevBlockSize = -best_heap[index - 1];
        best_heap[index - prevBlockSize - 2] = -(prevBlockSize + blockSize + 2);
        best_heap[index + blockSize + 1] = -(prevBlockSize + blockSize + 2);
        blockSize += prevBlockSize + 2; // Include the merged block size plus metadata.
        index -= (prevBlockSize + 2); // Update index to the start of the merged block.
				best_hStats.free += 8;
    }

    // Coalesce with next block if free.
   
		if (index + blockSize + 2 < HEAPSIZE+2 && best_heap[index + blockSize + 2] < 0) {
        int nextBlockSize = -best_heap[index + blockSize + 2];
        best_heap[index] = -(blockSize + nextBlockSize + 2);
        best_heap[index + blockSize + nextBlockSize + 3] = -(blockSize + nextBlockSize + 2);
				best_hStats.free += 8;
    }

    // Update stats.
 
		OS_bSignal(&best_HeapMutex);
    return 0; // Success.
}

//******** Heap_Stats *************** 
// return the current status of the heap
// input: reference to a heap_stats_t that returns the current usage of the heap
// output: 0 in case of success, non-zeror in case of error (e.g. corrupted heap)
int32_t Best_Heap_Stats(heap_stats_t *stats){
	if(!best_init) return 1;
	stats->free = best_hStats.free;
	stats->used = best_hStats.used;
	stats->size = best_hStats.size;
  return 0;   // replace
}
