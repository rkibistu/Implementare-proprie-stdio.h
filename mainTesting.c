#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

int _main() {


	FILE* f = fopen("test.txt", "r+");
	if (f == NULL) {

		printf("error open");
		return 0;
	}

	char buf[4];
	int x = fread(buf, 1, sizeof(buf), f);
	buf[x - 1] = '\0';
	printf("\nBuf: %s", buf);

	//fseek(f, 0, SEEK_END);
	fflush(f);

	x = fwrite(buf, 1, sizeof(buf), f);
	printf("\nX: %d", x);

	fflush(f);

	fclose(f);
}