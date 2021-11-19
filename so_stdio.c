
#include "so_stdio.h"

#define _DEBUG_
#include "utils.h"

#include <fileapi.h> //windows file api
#include <string.h>


#define SO_TRUE 1
#define SO_FALSE 0

#define SO_BUFFER_SIZE 5

struct _so_file {

	//handler-ul fisierului deschis
#if defined(__linux__)
	int _hFile;
#elif defined(_WIN32)
	HANDLE _hFile;
#else
#error "Unknown platform"
#endif

	//ptr cursor
	char* _read_ptr;		// NULL cand fisierul nu e deschis pentru citire	;	pointeaza spre buffer in rest
	char* _read_ptr_end;	// PTR spre finalul bufferului intern disponibil de citit
	char* _write_ptr;		// NULL cand fisierul nu e deschis pentru scriere	;	pointeaza spre buffer in rest
	char* _write_ptr_end;	// PTR spre finalul bufferului intern disponibil de scris

	//flaguri - 
	int _canRead;	// ia valoarea fals dupa o operatie de scriere	;	devine true dupa fflush/fseek
	int _canWrite;	// ia valoarea fals dupa operatie de citire		;	devine true dupa fseek
	int _update;	// modul update
	int _append;	// modul append

	//bufere - bufferele interne
	char* _buffer_base;		//bufferul intern
	char* _buffer_end;					//pointer spre ultima pozitie din el pentru verificari ulterioare

	//flaguri+
	int _feof;
	int _ferror;

	long _file_pointer_pos;
};

// Deschidere fisiere, in diferite moduri
static SO_FILE* OpenFileModeRead(const char* pathname);				// r
static SO_FILE* OpenFileModeWrite(const char* pathname);			// w
static SO_FILE* OpenFileModeAppend(const char* pathname);			// a
static SO_FILE* OpenFileModeReadUpdate(const char* pathname);		// r+
static SO_FILE* OpenFileModeWriteUpdate(const char* pathname);		// w+
static SO_FILE* OpenFileModeAppendUpdate(const char* pathname);		// a+


static int FillInternalBuffer(SO_FILE* stream);
static int WriteInternalBuffer(SO_FILE* stream);



SO_FILE* so_fopen(const char* pathname, const char* mode) {

	SO_FILE* file = NULL;

	if (strcmp(mode, "r") == 0) {
		file = OpenFileModeRead(pathname);
	}
	else if (strcmp(mode, "w") == 0) {
		file = OpenFileModeWrite(pathname);
	}
	else if (strcmp(mode, "a") == 0) {
		file = OpenFileModeAppend(pathname);
	}
	else if (strcmp(mode, "r+") == 0) {
		file = OpenFileModeReadUpdate(pathname);
	}
	else if (strcmp(mode, "w+") == 0) {
		file = OpenFileModeWriteUpdate(pathname);
	}
	else if (strcmp(mode, "a+") == 0) {
		file = OpenFileModeAppendUpdate(pathname);
	}

	if (file != NULL) {
		file->_buffer_base = (char*)malloc(sizeof(char) * SO_BUFFER_SIZE);
		file->_buffer_end = file->_buffer_base + SO_BUFFER_SIZE; 

		file->_read_ptr = file->_buffer_base;
		file->_read_ptr_end = file->_read_ptr;

		file->_write_ptr = file->_buffer_base;
		file->_write_ptr_end = file->_buffer_end;

		file->_feof = SO_FALSE;

		file->_file_pointer_pos = 0;	//pozitia cursorului in fiser!
	}
	return file;
}
int so_fclose(SO_FILE* stream) {

	if (stream == NULL)
		return;

	so_fflush(stream);

	free(stream->_buffer_base);

	stream->_read_ptr = NULL;
	stream->_write_ptr = NULL;
	stream->_buffer_end = NULL;

	free(stream);
	stream = NULL;
}

#if defined(__linux__)
int so_fileno(SO_FILE* stream) {

}
#elif defined(_WIN32)
HANDLE so_fileno(SO_FILE* stream) {
	return stream->_hFile;
}
#else
#error "Unknown platform"
#endif

int so_fflush(SO_FILE* stream) {

	int ret;
	if (stream->_canWrite) {

		ret = WriteInternalBuffer(stream);
		if (ret != 0)
			return ret;
	}
	if (stream->_update) {

		stream->_canRead = SO_TRUE;
		stream->_canWrite = SO_TRUE;
	}

	stream->_write_ptr = stream->_buffer_base; //invalidam tot ce era scris pana amu, aducem cursoru de scris la inceput
	stream->_write_ptr_end = stream->_write_ptr;
	
	stream->_read_ptr = stream->_buffer_base;
	stream->_read_ptr_end = stream->_read_ptr;
	

	return 0; 
}

int so_fseek(SO_FILE* stream, long offset, int whence) {

	long ret;

#if defined(__linux__)
	
#elif defined(_WIN32)
	ret = SetFilePointer(stream->_hFile, offset, NULL, whence);
	if (ret == INVALID_SET_FILE_POINTER) {
		PRINT_MY_ERROR("SetFilePointer");
		return -1;
	}
#else
#error "Unknown platform"
#endif

	so_fflush(stream);
	stream->_feof = 0;
	stream->_file_pointer_pos = ret;	//actaulizam pozitia pointerului la cursor
	return 0;
}
long so_ftell(SO_FILE* stream) {

	return stream->_file_pointer_pos;
}

size_t so_fread(void* ptr, size_t size, size_t nmemb, SO_FILE* stream) {

}
size_t so_fwrite(const void* ptr, size_t size, size_t nmemb, SO_FILE* stream) {

}

int so_fgetc(SO_FILE* stream) {

	int readChar;
	int ret;

	if (!stream->_canRead || stream->_feof) {

		if (!stream->_canRead && stream->_update)
			PRINT_MY_ERROR("FFLUSH NEDEED");
		return SO_EOF;
	}

	if (stream->_update) {
	
		//o operatie de scriere trebuie urmata de uuna de fflush/fseek pentru a putea si scrie
		//deci nu lasam sa se scrie
		stream->_canWrite = SO_FALSE; 
	}

	if (stream->_read_ptr == stream->_read_ptr_end) {
		//buffer intern gol sau plin -> populam buffer intern
		ret = FillInternalBuffer(stream);
		if (ret != SO_TRUE) {
			return SO_EOF;
		}
	}

	if (!stream->_feof) {
		readChar = stream->_read_ptr[0];
		stream->_read_ptr++;
		stream->_file_pointer_pos++;
	}


	return readChar;
}
int so_fputc(int c, SO_FILE* stream) {

	int ret;

	if (!stream->_canWrite) {

		if (!stream->_canWrite && stream->_update)
			PRINT_MY_ERROR("FFLUSH NEDEED");
		return SO_EOF;
	}

	if (stream->_update) {
			
		//dupa o operatie de scriere e nevoie de fflush pentru a puteta citii;
		stream->_canRead = SO_FALSE;
	}

	if (stream->_append) {

		//muta cursor la final
	}

	if (stream->_write_ptr == stream->_write_ptr_end) {
	
		ret = so_fflush(stream);
		if (ret != 0)
			return SO_EOF;
	}

	stream->_write_ptr[0] = c;
	stream->_write_ptr++;
	stream->_file_pointer_pos++;
}

int so_feof(SO_FILE* stream) {

	return stream->_feof;
}
int so_ferror(SO_FILE* stream) {
	return stream->_ferror;
}

static SO_FILE*  OpenFileModeRead(const char* pathname) {

	SO_FILE* so_file = (SO_FILE*)malloc(sizeof(SO_FILE));
	if (so_file == NULL) {

		PRINT_MY_ERROR("malloc failed");
		return NULL;
	}

#if defined(__linux__)
	

#elif defined(_WIN32)
	so_file->_hFile = CreateFileA(pathname, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (so_file->_hFile == INVALID_HANDLE_VALUE) {
		PRINT_MY_ERROR("CreateFile");
		free(so_file);
		return NULL;
	}

#else
#error "Unknown platform"
#endif

	so_file->_canRead = SO_TRUE;
	so_file->_canWrite = SO_FALSE;
	so_file->_append = SO_FALSE;
	so_file->_update = SO_FALSE;

	return so_file;
}
static SO_FILE* OpenFileModeWrite(const char* pathname) {

	SO_FILE* so_file = (SO_FILE*)malloc(sizeof(SO_FILE));
	if (so_file == NULL) {

		PRINT_MY_ERROR("malloc failed");
		return NULL;
	}

#if defined(__linux__)


#elif defined(_WIN32)
	so_file->_hFile = CreateFileA(pathname, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (so_file->_hFile == INVALID_HANDLE_VALUE) {
		PRINT_MY_ERROR("CreateFile");
		free(so_file);
		return NULL;
	}

#else
#error "Unknown platform"
#endif

	so_file->_canRead = SO_FALSE;
	so_file->_canWrite = SO_TRUE;
	so_file->_append = SO_FALSE;
	so_file->_update = SO_FALSE;

	return so_file;
}
static SO_FILE* OpenFileModeAppend(const char* pathname) {

	SO_FILE* so_file = (SO_FILE*)malloc(sizeof(SO_FILE));
	if (so_file == NULL) {

		PRINT_MY_ERROR("malloc failed");
		return NULL;
	}

#if defined(__linux__)


#elif defined(_WIN32)
	so_file->_hFile = CreateFileA(pathname, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (so_file->_hFile == INVALID_HANDLE_VALUE) {
		PRINT_MY_ERROR("CreateFile");
		free(so_file);
		return NULL;
	}

#else
#error "Unknown platform"
#endif

	so_file->_canRead = SO_FALSE;
	so_file->_canWrite = SO_TRUE;
	so_file->_append = SO_TRUE;
	so_file->_update = SO_FALSE;

	return so_file;
}
static SO_FILE* OpenFileModeReadUpdate(const char* pathname) {

	SO_FILE* so_file = (SO_FILE*)malloc(sizeof(SO_FILE));
	if (so_file == NULL) {

		PRINT_MY_ERROR("malloc failed");
		return NULL;
	}

#if defined(__linux__)


#elif defined(_WIN32)
	so_file->_hFile = CreateFileA(pathname, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (so_file->_hFile == INVALID_HANDLE_VALUE) {
		PRINT_MY_ERROR("CreateFile");
		free(so_file);
		return NULL;
	}

#else
#error "Unknown platform"
#endif

	so_file->_canRead = SO_TRUE;
	so_file->_canWrite = SO_TRUE;
	so_file->_append = SO_FALSE;
	so_file->_update = SO_TRUE;

	return so_file;
}
static SO_FILE* OpenFileModeWriteUpdate(const char* pathname) {

	SO_FILE* so_file = (SO_FILE*)malloc(sizeof(SO_FILE));
	if (so_file == NULL) {

		PRINT_MY_ERROR("malloc failed");
		return NULL;
	}

#if defined(__linux__)


#elif defined(_WIN32)
	so_file->_hFile = CreateFileA(pathname, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (so_file->_hFile == INVALID_HANDLE_VALUE) {
		PRINT_MY_ERROR("CreateFile");
		free(so_file);
		return NULL;
	}

#else
#error "Unknown platform"
#endif

	so_file->_canRead = SO_TRUE;
	so_file->_canWrite = SO_TRUE;
	so_file->_append = SO_FALSE;
	so_file->_update = SO_TRUE;

	return so_file;
}
static SO_FILE* OpenFileModeAppendUpdate(const char* pathname) {

	SO_FILE* so_file = (SO_FILE*)malloc(sizeof(SO_FILE));
	if (so_file == NULL) {

		PRINT_MY_ERROR("malloc failed");
		return NULL;
	}

#if defined(__linux__)


#elif defined(_WIN32)
	so_file->_hFile = CreateFileA(pathname, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (so_file->_hFile == INVALID_HANDLE_VALUE) {
		PRINT_MY_ERROR("CreateFile");
		free(so_file);
		return NULL;
	}

#else
#error "Unknown platform"
#endif

	so_file->_canRead = SO_TRUE;
	so_file->_canWrite = SO_TRUE;
	so_file->_append = SO_FALSE;
	so_file->_update = SO_TRUE;

	return so_file;
}



static int FillInternalBuffer(SO_FILE * stream) {

	int numberOfBytesRead = 0;

#if defined(__linux__)
	//citeste caracter
	//if eraore -> return SO_EOF
#elif defined(_WIN32)

	BOOL ret = ReadFile(stream->_hFile, stream->_buffer_base, SO_BUFFER_SIZE, &numberOfBytesRead, NULL);
	if (ret != TRUE) {
		PRINT_MY_ERROR("ReadFile");
		return SO_EOF;
	}
	if (ret && numberOfBytesRead == 0) {
		stream->_feof = SO_EOF;
		return SO_EOF;
	}	
#else
#error "Unknown platform"
#endif

	stream->_read_ptr_end = stream->_buffer_base + numberOfBytesRead;
	stream->_read_ptr = stream->_buffer_base;
	return SO_TRUE;
}
static int WriteInternalBuffer(SO_FILE* stream)
{

	int numberBytesWritten = 0;
	int numberBytesWrittenTotal = 0;
	int numberBytesToWrite = stream->_write_ptr - stream->_buffer_base;  //write_ptr indica ultimu caracter scris, buffer base e inceputul;
	if (numberBytesToWrite == 0)
		return 0;

#if defined(__linux__)
																		 //dsadas
#elif defined(_WIN32)
	BOOL bRet;
	do {
		bRet = WriteFile(stream->_hFile,
			stream->_buffer_base + numberBytesWrittenTotal,
			numberBytesToWrite - numberBytesWrittenTotal,
			&numberBytesWritten,
			NULL);
		if (bRet != TRUE) {
			PRINT_MY_ERROR("WriteFile - fflush");
			return SO_EOF;
		}

		numberBytesWrittenTotal += numberBytesWritten;
	} while (numberBytesWrittenTotal < numberBytesToWrite);

#else
#error "Unknown platform"
#endif

	return 0;
}


