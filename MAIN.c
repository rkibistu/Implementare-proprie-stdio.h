
#include "so_stdio.h"
#include <fileapi.h>


int main() {


	SO_FILE* file = so_fopen("out.txt","a+");
	if (file == NULL) {
		printf("so_fopen failed");
		exit(0);
	}



	char buf[] = "scriu in fiser";
	int bytes = so_fwrite(buf, 1, strlen(buf), file);

	so_fseek(file, 10, SEEK_SET);
	
	char buff[100];
	bytes = so_fread(buff, 1,20, file);

	buff[bytes] = '\0';
	printf("%d: %s", bytes, buff);
	
	//printf("strlen:%d   fwrite:%d", strlen(buf), bytes);

	//printf("\nEOF: %d\n", so_feof(file));
	////for (int i = 0; i < 25; i++) {

	//	int character = so_fgetc(file);
	//	printf("%c  %d\n", character, so_feof(file)); 
	//}


	//printf("\ntell: %d", so_ftell(file));

	//so_fseek(file, 7, SEEK_SET);
	//printf("\ntell: %d\n", so_ftell(file));

	//for (int i = 0; i < 5; i++) {

	//	int character = so_fgetc(file);
	//	printf("%c", character);
	//}

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