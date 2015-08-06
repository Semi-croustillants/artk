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

// Modifications by SYLVESTRE François


// Usage Notes:
// ARTK makes use of the Arduino TimerOne library, which is distributed with it.
// You must NOT implement a setup() function
// Implement a Setup() function instead - ARTK will call it for you
// You must NOT implement a loop() function
// Implement a Main() function instead, which will be the lowest priority task
// See the file ARTKtest.ino for example usage 
#include  <Arduino.h>    // for millis()
#include  <kernel.h>

// -----------------------------------------------------------------
// globals
int glargeModel = FALSE ;
unsigned char *glastSP = 0 ;
Scheduler *Scheduler::InstancePtr = 0 ;
DQNodeManager *DQNodeManager::instPtr = 0;
TaskManager *TaskManager::instPtr = 0;

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

DQNode DQNodeManager::DQList[MAX_THREAD_LIST];

void DQNodeManager::Instance() {
	if (instPtr == NULL) {
		instPtr = new DQNodeManager;
	}
}

DQNode *DQNodeManager::getFreeDQNode() {
	unsigned char i;
	for (i = 0; i < MAX_THREAD_LIST; i++) {
		if (!DQList[i].inUse) {
			DQList[i].inUse = !DQList[i].inUse;
			DQList[i].start = millis();
			return &DQList[i];
		}
	}
	return NULL;
}

void DQNodeManager::releaseDQNode(DQNode *addr) {
	unsigned char i;
	for (i = 0; i < MAX_THREAD_LIST; i++) {
		if (&DQList[i] == addr) {
			DQList[i].inUse = FALSE;
			DQList[i].pNext = 0;
			DQList[i].pTask = 0;
			DQList[i].dcount = 0;
		}
	}
}

DQNode *pSleepHead = NULL ;

// add a task to sleep q in sorted position
void addSleeper(Task *pTask, unsigned int count)
{

   DQNode *pNew = DQNodeManager::instPtr->getFreeDQNode();
   DQNode *pCurrent = NULL ;
   DQNode *pOneBack = NULL ;

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
         pOneBack = pCurrent ;
         pCurrent = pCurrent->pNext ;
      }
      // now insert the new item 
      // if our new count is the smallest in the list, put it at the head
      if (pOneBack == NULL)   
      {
         // decrement the current head count by the new count
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
   if ( (pSleepHead != NULL) && (pSleepHead->dcount <= 0) )
   {
      pTemp = pSleepHead ;
      pSleepHead = pTemp->pNext ;
	  pTask = pTemp->pTask ;
	  DQNodeManager::instPtr->releaseDQNode(pTemp);
   }
   return pTask ;
}

// Decrements the counter of the first node in the sleep queue
void sleepDecrement()
{
   unsigned long current;
   DQNode *tmp = pSleepHead;
   if (pSleepHead != NULL) {
	   current = millis();
	   pSleepHead->dcount-= current-pSleepHead->start ;
	   pSleepHead->start = current;
	   while (tmp->pNext != NULL) {
		   tmp = tmp->pNext;
		   tmp->dcount -= current-tmp->start ;
		   tmp->start = current;
	   }
   }
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
         DQNodeManager::instPtr->releaseDQNode(pCurrent);
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
}

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

    // remove highest priority task from readyList
	if (!readyList.isEmpty())
    {
		newTask = (Task *)readyList.removeFront() ;
	}
	else {
		while (!this->timerISR());
		newTask = (Task *)readyList.removeFront() ;
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
	
	// swap the new task in
	// if it is the first run, then use processor state from current task
	// otherwise get processor state from the stack of its previous swap out
	// if the oldTask is NULL then this is the first time we've ever done
	// a task switch, and we don't try to save the context (IOW, the stack
	// state of main() is abandoned on the first task switch)
	int firstRun = activeTask->parameter.firstRun ;
	activeTask->parameter.firstRun = FALSE ;
	if (oldTask != NULL) {
		ContextSwitch(&oldTask->pStack, activeTask->pStack, firstRun) ;
	}

    else {
 	   FirstSwitch(activeTask->pStack) ;
    }
}

//  Called by a task when it is ready to yield
void Scheduler::relinquish()
{
	timerISR();
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
}

Task::Task() {
	parameter.firstRun = TRUE ;
	parameter.inUse = FALSE;
	pStack = &stack[MIN_STACK-1] ;
}

//  When the root function for a task returns, it executes
//  this function.
void Task::taskDone()
{
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

void Task::PushScheduler() {
	// next put the entry function on the stack so we return to it after
	// returning from a context switch
	*pStack-- = (unsigned char)((long)rootFn & 0x00ff) ;
	*pStack-- = (unsigned char)(((long)rootFn >> 8) & 0x00ff) ;
	if (glargeModel)
	   *pStack-- = (unsigned char)(((long)rootFn >> 16) & 0x00ff) ;
	Scheduler::InstancePtr->addNewTask(this) ;
}

Task TaskManager::listTask[MAX_THREAD_LIST];

void TaskManager::Instance() {
	if (instPtr == NULL) {
		instPtr = new TaskManager;
	}
}

Task* TaskManager::getFreeTask() {
	unsigned char i;
	for (i = 0; i < MAX_THREAD_LIST; i++) {
		if (!listTask[i].parameter.inUse) {
			listTask[i].parameter.inUse = !listTask[i].parameter.inUse;
			return &listTask[i];
		}
	}
	return NULL;
}

void TaskManager::releaseTask(Task *addr) {
	unsigned char i;
	for (i = 0; i < MAX_THREAD_LIST; i++) {
		if (&listTask[i] == addr) {
			listTask[i].parameter.inUse = FALSE;
		}
	}
}

char Scheduler::timerISR()
{
	char taskReady = FALSE;
	Task *pWakeup ;
	// Check for waiting tasks that have timed out and 
    // sleeping tasks that must be woken
	// decrement the count of the head of the sleepq
	sleepDecrement() ;
	
	// get all those off the sleep q that are at 0
	pWakeup = removeWaker() ;
	while (pWakeup != NULL)
	{
        // either way (semaphore or just sleeping), it goes to ready list
        pWakeup->makeTaskReady() ;
		Scheduler::InstancePtr->addready(pWakeup) ;

        // see if anymore are at 0
		pWakeup = removeWaker() ;
		taskReady = TRUE;
	}
	return taskReady;
}

//--------------------------------------------------------------------------
// User-accessible constructs

Task *ARTK_CreateTask(void (*rootFnPtr)(), unsigned stacksize)
{
   Task *task = TaskManager::instPtr->getFreeTask();
   task->setFunction(rootFnPtr);
   task->PushScheduler();
   return task ;
}

void ARTK_TerminateMultitasking()
{
   exit(0) ;
}

void ARTK_SetOptions(int iLargeModel)
{
   if (iLargeModel == -1)
      glargeModel = FALSE ;
   else
      glargeModel = iLargeModel ;
}

//-------------------------------------------------------------------------
// Main and Idle tasks, startup functions

extern void SetupARTK() ;

void setup()
{
   glargeModel = FALSE ;
   Scheduler::Instance();
   DQNodeManager::Instance();
   TaskManager::Instance();

   SetupARTK() ;

   Scheduler::InstancePtr->startMultiTasking() ;
}

void loop() 
{ }
