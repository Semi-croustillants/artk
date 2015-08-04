// ARTK  inline.h
// A pre-emptive multitasking kernel for Arduino
//
// History:
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

#ifndef INLINE_H
#define INLINE_H

#include <Arduino.h>

inline 
void ARTK_Sleep(unsigned int ticks)
{
	Scheduler::InstancePtr->activeTask->task_sleep(ticks) ;
}

inline 
void ARTK_Yield()
{
   Scheduler::InstancePtr->relinquish() ;
}

#endif


