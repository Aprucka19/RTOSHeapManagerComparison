// heapbestfit.c
// Alex Prucka
// RTOS Grad Project
// Implementation of worst fit
// for heap management


#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../RTOS_Labs_common/heapworstfit.h"
#include "../RTOS_Labs_common/OS.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define HEAPSIZE 1022


//Heap Stats object
struct heap_stats worst_hStats;
long worst_heap[HEAPSIZE+2];
int worst_init = 0;
Sema4Type worst_HeapMutex;

//******** Heap_Init *************** 
// Initialize the Heap
// input: none
// output: always 0
// notes: Initializes/resets the heap to a clean state where no memory
//  is allocated.
int32_t Worst_Heap_Init(){
	worst_hStats.size = (HEAPSIZE + 2) * 4;
	worst_hStats.used = 0;
	worst_hStats.free = HEAPSIZE * 4;
	memset(worst_heap, 0, sizeof(worst_heap));
	worst_heap[0] = -HEAPSIZE;
	worst_heap[HEAPSIZE+1] = -HEAPSIZE;
	worst_init = 1;
	worst_HeapMutex.Value = 1;
	
  return 0;   // replace
}


//******** Heap_Malloc *************** 
// Allocate memory, data not initialized
// input: 
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory or will return NULL
//   if there isn't sufficient space to satisfy allocation request
int worst_sectorsNeeded;
int worst_spaceLeft;


void* Worst_Heap_Malloc(int32_t desiredBytes){
    OS_bWait(&worst_HeapMutex);

    worst_sectorsNeeded = (desiredBytes + 3) / 4;
    int worst_index = -1; // Track the index of the worst fit block
    int worst_fit_size = -1; // Initialize to an invalid size for comparison

    // Search the heap
    for (int i = 0; i < HEAPSIZE - 1;) {
        // Check if current block is free and big enough
        if (-worst_heap[i] >= worst_sectorsNeeded) {
            int current_block_size = -worst_heap[i];
            if (current_block_size > worst_fit_size) {
                worst_fit_size = current_block_size;
                worst_index = i;
            }
        }
        // Move to next block
        i += (abs(worst_heap[i]) + 2);
    }

    // If a valid block has been found
    if (worst_index != -1) {
        int spaceLeft = worst_fit_size - worst_sectorsNeeded;
        // Check if the space left after allocation is too small to be useful
        if (spaceLeft < 3) {
            // Use the entire block
            worst_heap[worst_index] = worst_fit_size;
            worst_heap[worst_index + worst_fit_size + 1] = worst_fit_size;
            worst_hStats.free -= worst_fit_size * 4;
            worst_hStats.used += worst_fit_size * 4;
        } else {
            // Allocate only the space needed and update the remaining block
            worst_heap[worst_index] = worst_sectorsNeeded;
            worst_heap[worst_index + worst_sectorsNeeded + 1] = worst_sectorsNeeded;
            worst_heap[worst_index + worst_sectorsNeeded + 2] = -(spaceLeft - 2);
            worst_heap[worst_index + worst_sectorsNeeded + spaceLeft + 1] = -(spaceLeft - 2);
            worst_hStats.free -= (worst_sectorsNeeded + 2) * 4;
            worst_hStats.used += worst_sectorsNeeded * 4;
        }
        OS_bSignal(&worst_HeapMutex);
        return &worst_heap[worst_index + 1];
    }

    // If no suitable block is found
    OS_bSignal(&worst_HeapMutex);
    return NULL; // No block large enough was found
}



//******** Heap_Calloc *************** 
// Allocate memory, data are initialized to 0
// input:
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory block or will return NULL
//   if there isn't sufficient space to satisfy allocation request
//notes: the allocated memory block will be zeroed out
void* Worst_Heap_Calloc(int32_t desiredBytes){
    void* ptr = Worst_Heap_Malloc(desiredBytes);
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
void* Worst_Heap_Realloc(void* oldBlock, int32_t desiredBytes){
	    // Calculate the block index from the pointer.
	long* blockPointer = (long*)oldBlock;
	int index = blockPointer - worst_heap - 1; // Adjust for the size metadata at the start.

	if (index < 0 || index > HEAPSIZE + 1) {
			return 0; // Error: Invalid pointer (out of bounds).
	}

	int blockSize = worst_heap[index];
	if (blockSize <= 0) {
			return 0; // Error: Double free or corrupted heap (block already free).
	}
	int32_t temp[blockSize];
	memcpy(temp, oldBlock, blockSize);
	Worst_Heap_Free(oldBlock);
	long* output = Worst_Heap_Malloc(desiredBytes);
	if(!output) return NULL;

	memcpy(output, temp, MIN(blockSize,desiredBytes));
	
 
  return output;   // NULL
}


//******** Heap_Free *************** 
// return a block to the heap
// input: pointer to memory to unallocate
// output: 0 if everything is ok, non-zero in case of error (e.g. invalid pointer
//     or trying to unallocate memory that has already been unallocated
int32_t Worst_Heap_Free(void* pointer){
    // Calculate the block index from the pointer.

    long* blockPointer = (long*)pointer;
    int index = blockPointer - worst_heap - 1; // Adjust for the size metadata at the start.

    if (index < 0 || index > HEAPSIZE + 1) {
        return 1; // Error: Invalid pointer (out of bounds).
    }

    int blockSize = worst_heap[index];
    if (blockSize <= 0) {
        return 2; // Error: Double free or corrupted heap (block already free).
    }
		OS_bWait(&worst_HeapMutex);
    // Mark the block as free.
    worst_heap[index] = -blockSize;
    worst_heap[index + blockSize + 1] = -blockSize;
		
		worst_hStats.used -= blockSize*4;
		worst_hStats.free += blockSize*4;

    // Coalesce with previous block if free.
    if (index > 2 && worst_heap[index - 1] < 0) {
        int prevBlockSize = -worst_heap[index - 1];
        worst_heap[index - prevBlockSize - 2] = -(prevBlockSize + blockSize + 2);
        worst_heap[index + blockSize + 1] = -(prevBlockSize + blockSize + 2);
        blockSize += prevBlockSize + 2; // Include the merged block size plus metadata.
        index -= (prevBlockSize + 2); // Update index to the start of the merged block.
				worst_hStats.free += 8;
    }

    // Coalesce with next block if free.
   
		if (index + blockSize + 2 < HEAPSIZE+2 && worst_heap[index + blockSize + 2] < 0) {
        int nextBlockSize = -worst_heap[index + blockSize + 2];
        worst_heap[index] = -(blockSize + nextBlockSize + 2);
        worst_heap[index + blockSize + nextBlockSize + 3] = -(blockSize + nextBlockSize + 2);
				worst_hStats.free += 8;
    }

    // Update stats.
 
		OS_bSignal(&worst_HeapMutex);
    return 0; // Success.
}

//******** Heap_Stats *************** 
// return the current status of the heap
// input: reference to a heap_stats_t that returns the current usage of the heap
// output: 0 in case of success, non-zeror in case of error (e.g. corrupted heap)
int32_t Worst_Heap_Stats(heap_stats_t *stats){
	if(!worst_init) return 1;
	stats->free = worst_hStats.free;
	stats->used = worst_hStats.used;
	stats->size = worst_hStats.size;
  return 0;   // replace
}
