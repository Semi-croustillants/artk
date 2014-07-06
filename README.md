artk
====

ARTK is a real-time priority-driven multitasking kernel for the Arduino released under GPL.

* You can read about ARTK in the following article (please cite this if you publish work that makes use of ARTK): 
 
  P. H. Schimpf, “ARTK, a Compact Real-Time Kernel for Arduino,” International Journal of Embedded Systems (IJES), Vol. 5, No. 1, pp. 106-113, 2013.
  (http://www.inderscience.com/info/inarticle.php?artid=52176)

* More information can be found here:
  (https://sites.google.com/site/pschimpf99/home/artk)

Jun 2012  Paul Schimpf
Feb 2013  Added citation for Embedded Systems article
Jul 2014  Minor tweaks by Craig Trader for portability.

To install ARTK
===============

Create a directory called ARTK under your Arduino library directory.
Extract the contents of ARTK.zip into that directory.

To test ARTK
============

Create a directory somewhere called ARTKtestMega or ARTKtestUno (depending on
whether you have an Arduino with more or less than a 64kbyte memory space).

Copy the corresponding .ino file into that directory.

Start the Arduino environment and load that sketch.

Compile and Upload to your Arduino.

Open your Serial Monitor (on the Tools menu).

You may need to press the Arduino reset button.

Compare the Monitor output to the contents of the file outputMega.txt 
or outputUno.txt

What next?
==========

Read all the comments and code in the test file carefully and make sure you
understand why the output looks the way it does.

Read the short user manual. This manual is published in the International Journal
for Embedded Systems:

Paul H. Schimpf, "ARTK: a compact real-time kernel for Arduino,"
Int. J. Embedded Systems, Vol. 5, No. 1/2, pp. 106-113, 2013

Please refer to the above publication if you publish an article describing a system 
that makes use of ARTK.

Experiment.

Try programming the dining philosophers problem in ARTK. Implement each philosopher
as a task, and each fork (or chopstick, if you prefer) as a semaphore that can support
a single entry. You can use ARTK_Sleep to simulate eating and thinking. For a bit of 
extra challenge implement all your philosophers using the same root function. On an 
Uno don't try to program more than 3 philosophers.

Is deadlock a problem with dining philosophers in ARTK?  Not if all your
philosophers have the same priority.  The reason deadlock will never happen
in this case is that, unlike some kernels, ARTK does NOT timeslice tasks 
with equal priority.  When one such task is pre-empted, or voluntary relinquishes
the processor with ARTK_Yield(), it goes to the end of a round-robin queue
with other tasks at the same priority level.  You can simulate what can happen
in a time-slicing kernel by having your philosophers call ARTK_Yield() in 
between obtaining their right and left forks (or chopsticks). Then try and find a 
solution that fixes the deadlock without removing the Yield (hint: try having one
philosopher pick up the forks in the opposite order of the other philosophers).

Try using ARTK tasks in your next Arduino embedded systems project,
but only if concurrent execution makes sense for it.

It's Free
=========

ARTK is open source, in keeping with the spirit of Arduino. Please respect the 
GPL license terms, which you can find in the file gpl.txt. ARTK is also free.
I developed this on my own time and received no compensation for doing so. If 
you find it useful feel free (or not, it is entirely up to you) to send a PayPal 
contribution to: 

pay.pschimpf@gmail.com 


