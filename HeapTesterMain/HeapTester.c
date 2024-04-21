// HeapTester.c
// Runs on LM4F120/TM4C123

// Alex Prucka, 4/20/2024, 
// RTOS Final Project

// This file contains the main program for this project
// 4 different heap implementations have been created
// heap.c is a switchpoint to each different implementation
// heapfirstfit.c
// heapbestfit.c
// heapworstfit.c
// heapknuth.c

// Main contains a heap tester function to test the
// validity of each implementaiton, as well as
// a stress tester module, used to log data]
// about the performance of each heap implementation

// SW1 launches the heap tester
// SW2 launches the stress tester


//Built off of Lab 5 from RTOS, Credit to the following

// Jonathan W. Valvano 3/29/17, valvano@mail.utexas.edu
// Andreas Gerstlauer 3/1/16, gerstl@ece.utexas.edu
// EE445M/EE380L.6 
// You may use, edit, run or distribute this file 
// You are free to change the syntax/organization of this file



#include <stdint.h>
#include <stdio.h> 
#include <stdlib.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../inc/LaunchPad.h"
#include "../inc/PLL.h"
#include "../inc/LPF.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Labs_common/Interpreter.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"


uint32_t NumCreated;   // number of foreground threads created
uint32_t IdleCount;    // CPU idle counter

//---------------------User debugging-----------------------
extern int32_t MaxJitter;             // largest time jitter between interrupts in usec

#define PD0  (*((volatile uint32_t *)0x40007004))
#define PD1  (*((volatile uint32_t *)0x40007008))
#define PD2  (*((volatile uint32_t *)0x40007010))
#define PD3  (*((volatile uint32_t *)0x40007020))

void PortD_Init(void){ 
  SYSCTL_RCGCGPIO_R |= 0x08;       // activate port D
  while((SYSCTL_RCGCGPIO_R&0x08)==0){};      
  GPIO_PORTD_DIR_R |= 0x0F;        // make PD3-0 output heartbeats
  GPIO_PORTD_AFSEL_R &= ~0x0F;     // disable alt funct on PD3-0
  GPIO_PORTD_DEN_R |= 0x0F;        // enable digital I/O on PD3-0
  GPIO_PORTD_PCTL_R = ~0x0000FFFF;
  GPIO_PORTD_AMSEL_R &= ~0x0F;;    // disable analog functionality on PD
}




 //------------------Idle Task--------------------------------
 // foreground thread, runs when nothing else does
 // never blocks, never sleeps, never dies
 // inputs:  none
 // outputs: none
 void Idle(void){
   IdleCount = 0;          
   while(1) {
     IdleCount++;
     PD0 ^= 0x01;
     WaitForInterrupt();
   }
 }



//*****************Heap Tester*************************
// Heap test, allocate and deallocate memory for
// each heap type
void heapError(const char* errtype,const char* v,uint32_t n){
  printf("%s",errtype);
  printf(" heap error %s%u",v,n);
  OS_Kill();
}
heap_stats_t stats;
void heapStats(void){
  if(Heap_Stats(&stats))  heapError("Heap_Stats","",0);
  ST7735_Message(1,0,"Heap size  =",stats.size); 
  ST7735_Message(1,1,"Heap used  =",stats.used);  
  ST7735_Message(1,2,"Heap free  =",stats.free);
  ST7735_Message(1,3,"Heap waste =",stats.size - stats.used - stats.free);
	//printf("Heap size  =%u\n\r",stats.size);
	//printf("Heap used  =%u\n\r",stats.used);
	//printf("Heap free  =%u\n\r",stats.free);
	//printf("Heap waste  =%u\n\r",stats.size - stats.used - stats.free);
}
int16_t* ptr;  // Global so easier to see with the debugger
int16_t* p1;   // Proper style would be to make these variables local
int16_t* p2;
int16_t* p3;
uint8_t* q1;
uint8_t* q2;
uint8_t* q3;
uint8_t* q4;
uint8_t* q5;
uint8_t* q6;
int16_t maxBlockSize;
uint8_t* bigBlock;

int testLock = 0;

void TestHeap(void){  int16_t i;
	testLock = 1;

	for (int heapType = 1; heapType <= 4; heapType++) {
    // Convert heapType number to its corresponding name
    const char* heapTypeName;
    switch (heapType) {
        case 1:
            heapTypeName = "First Fit";
            break;
        case 2:
            heapTypeName = "Best Fit";
            break;
        case 3:
            heapTypeName = "Worst Fit";
            break;
        case 4:
            heapTypeName = "Knuths Buddy";
            break;
        default:
            heapTypeName = "Unknown";
            break;
    }

    // Display on LCD
    char displayText[30];
    snprintf(displayText, sizeof(displayText), "%s Heap test            ", heapTypeName);
    ST7735_DrawString(0, 0, displayText, ST7735_WHITE);

    // Print to console
    printf("\n\rEE445M/EE380L, Lab 5 %s Heap Test\n\r", heapTypeName);
		if(Heap_Init(heapType))         heapError("Heap_Init","",0);

		ptr = Heap_Malloc(sizeof(int16_t));
		if(!ptr)                heapError("Heap_Malloc","ptr",0);
		*ptr = 0x1111;

		if(Heap_Free(ptr))      heapError("Heap_Free","ptr",0);

		ptr = Heap_Malloc(1);
		if(!ptr)                heapError("Heap_Malloc","ptr",1);

		if(Heap_Free(ptr))      heapError("Heap_Free","ptr",1);

		p1 = (int16_t*) Heap_Malloc(1 * sizeof(int16_t));
		if(!p1)                 heapError("Heap_Malloc","p",1);
		p2 = (int16_t*) Heap_Malloc(2 * sizeof(int16_t));
		if(!p2)                 heapError("Heap_Malloc","p",2);
		p3 = (int16_t*) Heap_Malloc(3 * sizeof(int16_t));
		if(!p3)                 heapError("Heap_Malloc","p",3);
		p1[0] = 0xAAAA;
		p2[0] = 0xBBBB;
		p2[1] = 0xBBBB;
		p3[0] = 0xCCCC;
		p3[1] = 0xCCCC;
		p3[2] = 0xCCCC;
		heapStats();

		if(Heap_Free(p1))       heapError("Heap_Free","p",1);
		if(Heap_Free(p3))       heapError("Heap_Free","p",3);

		if(Heap_Free(p2))       heapError("Heap_Free","p",2);
		heapStats();

		for(i = 0; i <= (stats.size / sizeof(int32_t)); i++){
			ptr = Heap_Malloc(sizeof(int16_t));
			if(!ptr) break;
		}
		if(ptr)                 heapError("Heap_Malloc","i",i);
		heapStats();
		
		printf("Realloc test\n\r");
		if(Heap_Init(heapType))         heapError("Heap_Init","",1);
		q1 = Heap_Malloc(1);
		if(!q1)                 heapError("Heap_Malloc","q",1);
		q2 = Heap_Malloc(2);
		if(!q2)                 heapError("Heap_Malloc","q",2);
		q3 = Heap_Malloc(3);
		if(!q3)                 heapError("Heap_Malloc","q",3);
		q4 = Heap_Malloc(4);
		if(!q4)                 heapError("Heap_Malloc","q",4);
		q5 = Heap_Malloc(5);
		if(!q5)                 heapError("Heap_Malloc","q",5);

		*q1 = 0xDD;
		q6 = Heap_Realloc(q1, 6);
		heapStats();

		for(i = 0; i < 6; i++){
			q6[i] = 0xEE;
		}
		q1 = Heap_Realloc(q6, 2);
		heapStats();

		printf("Large block test\n\r");
		if(Heap_Init(heapType))         heapError("Heap_Init","",2);
		heapStats();
		maxBlockSize = stats.free;
		bigBlock = Heap_Malloc(maxBlockSize);
		for(i = 0; i < maxBlockSize; i++){
			bigBlock[i] = 0xFF;
		}
		heapStats();
		if(Heap_Free(bigBlock)) heapError("Heap_Free","bigBlock",0);

		bigBlock = Heap_Calloc(maxBlockSize);
		if(!bigBlock)           heapError("Heap_Calloc","bigBlock",0);
		if(*bigBlock)           heapError("Zero initialization","bigBlock",0);
		heapStats();

		if(Heap_Free(bigBlock)) heapError("Heap_Free","bigBlock",0);
		heapStats();
		
		printf("Successful heap test\n\r");
		ST7735_DrawString(0, 0, "Heap test successful", ST7735_YELLOW);
	}
	testLock = 0;
	OS_Kill();
}


//*****************Stress Tester*************************
// Stress test each heap implementaiton, logging the output 
// via UART. The serial output can then be saved to a CSV and
// analyzed. 

// The same random seed is used for each heap implementaiton
// For each test, the stress tester does three things

// 1: Attempt to allocate 10 files of random sizes
//    Log how much time it takes to do this, how much memory
//    Is successfully allocated, and if any failures to allocate
//    Occur
// 2: Free all successfully allocated blocks
//    Log the time it takes to free the blocks
// 3: Complete a sequence of allocations and deallocations
// 		Time how long it takes to make these operations, and
//    Track the average success of the operations.
//
//  After the allocation and mixed operation stages, the state
//  of the heap including waste, used, and free space is saved
//
//  The seed is set for each heap type before NUMTESTS occur, so 
//  the allocated memory in each of NUMTESTS should be unique
//  1000 tests was used for data collecting and analysis

int stressLock = 0;
#define NUMTESTS 1000
#define NUMALLOCS 60

void stressTestHeap() {
		stressLock = 1;
    printf("HeapType,OperationIndex,AllocationTime,AllocatedBytes,AllocationsFailed,FreeTime,MixedOperationTime,TotalAllocatedDuringMixed,UsedAfterAlloc,FreeAfterAlloc,WasteAfterAlloc,UsedAfterMixed,FreeAfterMixed,WasteAfterMixed\n\r");  // Updated CSV Header
    for (int heapType = 1; heapType <= 4; heapType++) {
        Heap_Init(heapType);  // Initialize the heap for the given type

        srand(12345);  // Fixed seed for reproducibility across tests

        for (int j = 0; j < NUMTESTS; j++) {  
            unsigned long start, stop, elapsed;
            void *pointers[NUMALLOCS];  
            int sizes[NUMALLOCS];       
            heap_stats_t stats_before, stats_after;

            // Malloc Phase
            start = OS_Time();
            int totalAllocated1 = 0;
            int allocsFailed = 0;
            int numAllocations = 0;
            for (int i = 0; i < NUMALLOCS; i++) {
                int size = (rand() % 150 + 1);  // Allocation sizes between 1 and 150 bytes
                pointers[i] = Heap_Malloc(size);
                if (!pointers[i]) {
                    allocsFailed += 1;
                }
								else{
									sizes[i] = size;
									totalAllocated1 += size;
									numAllocations++;
								}

            }
            stop = OS_Time();
            elapsed = OS_TimeDifference(start, stop);
            
            Heap_Stats(&stats_before);  // Get heap stats after initial allocations

            // Free Phase
            start = OS_Time();
						for (int i = 0; i < NUMALLOCS; i++) {  // Loop through the whole array 
								if (pointers[i]) {  // Check if the pointer is not NULL
										Heap_Free(pointers[i]);
										pointers[i] = NULL;  // Nullify the pointer after freeing
								}
						}
            stop = OS_Time();
            unsigned long freeTime = OS_TimeDifference(start, stop);
            
						// Mixed Malloc/Free Operations
						start = OS_Time();
						int totalOperations = 100;  // Total number of malloc and free operations
						int allocCount = 0;  // To keep track of the number of successful mallocs
						int mallocOperations = 60;
						int freeOperations = 40;
						int operation;  // 0 for malloc, 1 for free
						int totalAllocated2 = 0;

						for (int op = 0; op < totalOperations; op++) {
								if (mallocOperations > 0 && (rand() % 2 == 0 || freeOperations == 0)) {
										operation = 0;  // Perform malloc
										mallocOperations--;
								} else if (freeOperations > 0) {
										operation = 1;  // Perform free
										freeOperations--;
								}

								if (operation == 0 && allocCount < NUMALLOCS) {  // Ensure there's space to allocate
										int size = (rand() % 200 + 1);  // Size between 1 and 200 bytes
										pointers[allocCount] = Heap_Malloc(size);
										if (pointers[allocCount]) {
												sizes[allocCount] = size;
												totalAllocated2 += size;
												allocCount++;
										}
								} else if (operation == 1 && allocCount > 0) {  // Ensure there's something to free
										int freeIndex = rand() % allocCount;  // Pick a random index to free
										if (pointers[freeIndex]) {
												Heap_Free(pointers[freeIndex]);
												pointers[freeIndex] = NULL;

												// Shift the remaining pointers down if necessary
												for (int i = freeIndex; i < allocCount - 1; i++) {
														pointers[i] = pointers[i + 1];
														sizes[i] = sizes[i + 1];
												}
												allocCount--;
												pointers[allocCount] = NULL;  // Clear the last element
										}
								}
						}
						stop = OS_Time();
						unsigned long mixedOpTime = OS_TimeDifference(start, stop);
            
            Heap_Stats(&stats_after);  // Get heap stats after mixed operations

						// Cleanup: Free any remaining allocations
						for (int i = 0; i < NUMALLOCS; i++) {  // Loop through the whole array regardless of allocIndex
								if (pointers[i]) {  // Check if the pointer is not NULL
										Heap_Free(pointers[i]);
										pointers[i] = NULL;  // Nullify the pointer after freeing
								}
						}
						//Heap_Init(heapType);



            // Print results in CSV format
            printf("%d,%d,%lu,%d,%d,%lu,%lu,%d,%u,%u,%u,%u,%u,%u\n\r",
                   heapType, j, elapsed, totalAllocated1, allocsFailed, freeTime, mixedOpTime, totalAllocated2,
                   stats_before.used, stats_before.free, stats_before.size - stats_before.used - stats_before.free,
                   stats_after.used, stats_after.free, stats_after.size - stats_after.used - stats_after.free);
        }
			printf("\r\n");
    }
		stressLock = 0;
    OS_Kill();  // Assuming OS_Kill terminates the program
}

void SW1Push(void){
	if(!testLock){
		if(OS_MsTime() > 60){ // debounce
			if(OS_AddThread(&TestHeap,128,2)){
				NumCreated++;
			}
			OS_ClearMsTime();  // at least 60ms between touches
		}
	}
}

void SW2Push(void){
	if(!stressLock){
		if(OS_MsTime() > 60){ // debounce
			if(OS_AddThread(&stressTestHeap,128,1)){
				NumCreated++;
			}
			OS_ClearMsTime();  // at least 60ms between touches
		}
	}
}

int main(void){   
  OS_Init();           // initialize, disable interrupts
  PortD_Init();

  // attach switch tasks
  OS_AddSW1Task(&SW1Push,2);
	OS_AddSW2Task(&SW2Push,1);
    
  // Only Idle thread
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Idle,128,3); 
 
  OS_Launch(10*TIME_1MS); // doesn't return, interrupts enabled in here
  return 0;               // this never executes
}

