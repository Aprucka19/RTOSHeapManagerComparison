// filename *************************heap.c ************************
// Implements memory heap for dynamic memory allocation.
// Follows standard malloc/calloc/realloc/free interface
// for allocating/unallocating memory.
// Acts as a bounce board swapping between different heap
// Implementations depending on the value passed to init


#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/heapfirstfit.h"
#include "../RTOS_Labs_common/heapworstfit.h"
#include "../RTOS_Labs_common/heapbestfit.h"
#include "../RTOS_Labs_common/heapknuth.h"

int HeapType = 0;
//******** Heap_Init *************** 
// Initialize the Heap
// input: int 1 - 4 
//   1: first fit
//   2: best fit
//   3: worst fit
//   4: knuths buddy algo
// output: 0 if successful, 1 if init failed
// notes: Initializes/resets the heap to a clean state where no memory
//  is allocated. Heaps all contain 1016 useable bytes in best case
//  Eg if a single malloc is called after initializing the heap, 1016 is 
//  the maximum valid value
int32_t Heap_Init(uint32_t heaptype){
	// Switch statement to handle different cases
	HeapType = heaptype;
    switch(HeapType) {
        case 1:
            return First_Heap_Init();
            break;
        case 2:
            return Best_Heap_Init();
            break;
        case 3:
            return Worst_Heap_Init();
            break;
        case 4:
            return Knuth_Heap_Init();
            break;
        default:
			//Invalid init.
            return 1;
    }
}


//******** Heap_Malloc *************** 
// Allocate memory, data not initialized
// input: 
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory or will return NULL
//   if there isn't sufficient space to satisfy allocation request

void* Heap_Malloc(int32_t desiredBytes){
	
    switch(HeapType) {
        case 1:
            return First_Heap_Malloc(desiredBytes);
            break;
        case 2:
            return Best_Heap_Malloc(desiredBytes);
            break;
        case 3:
            return Worst_Heap_Malloc(desiredBytes);
            break;
        case 4:
            return Knuth_Heap_Malloc(desiredBytes);
            break;
        default:
			//Invalid init.
            return NULL;
    }
}


//******** Heap_Calloc *************** 
// Allocate memory, data are initialized to 0
// input:
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory block or will return NULL
//   if there isn't sufficient space to satisfy allocation request
//notes: the allocated memory block will be zeroed out
void* Heap_Calloc(int32_t desiredBytes){
    switch(HeapType) {
        case 1:
            return First_Heap_Calloc(desiredBytes);
            break;
        case 2:
            return Best_Heap_Calloc(desiredBytes);
            break;
        case 3:
            return Worst_Heap_Calloc(desiredBytes);
            break;
        case 4:
            return Knuth_Heap_Calloc(desiredBytes);
            break;
        default:
			//Invalid init.
            return NULL;
    }
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
void* Heap_Realloc(void* oldBlock, int32_t desiredBytes){
    switch(HeapType) {
        case 1:
            return First_Heap_Realloc(oldBlock, desiredBytes);
            break;
        case 2:
            return Best_Heap_Calloc(desiredBytes);
            break;
        case 3:
            return Worst_Heap_Calloc(desiredBytes);
            break;
        case 4:
            return Knuth_Heap_Calloc(desiredBytes);
            break;
        default:
			//Invalid init.
            return NULL;
    }
}


//******** Heap_Free *************** 
// return a block to the heap
// input: pointer to memory to unallocate
// output: 0 if everything is ok, non-zero in case of error (e.g. invalid pointer
//     or trying to unallocate memory that has already been unallocated
int32_t Heap_Free(void* pointer){
    switch(HeapType) {
        case 1:
            return First_Heap_Free(pointer);
            break;
        case 2:
            return Best_Heap_Free(pointer);
            break;
        case 3:
            return Worst_Heap_Free(pointer);
            break;
        case 4:
            return Knuth_Heap_Free(pointer);
            break;
        default:
			//Invalid init
            return 1;
    }
}

//******** Heap_Stats *************** 
// return the current status of the heap
// input: reference to a heap_stats_t that returns the current usage of the heap
// output: 0 in case of success, non-zeror in case of error (e.g. corrupted heap)
int32_t Heap_Stats(heap_stats_t *stats){
    switch(HeapType) {
        case 1:
            return First_Heap_Stats(stats);
            break;
        case 2:
            return Best_Heap_Stats(stats);
            break;
        case 3:
            return Worst_Heap_Stats(stats);
            break;
        case 4:
            return Knuth_Heap_Stats(stats);
            break;
        default:
			//Invalid init
            return 1;
    }
}
