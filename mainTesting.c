#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

int _main() {


	FILE* file = fopen("out.txt", "r+");
	if (file == NULL) {

		printf("error open");
		return 0;
	}


	size_t bytes_read;
	char buf[10];

	
	bytes_read = fread(buf, sizeof(buf), 1, file);
	buf[9] = '\0';
	printf("%d sir:\n%s", bytes_read, buf);

	//int* intarray = (int*)malloc(sizeof(int) * 5);
	//fread((int*)intarray, sizeof(int), 5, file);

	//for (int i = 0; i < 5; i++) {

	//	printf("%d  ", intarray[i]);
	//}

 	fclose(file);
}