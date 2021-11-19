
#include "so_stdio.h"
#include <fileapi.h>


int main() {


	SO_FILE* file = so_fopen("out.txt","r+");
	if (file == NULL) {
		printf("so_fopen failed");
		exit(0);
	}


	for (int i = 0; i < 25; i++) {

		int character = so_fgetc(file);
		printf("%c", character); 
	}


	printf("\ntell: %d", so_ftell(file));

	so_fseek(file, 7, SEEK_SET);
	printf("\ntell: %d\n", so_ftell(file));

	for (int i = 0; i < 5; i++) {

		int character = so_fgetc(file);
		printf("%c", character);
	}

	//so_fflush(file);
	//
	//for (int i = 0; i < 5; i++) {

	//	so_fputc(97 + i, file);
	//}
	//printf("\ntell: %d", so_ftell(file));
	//
	
	//for (int i = 0; i < 5; i++) {

	//	so_fputc(97 + i, file);
	//}
	
	
	
	
	so_fclose(file);
} 