// ARTK demo program for UNO
// Note: the UNO has only 2kbytes of SRAM, and the ARTK IDLE task and Printf routine
// currently eat up about 384 bytes.  More than a couple additional tasks will likely 
// lead to running out of SRAM.  This example stretches that to 3 more tasks with 
// some careful memory management, and just barely has enough SRAM.  Whe running on
// UNO it is best to keep your user tasks to 3 or less and keep string lengths to
// a minimum unless you move those to flash memory.  Things will start breaking once 
// ARTK_EstAvailRam() gets below 0. Things will also start breaking once ARTK_StackLeft() 
// gets below around 204.  Unless your tasking needs are very simple, a MEGA is
// recommended for use with ARTK.
//
// After uploading, open the Serial Monitor
//
// This demo demonstrates the following:
// (for a more extensive demo, see ArTKtestMEGA - but you'll need a MEGA board
// Memory management in a small memory environment like UNO
// Preemption of a lower priority task by a higher priority task
// Waiting on a semaphore, both indefinitely and with a timeout
// Task sleeping
// Signaling a semaphore from both a task and an interrupt service routine
// Measurement of latency between an interrupt service routine and a task 

#include <ARTK.h>

// these pins and interrupt numbers should work for UNO or MEGA
#define INTPIN 2 
#define INTNUM 0 

void producer(), consumer(), callSomething(), myisr() ;
SEMAPHORE sem ;
long itime ;

// This will be called by ARTK once, before the scheduler is started.  Use it as a 
// replacement for the Arduino setup() routine.  
void Setup()
{
  // using small memory model, leave default stack alone
  ARTK_SetOptions(0, -1) ;
  
  // if an ISR that signals a semaphore is installed here, then the semaphore
  // should also be created here in case the ISR fires right away
  sem = ARTK_CreateSema(0) ;
  
  pinMode(INTPIN, OUTPUT) ;
  digitalWrite(INTPIN, HIGH) ;
  attachInterrupt(INTNUM, myisr, LOW) ;  // FALLING prematurely triggers interrupt on Uno

   // three tasks are created with varying priority
   ARTK_CreateTask(consumer, 3) ;
   ARTK_CreateTask(producer, 2) ;
   ARTK_CreateTask(recurser, 1) ;
   Printf("3 tasks, stack=%d\n", ARTK_EstAvailRam() ) ; 
}

// this task demonstrates waiting on a semaphore
void consumer()
{
   int result = -1 ;

   Printf("consumer wait, stack=%d\n", ARTK_StackLeft()) ;
   ARTK_WaitSema(sem) ;
   Printf("rcvd 1st\n") ; 
   
   Printf("timed wait for 2nd\n") ;
   result = ARTK_WaitSema(sem, 5) ;
   while (result == -1)
   {
      Printf("timed out\n") ;
      result = ARTK_WaitSema(sem, 20) ;
   }
   Printf("rcvd 2nd, int = %ld usec ago\n", micros()-itime) ; 
}

// this task demonstrates signaling a semaphore
void producer()
{
   Printf("signaling, stack=%d\n", ARTK_StackLeft()) ;
   ARTK_SignalSema(sem) ;
   Printf("sleep 20\n") ; 
   ARTK_Sleep(20) ;
   Printf("triggering\n") ; 
   digitalWrite(INTPIN, LOW) ;
   Printf("producer exit\n") ; 
}

// this ISR also signals the semaphore
void myisr()
{
   digitalWrite(INTPIN, HIGH) ;
   itime = micros() ;
   ARTK_SignalSema(sem) ;
}

// recursive function call shows stack shrinkage
void doSomething(int val)
{
   if (val==0) return ;
   doSomething(val-1) ;
   Printf("doSomething, stack=%d\n", ARTK_StackLeft()) ;
}
void recurser()
{
   Printf("recurser, stack=%d\n", ARTK_StackLeft()) ;
   doSomething(10) ;
   Printf("recurser exit\n") ;
}
