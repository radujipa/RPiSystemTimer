
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#define DEBUG_ST	/* Turns debugging messages ON for system timer operations */
#include "systimerops.h"


/*
 * 
 ******************************************************************************
 */
void print_syntax (void)
{
	printf ("Syntax is: interrupts [interval]\n") ;
}


/*
 * 
 ******************************************************************************
 */
volatile sig_atomic_t interrupts = 1 ;
volatile int interrupts_gone_wrong = 1 ;

void signal_handler (int signal)
{
	switch (signal)
	{
		case SIGUSR1:
			interrupts++ ;
			interrupts_gone_wrong++ ;
			//printf ("Interrupt occured. Next.\n") ;
			break ;

		case SIGUSR2:
			printf ("Received %d / %d interrupts. Well?\n", interrupts, interrupts_gone_wrong) ;
			break ;

		default:
			printf ("An unhandled signal was caught. Error or test?\n") ;
			break ;
	}
}


/*
 * A simple program to test the System Timer driver
 ******************************************************************************
 */
int main (int argc, char* argv[])
{
	/*  */	
	unsigned int  read_value ;
	unsigned int write_value = 0 ;

	const char *files[] = {"/dev/CS", "/dev/CLO", "/dev/CHI", "/dev/C0", "/dev/C1", "/dev/C2", "/dev/C3"} ;
	//const unsigned int file_count = sizeof (files) / sizeof (char*) ;

	/*  */
	if (argv[1] == NULL)
	{
		print_syntax () ;
		exit (1) ;
	}

	/*  */
	piHiPri (100) ;

	/*  */
	unsigned int interval ;
	sscanf (argv[1], "%u", &interval) ;

	/* 	 */
	read_from_device (files[1], &read_value) ;
	write_value = (read_value / 10000000 + 1) * 10000000 ;
	write_to_device  (files[4], &write_value) ;



	/*  */
	sigset_t block_mask, oldmask ;
	
	/* Set up the mask of signals to temporarily block. */ 
	sigemptyset (&block_mask) ;
	sigaddset (&block_mask, SIGUSR1) ;
	sigaddset (&block_mask, SIGUSR2) ;

	/*  */
	struct sigaction action ;
  
  /* Establish the signal handler.  */
  action.sa_handler = signal_handler ;
  action.sa_mask = block_mask ;
  action.sa_flags = 0 ;
  
  /*  */
  sigaction (SIGUSR1, &action, NULL) ;
	sigaction (SIGUSR2, &action, NULL) ;

	/*  */
	sigprocmask (SIG_BLOCK, &block_mask, &oldmask) ;
	
	/* Wait for a signal to arrive. */
	while (1)
	  sigsuspend (&oldmask) ;

	/*  */
	sigprocmask (SIG_UNBLOCK, &block_mask, NULL) ;

	
  /* Program exists cleanly */
	return 0 ;
} //main
