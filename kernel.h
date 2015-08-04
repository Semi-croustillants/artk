// ARTK  kernel.h 
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
 

#ifndef KERNEL_H
#define KERNEL_H

#include <machine.h>
#include <stdlib.h>
#include <ARTK.h>

#define TIMER_USEC  10000      // 10 msec sleep timer 
#define SERIAL_BAUD 9600

#define TRUE  1 
#define FALSE 0 

// just won't work w/ less than MIN_STACK
#define MIN_STACK     256

// task states
#define TASK_READY         1    // Task is ready
#define TASK_ACTIVE        2    // Task is currently running
#define TASK_BLOCKED       3    // Task is blocked on a semaphore
#define SLEEP_BLOCKED      4    // Task is sleeping

// Task priorities
/*#define LOWEST_PRIORITY   0
#define HIGHEST_PRIORITY  1
#define PRIORITY_LEVELS   2*/

#define MAX_THREAD_LIST 5

// The scheduler maintains an array of circular lists - one for each priority.
// Each semaphore maintains a circular list.
// Note that this has no data member pointer.
// Rather, it should be integrated as a member into any class who's objects
// go onto a list.
// Thus, each Task contains a DLink member that maintains the task's position
// in either a scheduler or semaphore list (but not both).
class DNode
{
private:
	DNode	*pNext ;
	DNode	*pPrev ;

public:
	int isEmpty() { return (pNext == this) ; }

    // Insert passed node before this node
    // when called on a head node, this adds at the end of the circular list
    #define addLast insertBefore
	void insertBefore(DNode *link) ;

    // Remove the next object from the queue and return a pointer to it
    // when called on a head node this removes the item at the front of the queue
    #define removeFront removeNext
	DNode *removeNext() ;

    // Remove this object from whatever queue it is currently in
	void remove() ;

    // important that this be kept circular
	DNode() { pNext = pPrev = this ; }
	~DNode() {}
};

// Task (process descriptor) class
class Task
{
private:
    // This should probably be cleaned up
	//friend void  timerISR() ;
	friend class Scheduler ;

    // This links the task into a doubly-linked list
	DNode mylink ;

	//uint8_t priority ;  // Task priority
	uint8_t state ;     // Task state
	uint8_t firstRun ;  // True if task has never been swapped in

    // During a context switch, the SP is saved here
	unsigned char *pStack ;

    void (*rootFn)() ;



public:
    // Task's stack	and root function pointer
    unsigned char stack[MIN_STACK];

private:
    // This method is executed when a task returns from it's root function
	static void taskDone();

public:
	unsigned char inUse;

    // These change the task state.
	void makeTaskReady() { state = TASK_READY ; }
	void makeTaskActive() { state = TASK_ACTIVE ; }
	void makeTaskBlocked(){ state = TASK_BLOCKED ; }
	void makeTaskSleepBlocked(){ state = SLEEP_BLOCKED ; }
	void setFunction(void (*rootFnPtr)()) { rootFn = rootFnPtr; }
	//void setPriority(uint8_t priority) { this->priority = priority; }
	void PushScheduler();

	uint8_t myState() { return state; }

    // called by the user's sleep() wrapper function.
	void task_sleep(unsigned time);

    // creates a new task
	//Task(void (*rootFnPtr)(), uint8_t priority, unsigned stackSize) ;

	Task();
	   
    // destructor cleans up the stack space
    // the new and delete operators are broken for arrays in AVR
	~Task() { }
} ;


////TEST
class TaskManager
{
private:
	Task listTask[MAX_THREAD_LIST];
	TaskManager() {};
public:
	static TaskManager *instPtr;
	static void Instance();
	Task *getFreeTask();
	void releaseTask(Task *addr);
};

class DQNode
{
public:
	Task *pTask ;
	DQNode *pNext ;
	long dcount ;
	unsigned long start;
	char inUse = false;
};

class DQNodeManager
{
private:
	// List of DQNode (static allocation memory)
	DQNode DQList[MAX_THREAD_LIST];
	// Constructor of DQNodeManager
	DQNodeManager() {};

public:
	static DQNodeManager *instPtr;
	static void Instance();
	DQNode *getFreeDQNode();
	void releaseDQNode(DQNode *addr);
};

//  Semaphore class
/*class Semaphore
{
private:
	int	    count ;
	DNode   taskList ;
	
public:
	//void	wait() ;
	int 	wait(unsigned int timeout) ;
	void 	signal() ;

    // allocates a new semaphore object with a default initial count of 0
	Semaphore(int initialCount = 0) ;
	~Semaphore() {}
} ;*/

// this class implements the ARTK task scheduler
class Scheduler
{
private:
    // For each priority level, the scheduler maintains a queue 
    // of process descriptors for that are ready to run.
	DNode readyList; //[PRIORITY_LEVELS] ;

    // Total number of tasks, including the Main task
	int numTasks ;

public:
    // Pointer to the single instance of scheduler
    static Scheduler *InstancePtr ;

    // points to the active task
	Task *activeTask ;

    // called to start the scheduler
	void startMultiTasking() ;

    // returns the stack space left for the active task
    //unsigned int stackLeft() ;

    // add/remove tasks on the ready lists
	void addready(Task *t) { readyList.addLast(&t->mylink) ; }
	void removeready(Task *t) { t->mylink.remove() ; }
	char addNewTask(Task *t) ;
	void removeTask() ;
	char timerISR();

    // called by the active task when it is willing to yield
	void relinquish() ;

    // reschedules the processor to next highest priority task
	void resched();

    // creates the single allowed instance (private constructor below)
	static void Instance() ;

private:
    Scheduler() ;

public:
	~Scheduler() {}
} ;

// ARTK_Xxx functions that are inlined are here
#include <inline.h>

#endif  // KERNEL_H
