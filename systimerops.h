
#include <unistd.h>		/* read(), close() */
#include <fcntl.h>		/* open() */

#include <sched.h>		/* Process priority changing mechanism via scheduler */
#include <string.h>		/* memset() */


/*
 * Attempt to set a high (realtime) priority schedulling for the running program
 * 		from Gordon Henderson's WiringPi project
 *********************************************************************************
 */
void piHiPri (unsigned int priority)
{
  struct sched_param scheduler ;

  /* Initialisation - clears the memory held by the structure */
  memset (&scheduler, 0, sizeof(scheduler)) ;

  /* Either set the given priority or clip it at maximum */
  if (priority > sched_get_priority_max (SCHED_RR))
    scheduler.sched_priority = sched_get_priority_max (SCHED_RR) ;
  else
    scheduler.sched_priority = priority ;

  /* Apply changes */
  sched_setscheduler (0, SCHED_RR, &scheduler) ;
}


/*
 * 
 *********************************************************************************
 */
ssize_t read_from_device (const char *file, unsigned int *value)
{
	int fd ;  													/* file descriptor */
	char buffer[32] ;										/* char driver requires char buffer */
	size_t bytes = sizeof (buffer) ; 		/* size of the buffer */

	/* Open the given file and check return code */
	if ((fd = open (file, O_RDONLY)) < 0)
	{
	  fprintf(stderr, "open() failed.\n") ;
	  exit(1) ;
	}

	/* Read a number of bytes from the file and put the contents in the buffer */
	ssize_t read_bytes = read (fd, buffer, bytes) ;

	/* Copy the buffer contents to the value to return */
	sscanf (buffer, "%u", value) ;

	/* DEBUGGING: Check return value of our read */
#ifdef DEBUG_ST
	if (read_bytes == 0)
		printf ("Nothing more to read. EOF.\n") ;
	else
		printf ("%s :\nBytes read: %zu\nContents: %s\n", file, read_bytes, buffer) ;
#endif

	/* Close the file and check return code */
	if (close (fd) < 0)
	{
		fprintf(stderr, "close() failed.\n") ;
	  exit(1) ;
	}

	/* Might be useful.. */
	return read_bytes ;
}


/*
 * 
 *********************************************************************************
 */
ssize_t write_to_device (const char *file, unsigned int *value)
{
	int fd ;  													/* file descriptor */
	char buffer[32] ;										/* char driver requires char buffer */
	size_t bytes = sizeof (buffer) ; 		/* size of the buffer */

	/* Open the given file and check return code */
	if ((fd = open (file, O_WRONLY)) < 0)
	{
	  fprintf(stderr, "open() failed.\n") ;
	  exit(1) ;
	}

	/* Copy the value to be written to the buffer */
	snprintf (buffer, bytes, "%u", *value) ;
		 	
	/* Write the number of bytes from the buffer into the file */
	ssize_t wrote_bytes = write (fd, buffer, bytes) ;

	/* DEBUGGING: Check the return value of our write */
#ifdef DEBUG_ST
	if (wrote_bytes == 0)
		printf ("%s :\nCould not write any bytes. EOF.\n", file) ;
	else
		printf ("%s :\nBytes wrote: %zu\nWrote:    %s\n", file, wrote_bytes, buffer) ;
#endif

	/* Close the file and check return code */
	if (close (fd) < 0)
	{
		fprintf(stderr, "close() failed.\n") ;
	  exit(1) ;
	}

	/* Again, might be useful.. */
	return wrote_bytes ;
}
