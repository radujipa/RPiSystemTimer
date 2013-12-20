
#include <stdio.h>
#include <stdlib.h>

#define DEBUG_ST	/* Turns debugging messages ON for system timer operations */
#include "systimerops.h"


/*
 * 
 ******************************************************************************
 */
void print_syntax (void)
{
	printf ("Syntax is: read [device_number]\n\n") ;

	printf("Device numbers:\n");
	printf ("0 - System Timer CS 	\n") ;
	printf ("1 - System Timer CLO	\n") ;
	printf ("2 - System Timer CHI \n") ;
	printf ("3 - System Timer C0  \n") ;
	printf ("4 - System Timer C1  \n") ;
	printf ("5 - System Timer C2  \n") ;
	printf ("6 - System Timer C3  \n") ;
	printf ("all - reads all the above\n") ;
}


/*
 * A simple program to test the System Timer driver
 ******************************************************************************
 */
int main (int argc, char* argv[])
{
	/*  */
	unsigned int read_value ;

	const char *files[] = {"/dev/CS", "/dev/CLO", "/dev/CHI", "/dev/C0", "/dev/C1", "/dev/C2", "/dev/C3"} ;
	const unsigned int file_count = sizeof (files) / sizeof (char*) ;

	/*  */
	if (argv[1] == NULL)
	{		
		print_syntax () ;
		exit (1) ;
	}

	if (strcmp(argv[1], "all") == 0)
	{
		for (int index = 0 ; index < file_count ; index++)
		  for (int repeat = 1 ; repeat <= 1 ; repeat++)
		  	read_from_device (files[index], &read_value) ;

	} // if

	/*  */
	else 
	{
		int index = atoi(argv[1]) ;
		read_from_device (files[index], &read_value) ;
	}
				
		
  /* Program exists cleanly */
	return 0 ;
} //main
