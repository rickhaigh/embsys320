#include <stdint.h>
#include "print.h"

/*
 *
 * Part of a fault exception handler. Prints the given register values.
 * pc: the value of the program counter when the fault occurred.
 * lr: the value of the link register when the fault occurred.
 *
 * Example output: "Hard fault at PC=0x1234ABCD LR=0xABCD1234"
 */
void FaultPrint(uint32_t pc, uint32_t lr)
//void FaultPrint(uint32_t reg_ptr[8])
{
    PrintString("Hard fault at PC=");
    PrintHex(pc);
    PrintString(" LR=");
    PrintHex(lr);
    PrintString("\n");
//    uint8_t i;
//    char reg_names[8][4] = {"R0", "R1", "R2", "R3", "R12", "LR", "PC", "PSR"};
//    
//    PrintString("\n");
//    PrintString("Hard fault register dump");
//    PrintString("\n");
//    
//    for (i=0 ;i < 8;i++) {
//        PrintString(reg_names[i]);
//        PrintString("\t");
//        PrintHex(reg_ptr[i]);
//        PrintString("\n");
//    }
//    PrintString("\n");
}
