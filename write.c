
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
	printf ("Syntax is: write [device_number] [value]\n\n") ;

	printf("Device numbers:\n");
	printf ("0 - System Timer CS  (NOT SAFE)\n") ;
	printf ("1 - System Timer CLO (NOT SAFE)\n") ;
	printf ("2 - System Timer CHI (NOT SAFE)\n") ;
	printf ("3 - System Timer C0  (NOT SAFE)\n") ;
	printf ("4 - System Timer C1  (safe)\n") ;
	printf ("5 - System Timer C2  (NOT SAFE)\n") ;
	printf ("6 - System Timer C3  (NOT SAFE)\n") ;
}


/*
 * A simple program to test the System Timer driver
 ******************************************************************************
 */
int main (int argc, char* argv[])
{
	const char *files[] = {"/dev/CS", "/dev/CLO", "/dev/CHI", "/dev/C0", "/dev/C1", "/dev/C2", "/dev/C3"} ;
	//const unsigned int file_count = sizeof (files) / sizeof (char*) ;

	/*  */
	if (argv[1] == NULL || argv[2] == NULL)
	{		
		print_syntax () ;
		exit (1) ;
	}

	/*  */
	int device_number = atoi(argv[1]) ;
	unsigned int write_value ;

	sscanf (atoi(argv[2]), "%u", &write_value) ;

	/*  */
	for (int repeat = 1 ; repeat <= 1 ; repeat++)
		write_to_device (files[device_number], &value) ;
	
	
  /* Program exists cleanly */
	return 0 ;
} //main
