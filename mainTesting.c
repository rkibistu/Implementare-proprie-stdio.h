#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

int _main() {


	FILE* file = fopen("out.txt", "r+");
	if (file == NULL) {

		printf("error open");
		return 0;
	}

	for (int i = 0; i < 3; i++) {

		printf("%c", fgetc(file));
	}

	
	for (int i = 0; i < 5; i++) {

		fputc('a', file);
	}
	
	for (int i = 0; i < 5; i++) {

		fputc('b', file);
	}
 	fclose(file);
}