/************************************************************************************

Copyright (c) 2001-2021  University of Washington Extension.  All rights reserved.

Module Name:

    main.c

Module Description:

    Test application for UCOS-II port.
	Ported from the UCOS test app.

Revision History:
Adapted for Cortex-M4 1/1/2016

************************************************************************************/


#include "bsp.h"
#include "print.h"
 

// Prototype for startup task
void StartupTask(void* pdata);

// Allocate a stack for the startup task
static OS_STK StartupStk[APP_CFG_TASK_START_STK_SIZE];

char clr_scrn[] = { 27, 91, 50, 74, 0 };              // esc[2J
// Allocate the print buffer
PRINT_DEFINEBUFFER();

int ricktest_int2 = 0xA0A0A0A0;
int ricktest_int  = 0x00EFBEEF;
uint8_t rick3 = 130;
uint8_t rick4 = 120;
/************************************************************************************

Routine Description:

    Standard program main entry point.

Arguments:

    None.

Return Value:

    none.

************************************************************************************/
void main() {
    
    int a = 4;
    
    INT8U err;
    Hw_init();
    
    PrintString(clr_scrn); /* Clear entire screen */
    
    RETAILMSG(1, ("uCOS Demo: Built %s %s.\r\n\r\n",
        __DATE__,
        __TIME__));  


    // Initialize the OS
    DEBUGMSG(1, ("main: Running OSInit()...\n"));

    OSInit();

    // Create the startup task
    DEBUGMSG(1, ("main: Creating start up task.\n"));

    err = OSTaskCreate(
        StartupTask,
        (void*)0,
        &StartupStk[APP_CFG_TASK_START_STK_SIZE-1],
        APP_TASK_START_PRIO);

    if (err != OS_ERR_NONE) {
        DEBUGMSG(1, ("main: failed creating start up task: %d\n", err));
        while(OS_TRUE);  //park on error
    }

    if (a == 4) {
        a = 0;
    } else {
        a = 1;
    }
    
    DEBUGMSG(1, ("Starting multi-tasking.\n"));

    // start the OS
    OSStart();

    // should never reach here
    while (1);
}


  