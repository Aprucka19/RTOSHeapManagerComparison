// *************os.c**************
// EE445M/EE380L.6 Labs 1, 2, 3, and 4
// High-level OS functions
// Students will implement these functions as part of Lab
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano
// Jan 12, 2020, valvano@mail.utexas.edu

#include <stdint.h>
#include <stdio.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../inc/PLL.h"
#include "../inc/LaunchPad.h"
#include "../inc/Timer2A.h"
#include "../inc/Timer4A.h"
#include "../inc/WTimer0A.h"
#include "../inc/Timer5A.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
//#include "../RTOS_Labs_common/eFile.h"
#include "../inc/Timer2A.h"
#include "../inc/Timer1A.h"
#include "../inc/Timer0A.h"
#include "../inc/Switch.h"
#include "../RTOS_Labs_common/heap.h"

// Function prototypes from OSasm.s
// extern void StartOS(long *sp);
extern void StartOS(void);
extern void ContextSwitch(void);
extern void PendSV_Handler(void);
extern void Wait(Sema4Type *semaPt);
extern void Signal(Sema4Type *semaPt);
extern void bWait(Sema4Type *semaPt);
extern void bSignal(Sema4Type *semaPt);

/*
 * Thread Control Operations
 */
// Static allocation of heap for storing TCB data structure and stacks
#define STACK_SIZE 512
#define MAX_THREADS 5
#define MAX_PCBS 1
TCB TCBs[MAX_THREADS];
PCB PCBs[MAX_PCBS];
uint32_t Stacks[MAX_THREADS][STACK_SIZE];

uint8_t numThreads;		// Total number of threads
uint8_t numProcesses;
uint32_t numSleep;		// Number of threads in sleep state
uint32_t numActive;		// Number of threads in active state
uint32_t timeslice;

// Linked list pointers
TCB *RunPt;				// Pointer to the currently running thread.
TCB *Pri0;
TCB *Pri1;
TCB *Pri2;
TCB *Pri3;
TCB *Pri4;
TCB *Pri5;
TCB *SleepList;			// Pointer to the head of the sleep list.

TCB** GetPriPointer(uint32_t priority){
	switch(priority){
		case 0: return &Pri0;
		case 1: return &Pri1;
		case 2: return &Pri2;
		case 3: return &Pri3;
		case 4: return &Pri4;
		case 5: return &Pri5;
		default: return &Pri5;
	}
}



void initTCBs()
{
	RunPt = NULL;
	Pri0 = NULL;
	Pri1 = NULL;
	Pri2 = NULL;
	Pri3 = NULL;
	Pri4 = NULL;
	Pri5 = NULL;
	SleepList = NULL;

	numProcesses = 0;
	numThreads = 0;
	numActive = 0;
	numSleep = 0;
	for (int i = 0; i < MAX_THREADS; i++) 
	{
		TCBs[i].next = NULL;
		TCBs[i].prev = NULL;
		TCBs[i].state = NULL;	// expect that all tcbs are initially idle
		TCBs[i].parent = NULL;
	}
	
	for (int i = 0; i < MAX_PCBS; i++){
		PCBs[i].ThreadCount = 0;
	}
	
}

// Add a thread to a list
// Used for AddThread
void appendToList(TCB *tcb, TCB **listHead)
{
	if (*listHead == NULL)
	{
		*listHead = tcb;
		(*listHead)->next = tcb;
		(*listHead)->prev = tcb;
	}
	else
	{
		tcb->prev = (*listHead)->prev;
		tcb->next = (*listHead);
		(*listHead)->prev->next = tcb;
		(*listHead)->prev = tcb;
	}
	tcb->state = listHead;
}

// remove a TCB node from a list.
// Used to kill a thread
void removeFromList(TCB *tcb, TCB **listHead)
{
	if (tcb->next == tcb)
	{
		*listHead = NULL;
	}
	else 
	{
		if (*listHead == tcb)
		{
			*listHead = tcb->next;
		}
		tcb->prev->next = tcb->next;
		tcb->next->prev = tcb->prev;
	}
	tcb->state = NULL;
}

// update the state of tcb by moving it from one list to another
// Used when sleeping, waking, blocking...
void updateState(TCB *tcb, TCB **previousState, TCB **nextState)
{
	if (tcb->next == tcb)
	{
		*previousState = NULL;
	}
	else
	{
		if (*previousState == tcb)
		{
			*previousState = tcb->next;
		}
		tcb->prev->next = tcb->next;
		tcb->next->prev = tcb->prev;
	}
	if (*nextState == NULL)
	{	// there are no tcbs in that state
		*nextState = tcb;
		tcb->next = tcb;
		tcb->prev = tcb;
	}
	else
	{	// just append it for now
		tcb->next = *nextState;
		tcb->prev = (*nextState)->prev;
		(*nextState)->prev->next = tcb;
		(*nextState)->prev = tcb;
	}
	tcb->state = nextState;
}

/*
 * Performance Measurements
 */ 
#define JITTER_SIZE 64
int32_t MaxJitter = 0;
int32_t MaxJitter2 = 0;
const uint32_t JitterSize = JITTER_SIZE;
const uint32_t JitterSize2 = JITTER_SIZE;
uint32_t JitterHistogram[JITTER_SIZE] = {0,};
uint32_t JitterHistogram2[JITTER_SIZE] = {0,};
int32_t LastTime = 0;
int32_t LastTime2 = 0;

// wraps a periodic task in a jitter measurement routine and stores data in the histogram
// assumes that lastTime, maxJitter, and jitterHistogram have been initialized
void jitterWrapper(void (*task)(void), uint32_t period, int32_t *lastTime, int32_t *maxJitter, 
				   const uint32_t jitterSize, uint32_t *jitterHistogram) 
{
	uint32_t jitter, thisTime;
	task();	// run the periodic task
	thisTime = OS_Time();
	if (*lastTime == 0)
	{	// first time
		*lastTime = thisTime;
		return;
	}
	uint32_t diff = OS_TimeDifference(*lastTime,thisTime);
 	if(diff>period) {
 		jitter = (diff-period+4)/8; // in 0.1 usec
 	} else {
 		jitter = (period-diff+4)/8; // in 0.1 usec
 	}
	if(jitter > *maxJitter)
	{
        *maxJitter = jitter;
	}
	if(jitter >= jitterSize)
	{
        jitter = jitterSize-1;
	}
	jitterHistogram[jitter]++;
	*lastTime = thisTime;
}


/*
 * Sheduler Function
 * Determines the next function to be run
 * Do this in the context switch
 */
TCB *NextPt;
void Scheduler(void)
{
    uint32_t status = StartCritical();
    TCB *priorityArray[] = {Pri0, Pri1, Pri2, Pri3, Pri4, Pri5}; // Array of priority pointers

    for (int i = 0; i < 6; i++)
    {
        if (priorityArray[i] != NULL)
        {
            if (*(RunPt->state) == priorityArray[i])
            {
                NextPt = RunPt->next;
            }
            else
            {
                NextPt = priorityArray[i];
            }
            break; // Found the next task, break out of the loop
        }
    }
    EndCritical(status);
    ContextSwitch();
}

/*------------------------------------------------------------------------------
  Systick Interrupt Handler
  SysTick interrupt happens every 10 ms
  used for preemptive thread switch
 *------------------------------------------------------------------------------*/
void SysTick_Handler(void)
{
	// DisableInterrupts();
	uint32_t status = StartCritical();
	if (numSleep > 0) 	// Process any sleeping threads
	{
		TCB * currSleepPt = SleepList;
		for (size_t i = 0; i < numSleep; i++)
		{
			currSleepPt->sleepTime--;
			if (currSleepPt->sleepTime == 0)
			{
				TCB * wakingThread = currSleepPt;
				currSleepPt = currSleepPt->prev;
				updateState(wakingThread, &SleepList, GetPriPointer(wakingThread->priority));
				numSleep--;
				numActive++;
			}
			currSleepPt = currSleepPt->next;
		}
	}

	if (DEBUG)
		PF1 ^= 0x02;
	if (DEBUG)
		PF1 ^= 0x02;
	Scheduler();
	if (DEBUG)
		PF1 ^= 0x02;
	// EnableInterrupts();
	EndCritical(status);
}

unsigned long OS_LockScheduler(void)
{
	// lab 4 might need this for disk formating
	return 0; // replace with solution
}
void OS_UnLockScheduler(unsigned long previous)
{
	// lab 4 might need this for disk formating
}

#define NVIC_ST_CTRL_COUNT 0x00010000	// Count flag
#define NVIC_ST_CTRL_CLK_SRC 0x00000004 // Clock Source
#define NVIC_ST_CTRL_INTEN 0x00000002	// Interrupt enable
#define NVIC_ST_CTRL_ENABLE 0x00000001	// Counter mode
#define NVIC_ST_RELOAD_M 0x00FFFFFF		// Counter load value

// Initialize SysTick with busy wait running at bus clock.
// void SysTick_Init(unsigned long period){
//  NVIC_ST_CTRL_R = 0;                   // disable SysTick during setup
//  NVIC_ST_RELOAD_R = period-1;  // maximum reload value
//  NVIC_ST_CURRENT_R = 0;                // any write to current clears it
//                                        // enable SysTick with core clock
//  NVIC_ST_CTRL_R = NVIC_ST_CTRL_ENABLE+NVIC_ST_CTRL_CLK_SRC;
//}
uint32_t SystickPeriod;
void SysTick_Init(uint32_t period)
{
	long sr;
	SystickPeriod = period;
	sr = StartCritical();
	NVIC_ST_CTRL_R = 0;											   // disable SysTick during setup
	NVIC_ST_RELOAD_R = period - 1;								   // reload value
	NVIC_ST_CURRENT_R = 0;										   // any write to current clears it
	NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R & 0x00FFFFFF) | 0xE0000000; // priority 7
																   // enable SysTick with core clock and interrupts
	NVIC_ST_CTRL_R = 0x07;
	EndCritical(sr);
}

// calibrate cpu utilization for this system by incrementing a counter
volatile uint32_t IdleCountRef;
volatile uint8_t IdleTime;
volatile uint32_t CPUUtil;
void waitTenms()
{
	IdleTime = 1;
}
void initCPUUtilization()
{
	IdleCountRef = 0;
	IdleTime = 0;
	Timer5A_Init(&waitTenms, 10*TIME_1MS, 0);
	EnableInterrupts();
	while (IdleTime == 0)
	{
		IdleCountRef++;
	}
	DisableInterrupts();
	Timer5A_Stop();
}

/**
 * @details  Initialize operating system, disable interrupts until OS_Launch.
 * Initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers.
 * Interrupts not yet enabled.
 * @param  none
 * @return none
 * @brief  Initialize OS
 */
void OS_Init(void)
{
	DisableInterrupts();
	PLL_Init(Bus80MHz);
	LaunchPad_Init();
	ST7735_InitR(INITR_REDTAB); // LCD initialization
	UART_Init();
	initCPUUtilization();
	OS_MsTimerInit();
	initTCBs();
	Heap_Init(1);
};

// ******** OS_InitSemaphore ************
// initialize semaphore
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, int32_t value)
{
	// DisableInterrupts();
	uint32_t status = StartCritical();
	semaPt->Value = value;
	semaPt->BlockList = NULL;
	// EnableInterrupts();
	EndCritical(status);

};

static inline uint32_t IsInISR(void) {
    uint32_t ipsr;
    __asm volatile ("MRS %0, ipsr" : "=r" (ipsr)); // This syntax is for ARMCC6 (Clang/LLVM)
    return (ipsr & 0xFF);
}





// ******** OS_Wait ************
// decrement semaphore
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt)
{
	// Wait(semaPt);
	uint32_t status = StartCritical();
	semaPt->Value--;
	if (semaPt->Value < 0)
	{	// block
		updateState(RunPt, GetPriPointer(RunPt->priority), &(semaPt->BlockList));
		EndCritical(status);
		OS_Suspend();
	}
	else
	{
		EndCritical(status);
	}
}

// ******** OS_Signal ************
// increment semaphore
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt)
{
    uint32_t status = StartCritical();
    semaPt->Value++;
    if (semaPt->Value <= 0) 
    {   // Need to wake up a thread
        TCB* highestPriorityThread = NULL;
        uint32_t highestPriority = UINT32_MAX; // Initialize with the maximum possible priority value

        TCB* currentThread = semaPt->BlockList;
        if(currentThread != NULL) {
            do {
                if (currentThread->priority < highestPriority) {
                    highestPriority = currentThread->priority;
                    highestPriorityThread = currentThread;
                }
                currentThread = currentThread->next;
            } while(currentThread != semaPt->BlockList); // Loop until we've checked the entire list

            // Wake up the highest priority thread
            if(highestPriorityThread != NULL) {
                updateState(highestPriorityThread, &(semaPt->BlockList), GetPriPointer(highestPriorityThread->priority));
                
                // Trigger a context switch if the awoken task is higher priority than the current task
                // and we're not in an ISR
								uint32_t inISR = IsInISR();
                if(highestPriorityThread->priority > RunPt->priority && !IsInISR()){
                    Scheduler();
                }
            }
        }
    }
    EndCritical(status);
}

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt)
{
	uint32_t status = StartCritical();
	if (semaPt->Value <= 0)
	{	// block
		updateState(RunPt, GetPriPointer(RunPt->priority), &(semaPt->BlockList));
		EndCritical(status);
		OS_Suspend();
	}
	else {
		semaPt->Value = 0;
		EndCritical(status);
	}

}

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt)
 {
    uint32_t status = StartCritical();
    if (semaPt->Value == 0)
    {   
        // Check if the block list is not empty
        if (semaPt->BlockList != NULL){
            TCB *current = semaPt->BlockList;
            TCB *highestPriorityNode = current;
            uint32_t highestPriority = current->priority;

            // Traverse the circular doubly linked list to find the node with the highest priority
            do {
                if (current->priority < highestPriority) {
                    highestPriority = current->priority;
                    highestPriorityNode = current;
                }
                current = current->next;
            } while(current != semaPt->BlockList);

            // Update state of the node with the highest priority
            updateState(highestPriorityNode, &(semaPt->BlockList), GetPriPointer(highestPriorityNode->priority));

            // Conditionally call Scheduler() based on priority and ISR context
            if(highestPriorityNode->priority > RunPt->priority && !IsInISR()){
                Scheduler();
            }
        }
        else 
        {
            semaPt->Value = 1; // If block list is empty, simply set semaphore value to 1
        }
    }
    EndCritical(status);
}


//******** OS_AddThread ***************
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields

int OS_AddThread(void (*task)(void),
				 uint32_t stackSize, uint32_t priority)
{
	if (numThreads >= MAX_THREADS)
	{
		return 0;
	}
	// DisableInterrupts();
	uint32_t status = StartCritical();

	TCB * newThread;
	size_t thread_index;
	for (thread_index = 0; thread_index < MAX_THREADS; thread_index++)
	{
		if(TCBs[thread_index].state == NULL)
		{
			break;
		}
	}
	if (thread_index == MAX_THREADS)
	{	// This should never happen
		EndCritical(status);
		return 0;
	}
	newThread = &TCBs[thread_index];
	if (RunPt == NULL) {
		RunPt = newThread;
	}
	// initialize the thread structure
	newThread->task = task;
	newThread->ID = thread_index+1;	// per OS spec, ID must be greater than 0
	newThread->sleepTime = 0;
	// set the initial stack
	Stacks[thread_index][STACK_SIZE - 1] = 0x01000000;   	// xPSR  thumb bthread_indext
	Stacks[thread_index][STACK_SIZE - 2] = (uint32_t) task; // R15   PC
	Stacks[thread_index][STACK_SIZE - 3] = (uint32_t) OS_Kill;   	// R14   LR
	Stacks[thread_index][STACK_SIZE - 4] = 0x12121212;   // R12
	Stacks[thread_index][STACK_SIZE - 5] = 0x03030303;   // R3
	Stacks[thread_index][STACK_SIZE - 6] = 0x02020202;   // R2
	Stacks[thread_index][STACK_SIZE - 7] = 0x01010101;   // R1
	Stacks[thread_index][STACK_SIZE - 8] = 0x00000000;   // R0
	Stacks[thread_index][STACK_SIZE - 9] = 0x11111111;   // R11
	Stacks[thread_index][STACK_SIZE - 10] = 0x10101010;  // R10
	Stacks[thread_index][STACK_SIZE - 11] = 0x09090909;  // R9
	Stacks[thread_index][STACK_SIZE - 12] = 0x08080808;  // R8
	Stacks[thread_index][STACK_SIZE - 13] = 0x07070707;  // R7
	Stacks[thread_index][STACK_SIZE - 14] = 0x06060606;  // R6
	Stacks[thread_index][STACK_SIZE - 15] = 0x05050505;  // R5
	Stacks[thread_index][STACK_SIZE - 16] = 0x04040404;  // R4
	newThread->stackPointer = &Stacks[thread_index][STACK_SIZE-16];
	newThread->priority = priority;
	appendToList(newThread, GetPriPointer(priority));
	uint32_t inISR = IsInISR();
	if(RunPt->parent){
		newThread->parent = RunPt->parent;
		newThread->parent->ThreadCount += 1;
		Stacks[thread_index][STACK_SIZE - 11] = (uint32_t)(newThread->parent->dataPointer);  // R9
	}



	numActive++;
	numThreads++;
	EndCritical(status);
	return 1;
};




//******** OS_AddThread ***************
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields

int OS_AddThreadProcess(void (*task)(void),
				 uint32_t stackSize, uint32_t priority, PCB* parent)
{
	if (numThreads >= MAX_THREADS)
	{
		return 0;
	}
	// DisableInterrupts();
	uint32_t status = StartCritical();

	TCB * newThread;
	size_t thread_index;
	for (thread_index = 0; thread_index < MAX_THREADS; thread_index++)
	{
		if(TCBs[thread_index].state == NULL)
		{
			break;
		}
	}
	if (thread_index == MAX_THREADS)
	{	// This should never happen
		EndCritical(status);
		return 0;
	}
	newThread = &TCBs[thread_index];
	if (RunPt == NULL) {
		RunPt = newThread;
	}
	// initialize the thread structure
	newThread->task = task;
	newThread->ID = thread_index+1;	// per OS spec, ID must be greater than 0
	newThread->sleepTime = 0;
	// set the initial stack
	Stacks[thread_index][STACK_SIZE - 1] = 0x01000000;   	// xPSR  thumb bthread_indext
	Stacks[thread_index][STACK_SIZE - 2] = (uint32_t) task; // R15   PC
	Stacks[thread_index][STACK_SIZE - 3] = (uint32_t) OS_Kill;   	// R14   LR
	Stacks[thread_index][STACK_SIZE - 4] = 0x12121212;   // R12
	Stacks[thread_index][STACK_SIZE - 5] = 0x03030303;   // R3
	Stacks[thread_index][STACK_SIZE - 6] = 0x02020202;   // R2
	Stacks[thread_index][STACK_SIZE - 7] = 0x01010101;   // R1
	Stacks[thread_index][STACK_SIZE - 8] = 0x00000000;   // R0
	Stacks[thread_index][STACK_SIZE - 9] = 0x11111111;   // R11
	Stacks[thread_index][STACK_SIZE - 10] = 0x10101010;  // R10
	Stacks[thread_index][STACK_SIZE - 11] = (uint32_t)(parent->dataPointer);  // R9
	Stacks[thread_index][STACK_SIZE - 12] = 0x08080808;  // R8
	Stacks[thread_index][STACK_SIZE - 13] = 0x07070707;  // R7
	Stacks[thread_index][STACK_SIZE - 14] = 0x06060606;  // R6
	Stacks[thread_index][STACK_SIZE - 15] = 0x05050505;  // R5
	Stacks[thread_index][STACK_SIZE - 16] = 0x04040404;  // R4
	newThread->stackPointer = &Stacks[thread_index][STACK_SIZE-16];
	newThread->priority = priority;
	appendToList(newThread, GetPriPointer(priority));
	uint32_t inISR = IsInISR();
	
	newThread->parent = parent;
	newThread->parent->ThreadCount += 1;
	



	numActive++;
	numThreads++;
	EndCritical(status);
	return 1;
};

//******** OS_AddProcess ***************
// add a process with foregound thread to the scheduler
// Inputs: pointer to a void/void entry point
//         pointer to process text (code) segment
//         pointer to process data segment
//         number of bytes allocated for its stack
//         priority (0 is highest)
// Outputs: 1 if successful, 0 if this process can not be added
// This function will be needed for Lab 5
// In Labs 2-4, this function can be ignored
int OS_AddProcess(void (*entry)(void), void *text, void *data,
				  unsigned long stackSize, unsigned long priority)
{
	// put Lab 5 solution here
	if (numProcesses >= MAX_PCBS)
	{
		return 0;
	}
	// DisableInterrupts();
	uint32_t status = StartCritical();

	PCB * newPCB;
	size_t process_index;
	for (process_index = 0; process_index < MAX_PCBS; process_index++)
	{
		if(PCBs[process_index].ThreadCount == 0)
		{
			break;
		}
	}
	newPCB = &PCBs[process_index];
	newPCB->codePointer = text;
	newPCB->dataPointer = data;
	newPCB->PID = process_index;
	OS_AddThreadProcess(entry, stackSize, priority,newPCB);
	EndCritical(status);
	Scheduler();
	
	return 1; // replace this line with Lab 5 solution
}

//******** OS_Id ***************
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero
uint32_t OS_Id(void)
{
	if (RunPt == NULL) {
		return 0;
	}
	return RunPt->ID;
};

//******** OS_AddPeriodicThread ***************
// add a background periodic task
// typically this function receives the highest priority
// Inputs: pointer to a void/void background function
//         period given in system time units (12.5ns)
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// You are free to select the time resolution for this function
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In lab 1, this command will be called 1 time
// In lab 2, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, this command will be called 0 1 or 2 times
// In lab 3, there will be up to four background threads, and this priority field
//           determines the relative priority of these four threads
// Update Jitter:
// Takes a pointer to a Jitter Histogram 
// Assumes that the provided jitter value is in microseconds (us)
// Updates max jitter and the jitter historgram
void (*PeriodicTask1_Task)(void);
void (*PeriodicTask2_Task)(void);
uint32_t PeriodicTask1_Period, PeriodicTask2_Period;
uint32_t PeriodicTask1_Priority, PeriodicTask2_Priority;

void wrappedPeriodicTask1(void) {
	jitterWrapper(PeriodicTask1_Task, PeriodicTask1_Period,
				  &LastTime, &MaxJitter, JitterSize, JitterHistogram);
}
void wrappedPeriodicTask2(void) {
	jitterWrapper(PeriodicTask2_Task, PeriodicTask2_Period,
				  &LastTime2, &MaxJitter2, JitterSize2, JitterHistogram2);
}

int OS_AddPeriodicThread(void (*task)(void),
						 uint32_t period, uint32_t priority)
{
	// Static, stays defined. Tracks periodic thread count.
	static int numPeriodicThreads = 0; 
	if (numPeriodicThreads == 0)
	{ // Use timer 0A
		PeriodicTask1_Task = task;
		PeriodicTask1_Period = period;
		PeriodicTask1_Priority = priority;
		Timer1A_Init(&wrappedPeriodicTask1, period, priority);
		numPeriodicThreads++;
		return 1;
	}
	if(numPeriodicThreads == 1)
	{
		PeriodicTask2_Task = task;
		PeriodicTask2_Period = period;
		PeriodicTask2_Priority = priority;
		Timer2A_Init(&wrappedPeriodicTask2,period,priority);
		numPeriodicThreads++;
		return 1;
	 }
	return 0;
};

void (*SW1Task)(void);
void (*SW2Task)(void);
/*----------------------------------------------------------------------------
  PF1 Interrupt Handler
 *----------------------------------------------------------------------------*/
void GPIOPortF_Handler(void){
  if(GPIO_PORTF_RIS_R & 0x10){ // Check if interrupt occurred on PF4
    GPIO_PORTF_ICR_R = 0x10;      // Acknowledge flag4
    (*SW1Task)();                 // Execute SW1Task
  }
  if(GPIO_PORTF_RIS_R & 0x01){ // Check if interrupt occurred on PF0
    GPIO_PORTF_ICR_R = 0x01;      // Acknowledge flag0
    (*SW2Task)();                 // Execute SW2Task
  }
}

//******** OS_AddSW1Task ***************
// add a background task to run whenever the SW1 (PF4) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In labs 2 and 3, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, there will be up to four background threads, and this priority field
//           determines the relative priority of these four threads
int OS_AddSW1Task(void (*task)(void), uint32_t priority)
{
	// put Lab 2 (and beyond) solution here
	SW1Task = task;
	SYSCTL_RCGCGPIO_R |= 0x00000020;  // (a) activate clock for port F
	GPIO_PORTF_LOCK_R = 0x4C4F434B;	  // 2) unlock GPIO Port F
	GPIO_PORTF_CR_R = 0x1F;			  // allow changes to PF4-0
	GPIO_PORTF_DIR_R |= 0x0E;		  // output on PF3,2,1
	GPIO_PORTF_DIR_R &= ~0x11;		  // (c) make PF4,0 in (built-in button)
	GPIO_PORTF_AFSEL_R &= ~0x1F;	  //     disable alt funct on PF4,0
	GPIO_PORTF_DEN_R |= 0x1F;		  //     enable digital I/O on PF4
	GPIO_PORTF_PCTL_R &= ~0x000FFFFF; // configure PF4 as GPIO
	GPIO_PORTF_AMSEL_R = 0;			  //     disable analog functionality on PF
	GPIO_PORTF_PUR_R |= 0x11;		  //     enable weak pull-up on PF4
	GPIO_PORTF_IS_R &= ~0x10;		  // (d) PF4 is edge-sensitive
	GPIO_PORTF_IBE_R &= ~0x10;		  //     PF4 is not both edges
	GPIO_PORTF_IEV_R &= ~0x10;		  //     PF4 falling edge event
	GPIO_PORTF_ICR_R = 0x10;		  // (e) clear flag4
	GPIO_PORTF_IM_R |= 0x10;		  // (f) arm interrupt on PF4 *** No IME bit as mentioned in Book ***
	NVIC_PRI7_R = (NVIC_PRI7_R & 0xFF00FFFF) | (priority << 21);
	NVIC_EN0_R = 0x40000000; // (h) enable interrupt 30 in NVIC
	return 1;				 // replace this line with solution
};

//******** OS_AddSW2Task ***************
// add a background task to run whenever the SW2 (PF0) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is highest, 5 is lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed user task will run to completion and return
// This task can not spin block loop sleep or kill
// This task can call issue OS_Signal, it can call OS_AddThread
// This task does not have a Thread ID
// In lab 2, this function can be ignored
// In lab 3, this command will be called will be called 0 or 1 times
// In lab 3, there will be up to four background threads, and this priority field
//           determines the relative priority of these four threads
int OS_AddSW2Task(void(*task)(void), uint32_t priority){
  SW2Task = task; // Assign the task to the function pointer

  // Similar setup as for PF4, but for PF0
  SYSCTL_RCGCGPIO_R |= 0x00000020; // Activate clock for port F
  GPIO_PORTF_LOCK_R = 0x4C4F434B;   // Unlock GPIO Port F
  GPIO_PORTF_CR_R |= 0x01;           // Allow changes to PF0
  GPIO_PORTF_DIR_R |=  0x0E;    // output on PF3,2,1 
  GPIO_PORTF_DIR_R &= ~0x11;    // Make PF0 an input (clear PF0 bit)
  GPIO_PORTF_AFSEL_R &= ~0x01;  // Disable alt funct on PF0
  GPIO_PORTF_DEN_R |= 0x01;     // Enable digital I/O on PF0   
  GPIO_PORTF_PCTL_R &= ~0x0000000F; // Configure PF0 as GPIO
  GPIO_PORTF_AMSEL_R &= ~0x01;       // Disable analog functionality on PF0
  GPIO_PORTF_PUR_R |= 0x01;     // Enable weak pull-up on PF0
  GPIO_PORTF_IS_R &= ~0x01;     // PF0 is edge-sensitive
  GPIO_PORTF_IBE_R &= ~0x01;    // PF0 is not both edges
  GPIO_PORTF_IEV_R &= ~0x01;    // PF0 falling edge event
  GPIO_PORTF_ICR_R = 0x01;      // Clear flag0
  GPIO_PORTF_IM_R |= 0x01;      // Arm interrupt on PF0

  // Set priority of PF0 interrupt (use same NVIC_PRI7_R as PF4 but for the relevant bits for PF0 interrupt)
  NVIC_PRI7_R = (NVIC_PRI7_R & 0xFFFFFF00) | (priority << 5); // PF0 is IRQ 30, priority set in the lower bits of NVIC_PRI7_R

  NVIC_EN0_R = 0x40000000;      // Enable interrupt 30 in NVIC (same as PF4, since it's the same port)

  return 1; // Success
};

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(uint32_t sleepTime)
{
	// DisableInterrupts();
	uint32_t status = StartCritical();
	updateState(RunPt, GetPriPointer(RunPt->priority), &SleepList);
	numActive--;
	numSleep++;
	RunPt->sleepTime = (sleepTime *TIME_1MS)/timeslice;
	// EnableInterrupts();
	EndCritical(status);
	OS_Suspend();
};

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void)
{
	// DisableInterrupts();
	uint32_t status = StartCritical();
	if(RunPt->parent){
		RunPt->parent->ThreadCount -= 1;
		if(RunPt->parent->ThreadCount == 0){
			Heap_Free(RunPt->parent->codePointer);
			Heap_Free(RunPt->parent->dataPointer);
		}
	}
	removeFromList(RunPt, GetPriPointer(RunPt->priority));
	numActive--;
	numThreads--;
	// EnableInterrupts();
	EndCritical(status);
	OS_Suspend();
	//for (;;)
	//{
	//}; // can not return
};

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void)
{
	Scheduler();
};

// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
#define FIFOSIZE 32
uint32_t fifo[FIFOSIZE];
Sema4Type DataAvailable;
Sema4Type Mutex;
uint32_t fifohead, fifotail;

void OS_Fifo_Init(uint32_t size)
{
	DataAvailable.Value = 0;
	fifohead = 0;
	fifotail = 0;
};

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers
//  this function can not disable or enable interrupts
int OS_Fifo_Put(uint32_t data)
{
	if (DataAvailable.Value < FIFOSIZE)
	{ // Place item in fifo
		if (fifohead == FIFOSIZE)
		{
			fifohead = 0;
		}
		fifo[fifohead] = data;
		fifohead++;
		OS_Signal(&DataAvailable);
		return 1;
	}

	return 0; // replace this line with solution
};

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data
uint32_t OS_Fifo_Get(void)
{
	OS_Wait(&DataAvailable);
	// DisableInterrupts();
	uint32_t status = StartCritical();
	if (fifotail == FIFOSIZE)
	{
		fifotail = 0;
	}
	uint32_t val = fifo[fifotail];
	fifotail++;
	// EnableInterrupts();
	EndCritical(status);

	return val; // replace this line with solution
};

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty
//          zero or less than zero if a call to OS_Fifo_Get will spin or block
int32_t OS_Fifo_Size(void)
{
	// put Lab 2 (and beyond) solution here

	return 0; // replace this line with solution
};

// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
uint32_t mailbox;
Sema4Type BoxFree;
Sema4Type DataValid;
void OS_MailBox_Init(void)
{
	BoxFree.Value = 1;
	DataValid.Value = 0;
	mailbox = 0;
};

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received
void OS_MailBox_Send(uint32_t data)
{
	OS_bWait(&BoxFree);
	mailbox = data;
	OS_bSignal(&DataValid);
};

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty
uint32_t OS_MailBox_Recv(void)
{
	OS_bWait(&DataValid);
	uint32_t value = mailbox;
	OS_bSignal(&BoxFree);
	return value; // replace this line with solution
};

// ******** OS_Time ************
// return the system time
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as
//   this function and OS_TimeDifference have the same resolution and precision
uint32_t OS_Time(void)
{
	return NVIC_ST_CURRENT_R;
};

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as
//   this function and OS_Time have the same resolution and precision
uint32_t OS_TimeDifference(uint32_t start, uint32_t stop)
{
	if (start < stop)
	{
		return SystickPeriod - stop + start;
	}
	else
	{
		return start - stop;
	}
};

uint32_t msTime;
/*------------------------------------------------------------------------------
  Timer5A Interrupt Ms Counter
  Timer5A interrupt happens every 1 ms
  used for ms clocking to keep OS time
 *------------------------------------------------------------------------------*/
void OS_Timer5A_Counter(void)
{
	// PF2 ^= 0x04;
	// PF2 ^= 0x04;
	msTime = msTime + 1;
	// PF2 ^= 0x04;
} // end Timer5A_Handler

//------------OS Ms Timer Init---------------
// Initializes the OS Ms Timer using timer 5
// No inputs or outputs
//-------------------------------------------
void OS_MsTimerInit(void)
{
	Timer5A_Init(&OS_Timer5A_Counter, 80000, 2);
}

// ******** OS_ClearMsTime ************
// sets the system time to zero (solve for Lab 1), and resets timer 5 to maintain
// accurate time. Assumes timer5 was initialized elsewhere
// Inputs:  none
// Outputs: none
// You are free to change how this works

void OS_ClearMsTime(void)
{
	uint32_t period = 80000;	 // Period set to 1ms
	TIMER5_TAILR_R = period - 1; // 4) reload value to ensure timer is not off by a fraction of a ms
	msTime = 0;
};

// ******** OS_MsTime ************
// reads the current time in msec (solve for Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// For Labs 2 and beyond, it is ok to make the resolution to match the first call to OS_AddPeriodicThread
uint32_t OS_MsTime(void)
{
	return msTime; // replace this line with solution
};

//******** OS_Launch ***************
// start the scheduler, enable interrupts
// Inputs: number of 12.5ns clock cycles for each time slice
//         you may select the units of this parameter
// Outputs: none (does not return)
// In Lab 2, you can ignore the theTimeSlice field
// In Lab 3, you should implement the user-defined TimeSlice field
// It is ok to limit the range of theTimeSlice to match the 24-bit SysTick

void OS_Launch(uint32_t theTimeSlice)
{
	timeslice = theTimeSlice;
	SysTick_Init(theTimeSlice);
	EnableInterrupts();
	// StartOS(RunPt->stackPointer);
	StartOS();
};

//************** I/O Redirection ***************
// redirect terminal I/O to UART or file (Lab 4)

int StreamToDevice = 0; // 0=UART, 1=stream to file (Lab 4)

int fputc(int ch, FILE *f)
{
	if (StreamToDevice == 1)
	{ // Lab 4
		//if (eFile_Write(ch))
		//{							// close file on error
		//	OS_EndRedirectToFile(); // cannot write to file
		//	return 1;				// failure
		//}
		return 0; // success writing
	}

	// default UART output
	UART_OutChar(ch);
	return ch;
}

int fgetc(FILE *f)
{
	char ch = UART_InChar(); // receive from keyboard
	UART_OutChar(ch);		 // echo
	return ch;
}

//int OS_RedirectToFile(const char *name)
//{						// Lab 4
//	eFile_Create(name); // ignore error if file already exists
//	if (eFile_WOpen(name))
//		return 1; // cannot open file
//	StreamToDevice = 1;
//	return 0;
//}

//int OS_EndRedirectToFile(void)
//{ // Lab 4
//	StreamToDevice = 0;
//	if (eFile_WClose())
//		return 1; // cannot close file
//	return 0;
//}

int OS_RedirectToUART(void)
{
	StreamToDevice = 0;
	return 0;
}

int OS_RedirectToST7735(void)
{
	return 1;
}
