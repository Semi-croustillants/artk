// ARTK.h - defines the public interface for user programs
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
 
#ifndef ARTK_H
#define ARTK_H

#include <kernel.h>

// This library won't work on with task stacks less than about 210,
// so the library won't let you set a stack size less than that
// This is the default size - only a bit larger than the min
#if defined(__AVR_ATmega328P__)
	#define DEFAULT_STACK 128
#elif defined (__AVR_ATmega1280__)
	#define DEFAULT_STACK 256
#elif defined (__AVR_ATmega2560__)
	#define DEFAULT_STACK 384
#else
	#define DEFAULT_STACK 128
#endif

class Task ;
typedef Task* TASK ;

// IMPORTANT: Call this from Setup() only, and only if you don't like a default
// For each option, -1 says to use the default
// iLargeModel:  1 if you have more than 64k memory (e.g., Mega)
//               0 otherwise (e.g., UNO)
//               Defaults to 0
void ARTK_SetOptions(int iLargeModel) ;

// Task functions
// Valid user task priority is 1 to 16 (1 being lowest)
// In general tasks are created from Setup, but it is safe to create a 
// from any task.  The new task can't swap in until the creating task does 
// something that allows a context switch, such as signaling a semaphore 
// (or allowing an ISR to signal a semaphore), or waiting on a semaphore, 
// or sleeping.  Of course, once a higher priority task starts up, it can 
// take the processor anytime it is ready to do so.
TASK ARTK_CreateTask(void (*root_fn_ptr)(), unsigned stacksize = DEFAULT_STACK) ;

// Sleep for so many ticks.  See ARTK_SetOptions above for the tick interval.
// inlined 
void ARTK_Sleep(unsigned ticks) ;

// ARTK is preemptive but does not timeshare automatically between tasks of 
// equal priority.  Don't create tasks of equal priority unless you don't 
// care about their relative scheduling.  If you create tasks of equal 
// priority, make sure they yield somewhere in order to allow other
// tasks of the same priority to run.  They can yield by sleeping, by waiting
// on a semaphore, by signaling a semaphore (directly or via an ISR), by
// exiting, or explicitly yielding:
// inlined 
void ARTK_Yield() ;

// ARTK will terminate when all tasks return (including Main), or you can 
// terminate early by calling this
void ARTK_TerminateMultitasking() ;

#endif
