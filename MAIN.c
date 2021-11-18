
#include "so_stdio.h"
#include <fileapi.h>


int main() {


	SO_FILE* file = so_fopen("out.txt","w");
	if (file == NULL) {
		printf("so_fopen failed");
		exit(0);
	}

	//for (int i = 0; i < 50; i++) {

	//	printf("%c", so_fgetc(file));
	//}

	for (int i = 0; i < 5; i++) {

		so_fputc(97 + i, file);
	}
	so_fflush(file);
	for (int i = 0; i < 5; i++) {

		so_fputc(97 + i, file);
	}
	so_fclose(file);
} 