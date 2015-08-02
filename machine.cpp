// ARTK  machine.cpp 
// A pre-emptive multitasking kernel for Arduino
//
// History:
// Release 0.1  June 2012  Paul H. Schimpf
// Release 0.2  June 2012  Paul H. Schimpf
//              Some changes to squeeze out bits of memory
//
// Acknowledgement:
// Thank you to Raymond J. A. Buhr and Donald L. Bailey for inspiration
// and ideas from "An Introduction to Real-Time Systems."  While there are 
// significant differences, there are also similarities in the structure 
// and implementation of ARTK and Tempo.

/******* License ***********************************************************
  This file is part of ARTK - Arduino Real-Time Kernel

  ARTK is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  ARTK is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with ARTK.  If not, see <http://www.gnu.org/licenses/>.
****************************************************************************/

// Modifications by SYLVESTRE François


// Usage Notes:
// ARTK makes use of the Arduino TimerOne library, which is distributed with it.
// You must NOT implement a setup() function
// Implement a Setup() function instead - ARTK will call it for you
// You must NOT implement a loop() function
// Implement a Main() function instead, which will be the lowest priority task
// See the file ARTKtest.ino for example usage 
 
// ARDUINO (AVR) Machine Code
// AVR stack grows downward in memory
// SP points to next free location (post decr on push, pre incr on pop)
// Low byte of ret addr goes on first (at the higher addr)
//
#include  <Arduino.h> 
#include "machine.h"

// perform a context switch
// firstRun is true when the incoming task is being run for the first time
void ContextSwitch(unsigned char **fromSP, unsigned char *toSP, int firstRun)
{
   // push registers to exiting process stack
   asm volatile (
     "push r0            \n\t"
     "in   r0, __SREG__  \n\t"
     "cli                \n\t"
     "push r0            \n\t"
     "push r1            \n\t"
     "clr  r1            \n\t"
     "push r2            \n\t"
     "push r3            \n\t"
     "push r4            \n\t"
     "push r5            \n\t"
     "push r6            \n\t"
     "push r7            \n\t"
     "push r8            \n\t"
     "push r9            \n\t"
     "push r10           \n\t"
     "push r11           \n\t"
     "push r12           \n\t"
     "push r13           \n\t"
     "push r14           \n\t"
     "push r15           \n\t"
     "push r16           \n\t"
     "push r17           \n\t"
     "push r18           \n\t"
     "push r19           \n\t"
     "push r20           \n\t"
     "push r21           \n\t"
     "push r22           \n\t"
     "push r23           \n\t"
     "push r24           \n\t"
     "push r25           \n\t"
     "push r26           \n\t"
     "push r27           \n\t"
     "push r28           \n\t"
     "push r29           \n\t"
     "push r30           \n\t"
     "push r31           \n\t"
   ) ;
     
   // update the exiting process SP
   *fromSP = (unsigned char *)(SP) ;
      
   // pop registers from starting process stack
   SP = (unsigned int)toSP ;
   if (firstRun)
   asm volatile (
     "sei                \n\t"
     "ret                \n\t"
   ) ;
   else
   asm volatile (
     "pop  r31           \n\t"
     "pop  r30           \n\t"
     "pop  r29           \n\t"
     "pop  r28           \n\t"
     "pop  r27           \n\t"
     "pop  r26           \n\t"
     "pop  r25           \n\t"
     "pop  r24           \n\t"
     "pop  r23           \n\t"
     "pop  r22           \n\t"
     "pop  r21           \n\t"
     "pop  r20           \n\t"
     "pop  r10           \n\t"
     "pop  r18           \n\t"
     "pop  r17           \n\t"
     "pop  r16           \n\t"
     "pop  r15           \n\t"
     "pop  r14           \n\t"
     "pop  r13           \n\t"
     "pop  r12           \n\t"
     "pop  r11           \n\t"
     "pop  r10           \n\t"
     "pop  r9            \n\t"
     "pop  r8            \n\t"
     "pop  r7            \n\t"
     "pop  r6            \n\t"
     "pop  r5            \n\t"
     "pop  r4            \n\t"
     "pop  r3            \n\t"
     "pop  r2            \n\t"
     "pop  r1            \n\t"
     "clr  r1            \n\n"
     "pop  r0            \n\t"
     "out  __SREG__, r0  \n\t"
     "pop  r0            \n\t"
     "sei                \n\t"
   ) ;
}

// perform a context switch to the very first task (Main task)
// a bit of a hack in that the context of main() is simply left stranded
void FirstSwitch(unsigned char *toSP)
{
   // pop registers from starting process stack
   SP = (unsigned int)toSP ;
   asm volatile (
     "sei                \n\t"
     "ret                \n\t"
   ) ;
}
