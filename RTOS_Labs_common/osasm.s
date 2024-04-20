;/*****************************************************************************/
;/* OSasm.s: low-level OS commands, written in assembly                       */
;/* derived from uCOS-II                                                      */
;/*****************************************************************************/
;Jonathan Valvano, OS Lab2/3/4/5, 1/12/20
;Students will implement these functions as part of EE445M/EE380L.12 Lab

        AREA |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8

        EXTERN  RunPt           ; currently running thread
        EXTERN  NextPt          ; Next thread, determined by scheduler
		EXTERN  savedSP
        EXPORT  StartOS
        EXPORT  ContextSwitch
        EXPORT  PendSV_Handler
        EXPORT  SVC_Handler
        EXPORT  Wait
        EXPORT  Signal
        EXPORT  bWait
        EXPORT  bSignal


NVIC_INT_CTRL   EQU     0xE000ED04                              ; Interrupt control state register.
NVIC_SYSPRI14   EQU     0xE000ED22                              ; PendSV priority register (position 14).
NVIC_SYSPRI15   EQU     0xE000ED23                              ; Systick priority register (position 15).
NVIC_LEVEL14    EQU           0xEF                              ; Systick priority value (second lowest).
NVIC_LEVEL15    EQU           0xFF                              ; PendSV priority value (lowest).
NVIC_PENDSVSET  EQU     0x10000000                              ; Value to trigger PendSV exception.


StartOS
    LDR     R0, =RunPt      ; Pointer to RunPt
    LDR     R1, [R0]        ; Load RunPt to R1
    LDR     SP, [R1]        ; Set the stack pointer from RunPt
    POP     {R4-R11}        ; Pop R4-R11 from stack
    POP     {R0-R3}         ; Pop R0-R3 from stack
    POP     {R12}           ; Pop R12 from stack
    POP     {R1}            ; Discard LR from initial stack
    POP     {LR}            ; Start task
    POP     {R1}            ; Discard PSR
    LDR R0, =NVIC_SYSPRI15    ; Load address of SysTick priority register
	LDR R1, =NVIC_LEVEL15     ; Load desired priority level
	STRB R1, [R0]             ; Set the SysTick priority
	LDR R0, =NVIC_SYSPRI14    ; Load address of PendSV priority register
	LDR R1, =NVIC_LEVEL14     ; Load desired priority level
	STRB R1, [R0]             ; Set the PendSV priority
    CPSIE   I               ; Enable interrupts
    BX      LR

StartOSHang
    B       StartOSHang     ; Should never get here

StartOSCustom
; put your code here
    MOV		SP,R0
	LDR R0, =NVIC_SYSPRI15    ; Load address of SysTick priority register
	LDR R1, =NVIC_LEVEL15     ; Load desired priority level
	STRB R1, [R0]             ; Set the SysTick priority
	LDR R0, =NVIC_SYSPRI14    ; Load address of PendSV priority register
	LDR R1, =NVIC_LEVEL14     ; Load desired priority level
	STRB R1, [R0]             ; Set the PendSV priority
	POP		{R0-R12,LR}
    BX      LR                 ; start first thread

OSStartHang
    B       OSStartHang        ; Should never get here


;********************************************************************************************************
;                               PERFORM A CONTEXT SWITCH (From task level)
;                                           void ContextSwitch(void)
;
; Note(s) : 1) ContextSwitch() is called when OS wants to perform a task context switch.  This function
;              triggers the PendSV exception which is where the real work is done.
;********************************************************************************************************

ContextSwitch
; edit this code
    LDR R0, =NVIC_INT_CTRL     ; Load address of interrupt control state register
    LDR R1, =NVIC_PENDSVSET    ; Load PendSV set value
    STR R1, [R0]               ; Write to trigger PendSV
    BX      LR				   ; 
    

;********************************************************************************************************
;                                         HANDLE PendSV EXCEPTION
;                                     void OS_CPU_PendSVHandler(void)
;
; Note(s) : 1) PendSV is used to cause a context switch.  This is a recommended method for performing
;              context switches with Cortex-M.  This is because the Cortex-M3 auto-saves half of the
;              processor context on any exception, and restores same on return from exception.  So only
;              saving of R4-R11 is required and fixing up the stack pointers.  Using the PendSV exception
;              this way means that context saving and restoring is identical whether it is initiated from
;              a thread or occurs due to an interrupt or exception.
;
;           2) Pseudo-code is:
;              a) Get the process SP, if 0 then skip (goto d) the saving part (first context switch);
;              b) Save remaining regs r4-r11 on process stack;
;              c) Save the process SP in its TCB, OSTCBCur->OSTCBStkPtr = SP;
;              d) Call OSTaskSwHook();
;              e) Get current high priority, OSPrioCur = OSPrioHighRdy;
;              f) Get current ready thread TCB, OSTCBCur = OSTCBHighRdy;
;              g) Get new process SP from TCB, SP = OSTCBHighRdy->OSTCBStkPtr;
;              h) Restore R4-R11 from new process stack;
;              i) Perform exception return which will restore remaining context.
;
;           3) On entry into PendSV handler:
;              a) The following have been saved on the process stack (by processor):
;                 xPSR, PC, LR, R12, R0-R3
;              b) Processor mode is switched to Handler mode (from Thread mode)
;              c) Stack is Main stack (switched from Process stack)
;              d) OSTCBCur      points to the OS_TCB of the task to suspend
;                 OSTCBHighRdy  points to the OS_TCB of the task to resume
;
;           4) Since PendSV is set to lowest priority in the system (by OSStartHighRdy() above), we
;              know that it will only be run when no other exception or interrupt is active, and
;              therefore safe to assume that context being switched out was using the process stack (PSP).
;********************************************************************************************************

PendSV_Handler
; put your code here
	CPSID	I	    	 ; Disable interrupts
	PUSH	{R4-R11}	 ; Save remaining regs
	LDR		R0, =RunPt   ; Pointer to the run pointer store in R0
	LDR		R1, [R0] 	 ; Run pointer into R1
	STR		SP, [R1]	 ; Load the stack pointer to the address of the run pointer
;	LDR		R1, [R1,#4]  ; Load the next TCB to R1
    LDR     R1, =NextPt  ; Pointer to NextPt
    LDR     R1, [R1]     ; Address of the next tcb
	STR		R1,	[R0]	 ; Store the next TCB in the run pointer
	LDR		SP, [R1]	 ; Load the stack pointer from the new run TCB
	POP		{R4-R11}	 ; Restore Regs
	CPSIE I              ; Interrupts Enabled    
    BX      LR           ; Exception return will restore remaining context   
    

;********************************************************************************************************
;                                         HANDLE SVC EXCEPTION
;                                     void OS_CPU_SVCHandler(void)
;
; Note(s) : SVC is a software-triggered exception to make OS kernel calls from user land. 
;           The function ID to call is encoded in the instruction itself, the location of which can be
;           found relative to the return address saved on the stack on exception entry.
;           Function-call paramters in R0..R3 are also auto-saved on stack on exception entry.
;********************************************************************************************************

	IMPORT    OS_Id
	IMPORT    OS_Kill
	IMPORT    OS_Sleep
	IMPORT    OS_Time
	IMPORT    OS_AddThread


	; Define the branch table



SVC_Handler
; put your Lab 5 code here
	LDR R12,[SP,#24] ; Return address
	LDRH R12,[R12,#-2] ; SVC instruction is 2 bytes
	BIC R12,#0xFF00 ; Extract ID in R12
	LDM SP,{R0-R3} ; Get any parameters
	PUSH {LR}
	
	CMP R12, #0
	BLEQ OS_Id

	CMP R12, #1
	BLEQ OS_Kill

	CMP R12, #2
	BLEQ OS_Sleep
	
	CMP R12, #3
	BLEQ OS_Time
	
	CMP R12, #4
	BLEQ OS_AddThread
	
	POP {LR}
	STR R0,[SP] ; Store return value
    BX      LR                   ; Return from exception


;*********************************************************************************************************
; IPC Primitives
; Assembly implementation of the IPC primitives Wait, Signal, bWait and bSignal
; 
; 
;*********************************************************************************************************
; Wait for the semaphore to be nonzero, then decrement
; Repeat if the exclusive store fails
Wait
    LDREX   R1, [R0]      ; counter
    SUBS    R1, #1      ; --counter
    ITT     PL          ; ok if >= 0
    STREXPL R2,R1,[R0]  ; try update
    CMPPL   R2, #0      ; succeed?
    BNE     Wait        ; no, try again
    BX      LR          

; Increment the semaphore unconditionally
; Repeat if the exclusive store fails
Signal
    LDREX   R1, [R0]    ; counter
    ADD     R1, #1      ; ++counter
    STREX   R2,R1,[R0]  ; try update
    CMP     R2, #0      ; succed?
    BNE     Signal      ; no, try again
    BX      LR          

; Wait for the semaphore to be nonzero, then set it to 0
; Repeat if the exclusive store fails
bWait
    LDREX   R1, [R0]    ; counter
    CMP     R1, #0      ; sema==0?
    BEQ     bWait       ; no, try again
    MOV     R1, #0      ; counter=0
    STREX   R2,R1,[R0]  ; try update
    CMP     R2, #0      ; succeed?
    BNE     bWait       ; no, try again
    BX      LR

; Set the semaphore to 1, unconditionally
; Repeat if the exclusive store fails
bSignal
    LDREX   R1, [R0]    ; counter
    MOV     R1, #1      ; counter=1
    STREX   R2,R1,[R0]  ; try update
    CMP     R2, #0      ; succeed?
    BNE     bSignal     ; no, try again
    BX      LR

;*********************************************************************************************************

    ALIGN
    END
