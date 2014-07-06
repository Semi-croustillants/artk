// ARTK demo program for MEGA
// Note: the UNO cannot handle this many tasks
// After uploading, open the Serial Monitor and press the processor reset button
//
// This demo demonstrates the following:
// Preemption of a lower priority task by a higher priority task
// Waiting on a semaphore, both indefinitely and with a timeout
// Signaling a semaphore from both a task and an interrupt service routine
// Two methods for distinguishing between tasks with the same root function
// Task sleeping
// Stack size checking
// How the processor is shared between tasks of equal priority (not by timeslicing)
// Measurement of latency between an interrupt service routine and a task 

#include <ARTK.h>

// these pins and interrupt numbers should work for UNO or MEGA
#define LED    13         // this pin has an LED on all Arduino boards
#define INTPIN 2          // we'll use this pin to trigger an interrupt
#define INTNUM 0          // this is the corresponding interrupt number

void producer(), consumer(), sleeper(), printer(), recurser(), myisr() ;
SEMAPHORE sem ;
long itime ;

// This will be called by ARTK once, before the scheduler is started.  Use it as a 
// replacement for the Arduino setup() routine.  
void Setup()
{
  // changing to large model, leaving default sleep timer interval (10 msec)
  // changing the memory model MUST be done before creating any tasks  
  ARTK_SetOptions(1,-1) ;

  // if an ISR that signals a semaphore is installed here, then the semaphore
  // should also be created here in case the ISR fires right away
  sem = ARTK_CreateSema(0) ;

  pinMode(LED, OUTPUT) ;                   // configure an output pin for the LED
  pinMode(INTPIN, OUTPUT) ;                // we'll trigger an interrupt with a high
  digitalWrite(INTPIN, HIGH) ;             // to low transition on an output pin
  attachInterrupt(INTNUM, myisr, FALLING) ;
  
  Printf("Hello from Setup (%u avail)\n", ARTK_EstAvailRam() ) ; 

  // create several tasks with varying priority and a default stack size of 256
  ARTK_CreateTask(sleeper, 4) ;
  ARTK_CreateTask(consumer, 3) ;
  ARTK_CreateTask(producer, 2) ;
  ARTK_CreateTask(printer, 1) ;
  ARTK_CreateTask(printer, 1) ;
  ARTK_CreateTask(recurser, 5) ;
  
  Printf("Setup returning (%u avail)\n", ARTK_EstAvailRam() ) ; 
}

// this task demonstrates waiting on a semaphore
void consumer()
{
   int result = -1 ;
   
   Printf("consumer waiting on event\n") ; 
   ARTK_WaitSema(sem) ;
   Printf("consumer received first event\n") ; 
   
   Printf("consumer entering timed wait for second event\n") ;
   result = ARTK_WaitSema(sem, 5) ;
   while (result == -1)
   {
      Printf("consumer wait for second event timed out, trying again\n") ;
      result = ARTK_WaitSema(sem, 20) ;
   }
      
   Printf("consumer received second event, interrupt was %ld usec ago\n", micros()-itime) ; 
   Printf("consumer exiting\n") ; 
}

// this task demonstrates signaling a semaphore (it also triggers the ISR before exiting)
void producer()
{
   Printf("producer signaling event\n") ; 
   ARTK_SignalSema(sem) ;
   Printf("producer sleeping for 20 ticks\n") ;
   ARTK_Sleep(20) ;
   Printf("producer triggering the interrupt service routine\n") ; 
   digitalWrite(INTPIN, LOW) ;
   Printf("producer exiting\n") ; 
}

// this ISR also signals the producer/consumer semaphore
void myisr()
{
   itime = micros() ;
   ARTK_SignalSema(sem) ;
}

// this task demonstrates sleeping, and is high enough priority to preempt 
// other tasks in this demo, as seen in the output
void sleeper()
{
   Printf("Hello from sleeper\n") ;
   // give a message every 80 msec, then sleep - blink the LED too
   for (int i=0 ; i<10 ; i++)
   {
      digitalWrite(LED, ~digitalRead(LED)) ;
      Printf("Sleep %d\n", i) ; 
      ARTK_Sleep(8) ;
   }
   Printf("sleeper exiting\n") ;
}

// two tasks are created with this root function and demonstrate two ways
// that they can distinguish themselves from each other
void printer()
{
   static int num = 1 ;
   int mynum = num++ ;
   for (int i=0 ; i<6 ; i++)
      Printf("Printer %d, id=%d, Value=%d\n", mynum, ARTK_MyId(), i) ; 
   Printf("Printer %d exiting\n", mynum) ;
}

// This task calls a recursive function, printing the stack status each time
// The GNU GCC optimizer eliminates tail recursion, so the stack status is 
// printed out AFTER the recursive call in order to see stack growth (as we climb
// out of the recursion)
void doAgain(int cnt)
{
   if (cnt==0) return ;
   doAgain(cnt-1) ;
   Printf("returning from doAgain %d, stack left = %d\n", cnt, ARTK_StackLeft()) ;
}
void recurser()
{
   Printf("Hello from recurser, stack left = %d\n", ARTK_StackLeft()) ;
   doAgain(5) ;
   Printf("recurser exiting\n") ;
}

