#include "so_stdio.h"

#define _DEBUG_
#include "utils.h"

#if defined(__linux__)
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#elif defined(_WIN32)
#include <fileapi.h> //windows file api
#else
#error "Unknown platform"
#endif
#include <string.h>


#define SO_TRUE 1
#define SO_FALSE 0

#define SO_BUFFER_SIZE 4096

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

	int ignore;
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


#if defined(__linux__)


#elif defined(_WIN32)
static int RedirectHandleWindows(STARTUPINFO* psi, HANDLE hFile, INT opt);
static HANDLE CreatePipeProcessWindows(const char* command, const char* mode);
static SO_FILE* OpenPipeProcessWindows(const char* command, const char* mode);
#else
#error "Unknown platform"
#endif


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
		return -1;

	so_fflush(stream);

	free(stream->_buffer_base);

	stream->_read_ptr = NULL;
	stream->_write_ptr = NULL;
	stream->_buffer_end = NULL;


#if defined(__linux__)
	close(stream->_hFile);
#elif defined(_WIN32)
	CloseHandle(stream->_hFile);
#else
#error "Unknown platform"
#endif

	free(stream);
	stream = NULL;

	return 0;
}



#if defined(__linux__)
int so_fileno(SO_FILE* stream) {
	return stream->_hFile;
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
	stream->_write_ptr_end = stream->_buffer_end;

	stream->_read_ptr = stream->_buffer_base;
	stream->_read_ptr_end = stream->_read_ptr;


	return 0;
}

int so_fseek(SO_FILE* stream, long offset, int whence) {

	long ret;

#if defined(__linux__)
	ret = lseek(stream->_hFile, offset, whence);
	if (ret == -1) {
		PRINT_MY_ERROR("lseek");
		return -1;
	}
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

	size_t indexNmemb;
	size_t indexSize;
	size_t bytesReadTotal = 0;
	int numberOfElementRead = 0;
	char* element = (char*)malloc(sizeof(char) * size);

	if (size == 0 || nmemb == 0)
		return 0;

	for (indexNmemb = 0; indexNmemb < nmemb; indexNmemb++) {

		for (indexSize = 0; indexSize < size; indexSize++) {

			element[indexSize] = so_fgetc(stream);
			if (element[indexSize] == SO_EOF) {

				if (stream->ignore == SO_FALSE) { /////////////////
					free(element);
					return bytesReadTotal;
				}

			}
		}
		numberOfElementRead++;
		memcpy((char*)ptr + indexNmemb * size, element, size);
		bytesReadTotal += size;
	}

	free(element);
	return numberOfElementRead;
}
size_t so_fwrite(const void* ptr, size_t size, size_t nmemb, SO_FILE* stream) {

	size_t index;
	int ret;
	size_t bytesWrittenTotal = 0;

	int numberOfElementsWritten = 0;

	for (index = 0; index < size * nmemb; index++) {

		ret = so_fputc(((unsigned char*)ptr)[index], stream);
		if (ret == SO_EOF) {

			return bytesWrittenTotal;
		}
		bytesWrittenTotal++;

		if (index % size == 0)
			numberOfElementsWritten++;
	}
	return numberOfElementsWritten;
}

int so_fgetc(SO_FILE* stream) {

	int readChar;
	int ret;

	stream->ignore = SO_FALSE;/////////////////////////////////////////////

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
		if (readChar == -1) {
			stream->ignore = SO_TRUE; ////////////////////////////////////////
		}
		stream->_read_ptr++;
		stream->_file_pointer_pos++;
	}

	//printf("%d\n",readChar);
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
		so_fseek(stream, 0, SEEK_END);
	}

	if (stream->_write_ptr == stream->_write_ptr_end) {

		ret = so_fflush(stream);
		if (ret != 0)
			return SO_EOF;
	}

	stream->_write_ptr[0] = c;
	stream->_write_ptr++;
	stream->_file_pointer_pos++;
	return c;
}

int so_feof(SO_FILE* stream) {

	return stream->_feof;
}
int so_ferror(SO_FILE* stream) {
	return stream->_ferror;
}

SO_FILE* so_popen(const char* command, const char* type) {

	SO_FILE* file = NULL;

#if defined(__linux__)
	//int hFile;

#elif defined(_WIN32)

	file = OpenPipeProcessWindows(command, type);
#else
#error "Unknown platform"
#endif

	return file;

}
int so_pclose(SO_FILE* stream) {

	if (stream == NULL)
		return -1;

	so_fflush(stream);

	free(stream->_buffer_base);

	stream->_read_ptr = NULL;
	stream->_write_ptr = NULL;
	stream->_buffer_end = NULL;


#if defined(__linux__)

#elif defined(_WIN32)
	CloseHandle(stream->_hFile);
	//SI UNDE SE INCHIDE HANDLERUL DE PROCES ?
#else
#error "Unknown platform"
#endif

	free(stream);
	stream = NULL;

	return 0;
}

static SO_FILE* OpenFileModeRead(const char* pathname) {

	SO_FILE* so_file = (SO_FILE*)malloc(sizeof(SO_FILE));
	if (so_file == NULL) {

		PRINT_MY_ERROR("malloc failed");
		return NULL;
	}

#if defined(__linux__)
	so_file->_hFile = open(pathname, O_RDONLY);
	if (so_file->_hFile == -1) {
		PRINT_MY_ERROR("open");
		free(so_file);
		return NULL;
	}

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
	so_file->_hFile = open(pathname, O_WRONLY | O_TRUNC | O_CREAT, 0664);
	if (so_file->_hFile == -1) {
		PRINT_MY_ERROR("open");
		free(so_file);
		return NULL;
	}

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
	so_file->_hFile = open(pathname, O_APPEND | O_WRONLY | O_CREAT, 0644);
	if (so_file->_hFile == -1) {
		PRINT_MY_ERROR("open");
		free(so_file);
		return NULL;
	}

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
	so_file->_hFile = open(pathname, O_RDWR);
	if (so_file->_hFile == -1) {
		PRINT_MY_ERROR("open");
		free(so_file);
		return NULL;
	}


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
	so_file->_hFile = open(pathname, O_RDWR | O_TRUNC | O_CREAT, 0644);
	if (so_file->_hFile == -1) {
		PRINT_MY_ERROR("open");
		free(so_file);
		return NULL;
	}


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
	so_file->_hFile = open(pathname, O_APPEND | O_RDWR | O_CREAT, 0644);
	if (so_file->_hFile == -1) {
		PRINT_MY_ERROR("open");
		free(so_file);
		return NULL;
	}


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
	so_file->_append = SO_TRUE;
	so_file->_update = SO_TRUE;

	return so_file;
}


static int FillInternalBuffer(SO_FILE* stream) {

	int numberOfBytesRead = 0;
#if defined(__linux__)
	//citeste caracter
	numberOfBytesRead = read(stream->_hFile, stream->_buffer_base, SO_BUFFER_SIZE);
	if (numberOfBytesRead == -1) {
		PRINT_MY_ERROR("read");
		return SO_EOF;
	}
	if (numberOfBytesRead == 0) {
		stream->_feof = SO_EOF;
		return SO_EOF;
	}
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
	//write(STDOUT_FILENO,stream->_buffer_base,numberOfBytesRead);
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
	do {
		numberBytesWritten = write(stream->_hFile,
			stream->_buffer_base + numberBytesWrittenTotal,
			numberBytesToWrite - numberBytesWrittenTotal);
		if (numberBytesWritten == -1) {
			PRINT_MY_ERROR("write - fflush");
			return SO_EOF;
		}
		numberBytesWrittenTotal += numberBytesWritten;
	} while (numberBytesWrittenTotal < numberBytesToWrite);
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

#if defined(__linux__)


#elif defined(_WIN32)

static int RedirectHandleWindows(STARTUPINFO* psi, HANDLE hFile, INT opt) {

	HANDLE ret;

	if (hFile == INVALID_HANDLE_VALUE)
		return -1;

	//preluam handlerurile
	ret = GetStdHandle(STD_INPUT_HANDLE);
	if (ret == INVALID_HANDLE_VALUE) {
		PRINT_MY_ERROR("get input handle");
		return -1;
	}
	psi->hStdInput = ret;

	ret = GetStdHandle(STD_OUTPUT_HANDLE);
	if (ret == INVALID_HANDLE_VALUE) {
		PRINT_MY_ERROR("get output handle");
		return -1;
	}
	psi->hStdOutput = ret;

	ret = GetStdHandle(STD_ERROR_HANDLE);
	if (ret == INVALID_HANDLE_VALUE) {
		PRINT_MY_ERROR("get error handle");
		return -1;
	}
	psi->hStdError = ret;

	//sa se paote mosteni
	psi->dwFlags = STARTF_USESTDHANDLES;

	//redirectam unul din ele la hFile
	switch (opt) {
	case STD_INPUT_HANDLE:
		psi->hStdInput = hFile;
		break;
	case STD_OUTPUT_HANDLE:
		psi->hStdOutput = hFile;
		break;
	case STD_ERROR_HANDLE:
		psi->hStdError = hFile;
		break;
	}

	return 0;
}
static HANDLE CreatePipeProcessWindows(const char* command, const char* mode) {

	SECURITY_ATTRIBUTES sa;
	HANDLE hRead, hWrite; //pipe handles

	STARTUPINFO si;				//process info
	PROCESS_INFORMATION pi;
	BOOL bRet;
	int ret;

	//initializam structurile
	ZeroMemory(&sa, sizeof(sa));
	sa.bInheritHandle = TRUE;

	ZeroMemory(&pi, sizeof(pi));
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);


	//creem pipe si redirectam input/output
	bRet = CreatePipe(&hRead, &hWrite, &sa, 0);
	if (bRet == 0) {
		PRINT_MY_ERROR("Create pipe");
		return INVALID_HANDLE_VALUE;
	}

	if (strcmp(mode, "r") == 0) {

		ret = RedirectHandleWindows(&si, hRead, STD_INPUT_HANDLE);
		if (ret == -1) {
			PRINT_MY_ERROR("redirect handle windows");
			return INVALID_HANDLE_VALUE;
		}
	}
	else if (strcmp(mode, "w") == 0) {

		ret = RedirectHandleWindows(&si, hWrite, STD_OUTPUT_HANDLE);
		if (ret == -1) {
			PRINT_MY_ERROR("redirect handle windows");
			return INVALID_HANDLE_VALUE;
		}
	}
	else {
		PRINT_MY_ERROR("wrong mode for pipe");
		return INVALID_HANDLE_VALUE;
	}

	//creem proces cara sa executa commanda
	bRet = CreateProcess(
		NULL,
		command,
		NULL,
		NULL,
		TRUE,
		0,
		NULL,
		NULL,
		&si,
		&pi
	);
	if (bRet == 0) {
		PRINT_MY_ERROR("create proces");
		return INVALID_HANDLE_VALUE;
	}

	if (strcmp(mode, "r") == 0) {

		CloseHandle(hRead);
		return hWrite;
	}
	else if (strcmp(mode, "w") == 0) {

		CloseHandle(hWrite);
		return hRead;
	}

	//DECI
	// Aici creez un nou proces. Ar trebui sa inchid handlerul pentru proces ? CloseHandle(pi.hProces) ???
	// Daca il inchid, nu inseamna ca proceesul copial si a terminat executia ? (ca ar trebui sa astept pana temrina inainte sa ii dau closehandle - Waitforsingleobject)
	//si daca il inchid aici, ce sens mai are handlerul de fisier care il returneaza functia ?



	return INVALID_HANDLE_VALUE;
}
static SO_FILE* OpenPipeProcessWindows(const char* command, const char* mode) {

	SO_FILE* file = NULL;

	HANDLE hFile = CreatePipeProcessWindows(command, mode);
	if (hFile == INVALID_HANDLE_VALUE)
		return NULL;

	file = (SO_FILE*)malloc(sizeof(SO_FILE));


	file->_hFile = hFile;

	file->_buffer_base = (char*)malloc(sizeof(char) * SO_BUFFER_SIZE);
	file->_buffer_end = file->_buffer_base + SO_BUFFER_SIZE;

	file->_read_ptr = file->_buffer_base;
	file->_read_ptr_end = file->_read_ptr;

	file->_write_ptr = file->_buffer_base;
	file->_write_ptr_end = file->_buffer_end;

	file->_feof = SO_FALSE;

	file->_file_pointer_pos = 0;	//pozitia cursorului in fiser!

	if (strcmp("mode", "r") == 0) {

		file->_canRead = SO_TRUE;
		file->_canWrite = SO_FALSE;
		file->_append = SO_FALSE;
		file->_update = SO_FALSE;
	}
	else if (strcmp("mode", "w") == 0) {

		file->_canRead = SO_FALSE;
		file->_canWrite = SO_TRUE;
		file->_append = SO_FALSE;
		file->_update = SO_FALSE;
	}

	return file;
}
#else
#error "Unknown platform"
#endif


