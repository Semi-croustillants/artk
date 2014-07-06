// ARTK  kernel.cpp 
// A pre-emptive multitasking kernel for Arduino
//
// History:
// Release 0.1  June 2012  Paul H. Schimpf
// Release 0.2  June 2012  Paul H. Schimpf
//              Some changes to squeeze out bits of memory
// Release 0.3  Moved cli() to top of sema wait routine, as a 
//              higher priority waker could preempt while count
//              is being checked
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
 
#include  <stdarg.h>     // for va_xxx
#include  <stdio.h>      // for vsnprintf
#include  <Arduino.h>    // for Serial
#include  <TimerOne.h>
#include  <kernel.h>

// -----------------------------------------------------------------
// globals
const char *release = "0.3" ;
const int year = 2012 ;
int glargeModel = FALSE ;
int gtimerUsec = TIMER_USEC ;
unsigned char *glastSP = 0 ;
Scheduler *Scheduler::InstancePtr = 0 ;

//----------------------------------------------------------------
// Doubly-linked list manipulation

// also known as addLast
void DNode::insertBefore(DNode *pLink)
{
	pLink->pNext = this ;
	pLink->pPrev = pPrev ;
	pPrev->pNext = pLink ;
	pPrev = pLink ;
}

// also known as removeFront
DNode *DNode::removeNext()
{
	DNode   *pLink ;

	if (isEmpty()) return NULL ;
	pLink = pNext ;
	pNext = pLink->pNext ;
	pLink->pNext->pPrev = this ; 
	// self reference of both is important for remove() to be safe
    pLink->pPrev = pLink ;
	pLink->pNext = pLink ;
	return (pLink) ;
}

void DNode::remove()
{
	pPrev->pNext = pNext ;
	pNext->pPrev = pPrev ;
    pPrev = this ;
	pNext = this ;
}

//--------------------------------------------------------------------
// Sleep Queue
// The Sleep Queue is sorted singly linked list of DQNode (Delta Queue Node)
// These are sorted in increasing order and keep track of the tick counts remaining
// The counts remaining for a particular entry is the sum off all dcounts
// up to and including that entry

struct DQNode
{
	Task *pTask ;
	DQNode *pNext ;
	unsigned int dcount ;
} ;

DQNode *pSleepHead = NULL ;

// add a task to sleep q in sorted position
void addSleeper(Task *pTask, unsigned int count)
{
   DQNode *pNew ;
   DQNode *pCurrent ;
   DQNode *pOneBack ;

   pNew = new DQNode ;
   pNew->pTask = pTask ;
   pNew->pNext = NULL ;
   pNew->dcount = count ;

   if (pSleepHead == NULL) 
   {
      pSleepHead = pNew ;
   } 
   else  
   {
      // find the position in increasing order
      // at the same time, update the dcount of the new item by subtracting
      // the count of all items that remain in front of it
      pCurrent = pSleepHead ;
      pOneBack = NULL ;
      while ( (pCurrent != NULL) && (pCurrent->dcount < pNew->dcount) ) 
      {
	     pNew->dcount -= pCurrent->dcount ;
         pOneBack = pCurrent ;
         pCurrent = pCurrent->pNext ;
      }
      // now insert the new item 
      // if our new count is the smallest in the list, put it at the head
      if (pOneBack == NULL)   
      {
         // decrement the current head count by the new count
         pSleepHead->dcount -= pNew->dcount ;
		 pSleepHead = pNew ;
		 pNew->pNext = pCurrent ;
      } 
      // else if our new count is the largest, put it at the tail
      else if (pCurrent == NULL) 
      {
	     pOneBack->pNext = pNew ;
	  } 
	  // else we're going in the middle somewhere
      else 
      {
         // decrement the follower count by the new count
         pCurrent->dcount -= pNew->dcount ;
         pOneBack->pNext = pNew ;
         pNew->pNext = pCurrent ;
	  }
   }
}

// If the count of the first task on the sleep queue is 0 then remove it
Task *removeWaker()
{
   Task *pTask ;
   DQNode *pTemp ;

   pTask = NULL ;
   if ( (pSleepHead != NULL) && (pSleepHead->dcount == 0) )
   {
      pTemp = pSleepHead ;
      pSleepHead = pTemp->pNext ;
	  pTask = pTemp->pTask ;
      delete pTemp ;
   }
   return pTask ;
}

// Decrements the counter of the first node in the sleep queue
void sleepDecrement()
{
   if (pSleepHead != NULL) 
      pSleepHead->dcount-- ;
}

// search for a task and remove it from the sleep queue
void removeSleeper(Task *pTask)
{
   int    done ;
   DQNode *pOneBack ;
   DQNode *pNext ;
   DQNode *pCurrent ;

   pCurrent = pSleepHead ;
   pOneBack = NULL ;
   done = (pCurrent == NULL) ;
   while (!done) 
   {
      // if we found the task
      if (pCurrent->pTask == pTask) 
      {
         done = TRUE ;
         // if found was first entry, adjust head pointer
         if (pOneBack == NULL) 
            pSleepHead = pCurrent->pNext ;
         // else adjust the one position back next pointer
         else
		    pOneBack->pNext = pCurrent->pNext ;
         
         // adjust the delta of the following entry up
         pNext = pCurrent->pNext ;
         if (pNext != NULL) 
            pNext->dcount += pCurrent->dcount ;

         delete pCurrent ;
      } 
      else 
      {
         pOneBack = pCurrent ;
         pCurrent = pCurrent->pNext ;
         done = (pCurrent == NULL) ;
      }
	}
}

//-------------------------------------------------------------
// Scheduler
//
Scheduler::Scheduler()
{
	numTasks = 0 ;
	activeTask = NULL ;

    Printf("ARTK release %s\n", release) ;
    Printf("Paul Schimpf, %d, GNU GPL\n", year) ;
}

/****
// got rid of this extra layer by making stack member public (so shoot me)
int Scheduler::stackLeft()
{
    return ((unsigned char *)(SP)-activeTask->stack) ;
}
****/

// called when a new task is created
char Scheduler::addNewTask(Task *t)
{
	numTasks++ ;
	t->makeTaskReady() ;
	addready(t) ;
	return(TRUE) ;
}

// reduce the count of active tasks by one
void Scheduler::removeTask()
{
	numTasks-- ;
	if (numTasks == 1) // all but idle have terminated
		ARTK_TerminateMultitasking() ;
	else 
       resched() ;
}

//  Selects the next task and performs a context switch
void Scheduler::resched()
{
	Task   *oldTask ;
	Task   *newTask ;
	int    priority ;

    // remove highest priority task from readyList
	for (priority=HIGHEST_PRIORITY; priority >= LOWEST_PRIORITY; priority--) 
    {
		if (!readyList[priority].isEmpty()) 
        {
			newTask = (Task *)readyList[priority].removeFront() ;
			break ;
		}
	}

	// If calling task is still the highest priority just return
	if (newTask == activeTask) 
    {
		activeTask->makeTaskActive() ;
		return ;
	}

	oldTask = activeTask ;
	activeTask = newTask ;
	activeTask->makeTaskActive() ;
	
	// a context switch is necessary - clear interrupts while we do this
	// interrupts are reenabled when the new task is swapped in
    cli() ;
    	
	/***
	sei() ;
	Printf("about to context switch\n") ;
	if (oldTask != NULL)
	Printf("from root ptr=%x, SP=%x \n", oldTask->rootFn, oldTask->pStack) ;
	Printf("to root ptr=%x, SP=%x \n", newTask->rootFn, newTask->pStack) ;
	cli() ;
	***/
	
	// swap the new task in
	// if it is the first run, then use processor state from current task
	// otherwise get processor state from the stack of its previous swap out
	// if the oldTask is NULL then this is the first time we've ever done
	// a task switch, and we don't try to save the context (IOW, the stack
	// state of main() is abandoned on the first task switch)
	int firstRun = activeTask->firstRun ;
	activeTask->firstRun = FALSE ;
	if (oldTask != NULL)
	   ContextSwitch(&oldTask->pStack, activeTask->pStack, firstRun) ;
    else
 	   FirstSwitch(activeTask->pStack) ;
}

//  Called by a task when it is ready to yield
void Scheduler::relinquish()
{
	activeTask->makeTaskReady() ;
	addready(activeTask) ;
	resched() ;
}

// Creates an instance of the scheduler only if none exists
void Scheduler::Instance()
{
    if (InstancePtr == 0)
       InstancePtr = new Scheduler() ;
}

void Scheduler::startMultiTasking()
{
    // get Idle and Main tasks going
    resched() ;   
    // Printf("at end of startMultiTasking\n") ;
}

// Constructor for class Task
Task::Task(void (*rootFnPtr)(), uint8_t taskPriority, unsigned stackSize) : mylink()
{
   rootFn = rootFnPtr ;
   // because the new operator for arrays is broken in AVR
   stack = (unsigned char *)malloc(stackSize) ;
   if (stack==NULL)
   {
      Printf("Insufficient Mem to Create Task\n") ;
      return ;
   }
   
   //Printf("stack at %d\n", stack) ;
    
   //stack = (unsigned char *) new char[stackSize] ;
   //char *goo = new char[10] ;
   //stack = (unsigned char *) goo ;
   firstRun = TRUE ;
    
   pStack = &stack[stackSize-1] ;
	
   // Initialize the stack so that the root function
   // for this task returns to taskDone().
   *pStack-- = (unsigned char)((long)Task::taskDone & 0x00ff) ;
   *pStack-- = (unsigned char)(((long)Task::taskDone >> 8) & 0x00ff) ; 
   if (glargeModel)
      *pStack-- = (unsigned char)(((long)Task::taskDone >> 16) & 0x00ff) ; 
	
   // next put the entry function on the stack so we return to it after
   // returning from a context switch
   *pStack-- = (unsigned char)((long)rootFnPtr & 0x00ff) ;
   *pStack-- = (unsigned char)(((long)rootFnPtr >> 8) & 0x00ff) ;
   if (glargeModel)
      *pStack-- = (unsigned char)(((long)rootFnPtr >> 16) & 0x00ff) ; 
    
   // what goes next on the stack are 32 registers and SREG = 33
   // for (int i=0 ; i <33 ; i++) *pStack-- = 0 ;

   priority = taskPriority ;

   // Ask the scheduler to add this task to the ready list
   Scheduler::InstancePtr->addNewTask(this) ;

   /****
   Printf("Initialized Process Descriptor Table for:\n") ;
   Printf("SP=%x, IP=%x\n", pStack, rootFnPtr) ;
   //Printf("Stack Dump:\n") ;
   //unsigned char *temp = &stack[stackSize-1] ;
   //while (temp>=pStack) Printf("%hhx ", *temp--) ;
   //Printf("\n") ;
   ****/
}

//  When the root function for a task returns, it executes
//  this function.
void Task::taskDone()
{
   // Printf("In taskDone\n") ;
   // added this call
   Scheduler::InstancePtr->removeready(Scheduler::InstancePtr->activeTask) ;
   Scheduler::InstancePtr->removeTask();
}

// the calling task is put to sleep for cnt ticks of the system timer
void Task::task_sleep(unsigned int cnt)
{
	if (cnt > 0) 
    {
		makeTaskSleepBlocked() ;
		addSleeper(this, cnt) ;
		Scheduler::InstancePtr->resched() ;
	}
}

void timerISR()
{
	Task *pWakeup ;
	int contextSwitchNeeded = FALSE ;

    // interrupts will be disabled on the way in
    
	// Check for waiting tasks that have timed out and 
    // sleeping tasks that must be woken
	Task *active = Scheduler::InstancePtr->activeTask ;
	
	// decrement the count of the head of the sleepq
	sleepDecrement() ;
	
	// get all those off the sleep q that are at 0
	pWakeup = removeWaker() ;
	while (pWakeup != NULL) 
    {
		// A task that blocked on a semaphore has timed out 
        // Remove it from the semaphore list and flag the semaphore timeout
		if (pWakeup->myState() == SEM_TIMED_BLOCKED) 
		{
		    pWakeup->timedOut = TRUE ;
			pWakeup->mylink.remove() ;
        }
        
        // either way (semaphore or just sleeping), it goes to ready list
        pWakeup->makeTaskReady() ;
		Scheduler::InstancePtr->addready(pWakeup) ;
		if (pWakeup->priority > active->priority)
			contextSwitchNeeded = TRUE ;

        // see if anymore are at 0
		pWakeup = removeWaker() ;
	}
	if (contextSwitchNeeded) 
    {
		active->makeTaskReady() ;
		Scheduler::InstancePtr->addready(active) ;
		Scheduler::InstancePtr->resched() ;
	}
	
	// interrupts will be reenabled on the way out
}

//--------------------------------------------------------------------------
//  Semaphore Class
Semaphore::Semaphore(int initialCount) : taskList()
{
   count = initialCount ;
}

void Semaphore::wait()
{
    cli() ;    // no preempt while check and possibly modify count
    // if available, give it and return
	if (count > 0) 
    {
		count-- ;
		sei() ;
	} 
    else 
    {
		// block the caller
		Task* active = Scheduler::InstancePtr->activeTask ;
		active->makeTaskBlocked() ;
		// move the active task to this semaphore queue
		taskList.addLast(&active->mylink) ;
		Scheduler::InstancePtr->resched() ;  // this call reenables
	}
}

int Semaphore::wait(unsigned int timeout)
{
    // set the task status to not timed out (yet)
    Task *active = Scheduler::InstancePtr->activeTask ;
	active->timedOut = FALSE ;

    cli() ;    // no preempt while check and possibly modify count    
	// if available, give it and return
	if (count>0) 
    {
	   count-- ;
	   return ACQUIRED_SEMA ;
	} 
	
	// if specified a 0 wait can return now
    if (timeout == 0) {
       sei() ;
       return TIMED_OUT ;
	}
	
    // otherwise, this task needs to sleep until either timeout
    // or this semaphore is signaled

    // cli() ;    // to keep sleepq and sematimedblocked consistent
	// add the calling task to the sleep queue
	addSleeper(active, timeout) ;
	
	// move the calling task to the semaphore queue and context switch
	active->makeTaskSemaphoreTimedBlocked() ;
	taskList.addLast(&active->mylink) ;
    Scheduler::InstancePtr->resched() ;   // this call will reenable

    // the calling task returns here after being swapped back in
    // it is now the active task again, so check to see if its timed out flag
    // was set by the timerISR at some point
    if (active->timedOut) 
       return TIMED_OUT ;
       
	return ACQUIRED_SEMA ;
}

void Semaphore::signal()
{
	Task    *t ;

    // an ISR can call this, so make sure we don't allow that to happen
    // while a task is inside
	cli() ;
	count++ ;   // increment the semaphore count
	
    // Remove the task at the front of this semaphore queue
	if (!taskList.isEmpty())
    {
        count-- ;   
		t  = (Task *)taskList.removeFront() ;
		
		// It may have been flagged with a timeout and not swapped in yet,
		// so in that event we override the timedout flag so it knows it
		// now has the semaphore when it swaps back in
		t->timedOut = FALSE ;

		// If the task is still in a timed wait remove it from the sleep q
		if (t->myState() == SEM_TIMED_BLOCKED) removeSleeper(t) ;
		
		// the task is now ready to run
		t->makeTaskReady() ;
		Scheduler::InstancePtr->addready(t) ;
		
		// if the waiting task is higher priority, reschedule
		if (t->priority > Scheduler::InstancePtr->activeTask->priority) 
        {
			Scheduler::InstancePtr->activeTask->makeTaskReady() ;
			Scheduler::InstancePtr->addready(Scheduler::InstancePtr->activeTask) ;
			Scheduler::InstancePtr->resched() ;
		}
	}
	sei() ;     // can allow an ISR in now
}

//--------------------------------------------------------------------------
// User-accessible constructs

Semaphore *ARTK_mutex ;   // used by the CS macro

// extern void Idle() ;
void Idle()
{
   while (1) ;
}

Task *ARTK_CreateTask(void (*rootFnPtr)(), uint8_t priority, unsigned stacksize)
{
   Task *t ;
   if (priority<1) priority = 1 ;
   if (priority>=PRIORITY_LEVELS) priority = PRIORITY_LEVELS-1 ;
   if (rootFnPtr==Idle) priority = 0 ;
   if (stacksize<MIN_STACK) stacksize = MIN_STACK ;
   t = new Task(rootFnPtr, priority, stacksize) ;
   return t ;
}

void ARTK_TerminateMultitasking()
{
   Printf("All tasks done, exiting\n") ; 
   // stop timer isr
   Timer1.detachInterrupt() ;
   exit(0) ;
}

Semaphore *ARTK_CreateSema(int initial_count)
{
	Semaphore *s;
	s = new Semaphore(initial_count);
	return s;
}

void ARTK_SetOptions(int iLargeModel, int iTimerUsec)
{
   if (iLargeModel == -1)
      //Scheduler::largeModel = FALSE ;
      glargeModel = FALSE ;
   else
      //Scheduler::largeModel = iLargeModel ;
      glargeModel = iLargeModel ;
      
   if (iTimerUsec == -1) 
      //Scheduler::timerUsec = TIMER_USEC ;
      gtimerUsec = TIMER_USEC ;
   else
   {
      if (iTimerUsec < 1000)
         printf("WARNING: do you really need sleep res < 1 msec?\n") ;
      //Scheduler::timerUsec = iTimerUsec ;
      gtimerUsec = iTimerUsec ;
   }
}

// like printf - to the Arduino Serial Monitor
void Printf(char *fmt, ... ) 
{
   char tmp[128]; // resulting string limited to 128 chars
   va_list args;
   va_start (args, fmt );
   vsnprintf(tmp, 128, fmt, args);
   va_end (args);
   Serial.print(tmp);
   Serial.flush() ;
}

//-------------------------------------------------------------------------
// Main and Idle tasks, startup functions 

extern void Setup() ;

void setup()
{
   //Scheduler::InstancePtr = NULL ;
   //Scheduler::timerUsec = TIMER_USEC ;
   //Scheduler::largeModel = FALSE ;
   gtimerUsec = TIMER_USEC ;
   glargeModel = FALSE ;

   // init the serial channel 
   Serial.begin(9600) ;
   
   // Printf("setup creating scheduler and CS semaphore\n") ; 
   Scheduler::Instance();
   ARTK_mutex = ARTK_CreateSema(1);

   // init the sleep timer
   Timer1.initialize(gtimerUsec) ;
   Timer1.attachInterrupt(timerISR) ;

   // disable interrupts in case user is installing any?
   // cli() ;
   Setup() ;
   // sei() ;
   
   // need an idle task in case all others are sleeping or exited
   // Printf("Creating Idle task\n", Idle) ;
   // Setup() must be called before doing this in case the memory model
   // is changed there 
   ARTK_CreateTask(Idle, 0, IDLE_STACK) ;

   Printf("Start Tasking\n") ; 
   // store the SP for free memory estimation (task stacks are separate
   // from the main stack, which is abandoned at this point)
   glastSP = (unsigned char *)(SP) ;
   Scheduler::InstancePtr->startMultiTasking() ;
}

void loop() 
{
   Printf("Something is wrong\n") ;
}
