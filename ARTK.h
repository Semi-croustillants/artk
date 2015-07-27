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

// Usage Notes:
// ARTK makes use of the Arduino TimerOne library, which is distributed with it.
// You must NOT implement a setup() function
// Implement a Setup() function instead - ARTK will call it for you
// You must NOT implement a loop() function
// Implement a Main() function instead, which will be the lowest priority task
// See the file ARTKtest.ino for example usage 
 
#ifndef ARTK_H
#define ARTK_H
#include <stdint.h>
#include <kernel.h>

// This library won't work on with task stacks less than about 210,
// so the library won't let you set a stack size less than that
// This is the default size - only a bit larger than the min
#define DEFAULT_STACK 256

//class Semaphore ;
class Task ;
//typedef Semaphore* SEMAPHORE ;
typedef Task* TASK ;

// utility to printf to serial port
void Printf(char *fmt, ... )  ;

// IMPORTANT: Call this from Setup() only, and only if you don't like a default
// For each option, -1 says to use the default
// iLargeModel:  1 if you have more than 64k memory (e.g., Mega)
//               0 otherwise (e.g., UNO)
//               Defaults to 0
// iTimerUsec:   Number of microseconds for each timer tick.  You'll get a
//               warning if this is less than 1000
//               Defaults to 10000 (10 msec)
void ARTK_SetOptions(int iLargeModel, int iTimerUsec) ;

// a Task can call this to determine how much local stack space it has left
// if it gets below about 203, things will probably break as the preemptive
// kernel needs about that much space on any task to get business done
// inline 
//int ARTK_StackLeft() ;

// this is a conservative estimate - it does not traverse the heap freelist, so 
// it includes 16 bytes times the max number of simultaneous sleeps that have 
// ever occurred at the time it is called. If the number of tasks sleeping is 
// less than that max then there will be some additional space left on the 
// freelist, but it is best to assume you will build to that number of sleepers
// again at some point
//inline 
//int ARTK_EstAvailRam() ;

// All code enclosed by the following Critical Section macro are protected by 
// a single a global mutex.  Useful, for example, to prevent a waking Task of 
// higher priority from interspersing output on a shared resource (like the
// Serial link) with a lower-priority task that it preempts.
/*extern SEMAPHORE ARTK_mutex ;
#define CS(x) {ARTK_WaitSema(ARTK_mutex); x ARTK_SignalSema(ARTK_mutex);}*/

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

// Multiple tasks can use the same root function, in which case this function
// is handy for distinguishing between themselves at run-time:
// inlined 
TASK ARTK_MyId() ;

// Semaphore functions - for the last the timeout is in ticks
// The version with timeout returns -1 if it timed out, 0 if the semaphore
// was acquired
/*SEMAPHORE ARTK_CreateSema(int initial_count = 0) ;
// inlined 
void ARTK_WaitSema(SEMAPHORE semaphore) ;
// inlined 
void ARTK_SignalSema(SEMAPHORE semaphore) ;
// inlined 
int  ARTK_WaitSema(SEMAPHORE semaphore, unsigned timeout) ;*/

// ARTK will terminate when all tasks return (including Main), or you can 
// terminate early by calling this
void ARTK_TerminateMultitasking() ;

#endif
