// heapbestfit.c
// Alex Prucka
// RTOS Grad Project
// Implementation of first fit
// for heap management

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../RTOS_Labs_common/heapfirstfit.h"
#include "../RTOS_Labs_common/OS.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define HEAPSIZE 1022


//Heap Stats object
struct heap_stats first_hStats;
long first_heap[HEAPSIZE+2];
int first_init = 0;
Sema4Type first_HeapMutex;

//******** Heap_Init *************** 
// Initialize the Heap
// input: none
// output: always 0
// notes: Initializes/resets the heap to a clean state where no memory
//  is allocated.
int32_t First_Heap_Init(){
	first_hStats.size = (HEAPSIZE + 2) * 4;
	first_hStats.used = 0;
	first_hStats.free = HEAPSIZE * 4;
	memset(first_heap, 0, sizeof(first_heap));
	first_heap[0] = -HEAPSIZE;
	first_heap[HEAPSIZE+1] = -HEAPSIZE;
	first_init = 1;
	first_HeapMutex.Value = 1;
	
  return 0;   // replace
}


//******** Heap_Malloc *************** 
// Allocate memory, data not initialized
// input: 
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory or will return NULL
//   if there isn't sufficient space to satisfy allocation request
int first_sectorsNeeded;
int first_spaceLeft;


void* First_Heap_Malloc(int32_t desiredBytes){
	
	OS_bWait(&first_HeapMutex);
	
	first_sectorsNeeded = (desiredBytes+3)/4;
	//Search the heap
	for(int i = 0; i < HEAPSIZE - 1;){
		//Found a section big enough?
		if(-first_heap[i] >= first_sectorsNeeded){
			//Big enough to split the section?
			first_spaceLeft = -first_sectorsNeeded - first_heap[i];
			if (first_spaceLeft < 3){
				//Allocate whole space
				first_heap[i] = abs(first_heap[i]);
				first_heap[i+first_heap[i]+1] = first_heap[i];
				
				//Update Stats
				first_hStats.free -= first_heap[i]*4;
				first_hStats.used += first_heap[i]*4; 
				OS_bSignal(&first_HeapMutex);
				return &first_heap[i+1];
			}
			else{
				//Allocate what is needed, make new empty segment after
				first_heap[i] = first_sectorsNeeded;
				first_heap[i+first_sectorsNeeded+1] = first_sectorsNeeded;
				
				first_heap[i+first_sectorsNeeded+2] = -(first_spaceLeft - 2);
				first_heap[i+first_sectorsNeeded+1+first_spaceLeft] = -(first_spaceLeft - 2);

				//update heapstats
				first_hStats.free -= (first_sectorsNeeded + 2)*4;
				first_hStats.used += first_sectorsNeeded*4; 				
				OS_bSignal(&first_HeapMutex);
				return &first_heap[i+1];
			}
		}
		//Check next block
		i  += (abs(first_heap[i])+2);
	}
	OS_bSignal(&first_HeapMutex);
  return NULL;   // NULL
}


//******** Heap_Calloc *************** 
// Allocate memory, data are initialized to 0
// input:
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory block or will return NULL
//   if there isn't sufficient space to satisfy allocation request
//notes: the allocated memory block will be zeroed out
void* First_Heap_Calloc(int32_t desiredBytes){
    void* ptr = First_Heap_Malloc(desiredBytes);
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
void* First_Heap_Realloc(void* oldBlock, int32_t desiredBytes){
	    // Calculate the block index from the pointer.
	long* blockPointer = (long*)oldBlock;
	int index = blockPointer - first_heap - 1; // Adjust for the size metadata at the start.

	if (index < 0 || index > HEAPSIZE + 1) {
			return 0; // Error: Invalid pointer (out of bounds).
	}

	int blockSize = first_heap[index];
	if (blockSize <= 0) {
			return 0; // Error: Double free or corrupted heap (block already free).
	}
	int32_t temp[blockSize];
	memcpy(temp, oldBlock, blockSize);
	First_Heap_Free(oldBlock);
	long* output = First_Heap_Malloc(desiredBytes);
	if(!output) return NULL;

	memcpy(output, temp, MIN(blockSize,desiredBytes));
	
 
  return output;   // NULL
}


//******** Heap_Free *************** 
// return a block to the heap
// input: pointer to memory to unallocate
// output: 0 if everything is ok, non-zero in case of error (e.g. invalid pointer
//     or trying to unallocate memory that has already been unallocated
int32_t First_Heap_Free(void* pointer){
    // Calculate the block index from the pointer.

    long* blockPointer = (long*)pointer;
    int index = blockPointer - first_heap - 1; // Adjust for the size metadata at the start.

    if (index < 0 || index > HEAPSIZE + 1) {
        return 1; // Error: Invalid pointer (out of bounds).
    }

    int blockSize = first_heap[index];
    if (blockSize <= 0) {
        return 2; // Error: Double free or corrupted heap (block already free).
    }
		OS_bWait(&first_HeapMutex);
    // Mark the block as free.
    first_heap[index] = -blockSize;
    first_heap[index + blockSize + 1] = -blockSize;
		
		first_hStats.used -= blockSize*4;
		first_hStats.free += blockSize*4;

    // Coalesce with previous block if free.
    if (index > 2 && first_heap[index - 1] < 0) {
        int prevBlockSize = -first_heap[index - 1];
        first_heap[index - prevBlockSize - 2] = -(prevBlockSize + blockSize + 2);
        first_heap[index + blockSize + 1] = -(prevBlockSize + blockSize + 2);
        blockSize += prevBlockSize + 2; // Include the merged block size plus metadata.
        index -= (prevBlockSize + 2); // Update index to the start of the merged block.
				first_hStats.free += 8;
    }

    // Coalesce with next block if free.
   
		if (index + blockSize + 2 < HEAPSIZE+2 && first_heap[index + blockSize + 2] < 0) {
        int nextBlockSize = -first_heap[index + blockSize + 2];
        first_heap[index] = -(blockSize + nextBlockSize + 2);
        first_heap[index + blockSize + nextBlockSize + 3] = -(blockSize + nextBlockSize + 2);
				first_hStats.free += 8;
    }

    // Update stats.
 
		OS_bSignal(&first_HeapMutex);
    return 0; // Success.
}

//******** Heap_Stats *************** 
// return the current status of the heap
// input: reference to a heap_stats_t that returns the current usage of the heap
// output: 0 in case of success, non-zeror in case of error (e.g. corrupted heap)
int32_t First_Heap_Stats(heap_stats_t *stats){
	if(!first_init) return 1;
	stats->free = first_hStats.free;
	stats->used = first_hStats.used;
	stats->size = first_hStats.size;
  return 0;   // replace
}
