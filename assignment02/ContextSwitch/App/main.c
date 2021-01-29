/*
 * University of Washington
 * Certificate in Embedded and Real-Time Systems
 *
 * Context Switch Homework
 */


#include "bsp.h"
#include "print.h"
#include "stack.h"

// uncomment TEST_AND_SET to use lock for print functions
#define TEST_AND_SET 0

char clr_scrn[] = { 27, 91, 50, 74, 0 };              // esc[2J
/* Public variables ----------------------------------------------------------*/
PRINT_DEFINEBUFFER();           /* required for lightweight sprintf */
/* Private prototype ---------------------------------------------------------*/
void delay(uint32_t time);
void get_lock_with_WFE(volatile unsigned int *Lock_Variable);
void free_lock(volatile unsigned int *Lock_Variable);


// Allocate space for two task stacks
uint32_t stackOne[STACKSIZE];
uint32_t stackTwo[STACKSIZE];

// Lock variable used to prevent overrun during task switching
volatile unsigned int g_lock = 0;

// To manage the contexts of two tasks we need to keep track of
// a separate stack for each task. The context of the task will 
// be stored in its stack.
//
// taskID is a global variable that holds the ID of the currently running task.
//
// currentSP is a global variable that holds the value of the
// stack pointer for the currently running task.

uint32_t stackOneSP;       // Value of task 1's stack pointer
uint32_t stackTwoSP;       // Value of task 2's stack pointer

uint32_t currentSP;        // The value of the current task's stack pointer
uint32_t taskID;           // The ID of the current task


//
// taskOne counts up from 0.
// Never exits.
//
void taskOne(void)
{
	int count = 0;
	while(1)
	{
#ifdef TEST_AND_SET
        // use lock to make sure only one task has the print function at a time
        get_lock_with_WFE(&g_lock);
#endif
        
		PrintString("task one: ");
		Print_uint32(count++);
		PrintString("\n");
#ifdef TEST_AND_SET
        free_lock(&g_lock);
#endif
		int i;
		for(i=0;i<100000;i++);    // delay
	}
}

//
// taskTwo counts down from 0xFFFFFFFF
// Never exits.
//
void taskTwo(void)
{
	int count = 0xFFFFFFFF;
	while(1)
	{
#ifdef TEST_AND_SET
        // use lock to make sure only one task has the print function at a time
        get_lock_with_WFE(&g_lock);
#endif        
		PrintString("task two: ");
		Print_uint32(count--);
		PrintString("\n");
        
#ifdef TEST_AND_SET
        free_lock(&g_lock);
#endif
		int i;
		for(i=0;i<100000;i++);    // delay
	}
}

/*
	The task scheduler decides which task to run next.

	It alternates back and forth between the two tasks by changing the
	value of currentSP used by TaskSwitch in switch.s

	TaskSwitch does the real context switching work.

    sp: the top-of-stack pointer of the current task
*/
void scheduler(uint32_t sp)
{
	//PrintString("\nSwitch\n");

	if (taskID == 1)
	{
		stackOneSP = sp;               // Store stack pointer for task 1
		taskID = 2;                    // Set the taskID to task 2
		currentSP = stackTwoSP;        // Set the current stack pointer to task 2's stack pointer
	}
	else
	{
		stackTwoSP = sp;               // Store stack pointer for task 2
		taskID = 1;                    // Set the taskID to task 1
		currentSP = stackOneSP;        // Set the current stack pointer to task 1's stack pointer
	}
}


// Function to gain a lock in MUTEX (mutual exclusive)/semaphore
void get_lock_with_WFE(volatile unsigned int *Lock_Variable)
{// Note: __LDREXW and __STREXW are functions in CMSIS-Core
    int status;
    do {
        while ( __LDREXW(Lock_Variable) != 0){ // Wait until lock
        __WFE();} // variable is free, if not, enter sleep until event
        status = __STREXW(1, Lock_Variable);   // Try set Lock_Variable
                                                // to 1 using STREX
    } while (status != 0); // retry until lock successfully
    __DMB(); // Data memory Barrier
    return;
}

void free_lock(volatile unsigned int *Lock_Variable)
{
    __DMB(); // Data memory Barrier
    *Lock_Variable = 0; // Free the lock
    __SEV(); // Send Event to wake-up other processors
    return;
}

void main() {
    Hw_init();
    
    PrintString(clr_scrn); /* Clear entire screen */
    PrintString("University of Washington - Context Switch Application \n");  
  
    // Setup initial stack for taskTwo
    stackTwoSP = initialize_stack(stackTwo, (void*)taskTwo);

    // Make Task One the current task and begin executing it
    taskID = 1;
    taskOne();  // Should never return
    
    while(1);  // Should not arrive here
    
}

 