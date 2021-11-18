#ifndef LIN_UTILS_H_
#define LIN_UTILS_H_	1

#include <stdio.h>
#include <stdlib.h>

#ifdef _DEBUG_	

#define PRINT_MY_ERROR( call_description)				\
	do {								\
		fprintf(stderr, "\n(%s, %d): ",			\
				__FILE__, __LINE__);		\
		printf(call_description);			\
		printf("\n");		\
						\
	} while (0)		

#else

#define PRINT_MY_ERROR( call_description)	
	

#endif _DEBUG_



#endif