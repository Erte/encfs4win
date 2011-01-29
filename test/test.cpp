#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <windows.h>

static void
check(int line, bool test, const char *chk, const char *fmt, ...)
{
	if (test) return;

	DWORD err = GetLastError();
	va_list ap;

	fprintf(stderr, "error at line %d (last err %u): ", line, (unsigned) err);
	va_start(ap, fmt);
	if (fmt)
		vfprintf(stderr, fmt, ap);
	else
		fprintf(stderr, "test failed was %s", chk);
	va_end(ap);
	fprintf(stderr, "\n");
	exit(1);
}

#define CHECK(test, arg...) do { SetLastError(0); check(__LINE__, test, #test, arg); } while(0)

static const char fn[] = "testfile.txt";
static const char fn2[] = "testfile2.txt";

int main()
{
	HANDLE h1, h2;

	// cleanup
	DeleteFile(fn);

	// create simple test file
	FILE *f = fopen(fn, "w");
	CHECK(f != NULL, "error creating test file");
	fputs("test 123\n", f);
	fclose(f);

	printf("test share\n");
	h1 = CreateFile(fn, FILE_READ_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	CHECK(h1 != INVALID_HANDLE_VALUE, NULL);
	h2 = CreateFile(fn, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	CHECK(h2 != INVALID_HANDLE_VALUE, "second handle should open successfully");
	CloseHandle(h1);
	CloseHandle(h2);

	printf("test share (copy&paste on same directory create this sequence)\n");
	h1 = CreateFile(fn, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	CHECK(h1 != INVALID_HANDLE_VALUE, NULL);
	h2 = CreateFile(fn, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	CHECK(h2 == INVALID_HANDLE_VALUE, "second handle should fail");
	CloseHandle(h1);
	CloseHandle(h2);

	// excel need locking to open a file
	printf("locking should succeed\n");
	h1 = CreateFile(fn, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	CHECK(h1 != INVALID_HANDLE_VALUE, NULL);
	CHECK(LockFile(h1, 0, 0, 10, 0), "lock failed");
	CHECK(!LockFile(h1, 0, 0, 10, 0), "double lock succeeded");
	CHECK(UnlockFile(h1, 0, 0, 10, 0), "unlock failed");
	CHECK(!UnlockFile(h1, 0, 0, 20, 0), "unlock succeeded");

	// check overlapping locks
	CHECK(LockFile(h1, 10, 0, 10, 0), "lock failed");
	CHECK(LockFile(h1, 20, 0, 10, 0), "lock failed");
	CHECK(LockFile(h1, 40, 0, 10, 0), "lock failed");

	// (un)lock that overlap (inside, middle, side, two contiguos)
	CHECK(!LockFile(h1, 15, 0, 3,  0), "lock succeeded inside");
	CHECK(!LockFile(h1, 10, 0, 5,  0), "lock succeeded on left side");
	CHECK(!LockFile(h1, 15, 0, 5,  0), "lock succeeded on right side");
	CHECK(!LockFile(h1, 10, 0, 20, 0), "lock succeeded of 2 contiguos");
	CHECK(!LockFile(h1, 30, 0, 20, 0), "lock succeeded 1+free piece");

	CHECK(!UnlockFile(h1, 15, 0, 3,  0), "unlock succeeded inside");
	CHECK(!UnlockFile(h1, 10, 0, 5,  0), "unlock succeeded on left side");
	CHECK(!UnlockFile(h1, 15, 0, 5,  0), "unlock succeeded on right side");
	CHECK(!UnlockFile(h1, 10, 0, 20, 0), "unlock succeeded of 2 contiguos");
	CHECK(!UnlockFile(h1, 30, 0, 20, 0), "unlock succeeded 1+free piece");

	CHECK(UnlockFile(h1, 20, 0, 10, 0), "unlock failed");
	CHECK(UnlockFile(h1, 10, 0, 10, 0), "unlock failed");
	CHECK(UnlockFile(h1, 40, 0, 10, 0), "unlock failed");
	CloseHandle(h1);

	printf("check a bug with CREATE_ALWAYS\n");
	h1 = CreateFile(fn, FILE_GENERIC_WRITE|FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
	CHECK(h1 != INVALID_HANDLE_VALUE, "create failed");
	CloseHandle(h1);

	printf("rename to an open file\n");
	h1 = CreateFile(fn2, SYNCHRONIZE|FILE_WRITE_DATA, FILE_SHARE_WRITE|FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
	CHECK(h1 != INVALID_HANDLE_VALUE, "create failed");
	CHECK(!MoveFile(fn, fn2), "move succeeded");
	CloseHandle(h1);
	DeleteFile(fn2);

	printf("deleting while open\n");
	h1 = CreateFile(fn, FILE_READ_ATTRIBUTES, FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
	CHECK(h1 != INVALID_HANDLE_VALUE, NULL);
	CHECK(DeleteFile(fn), "failed deleting file");
	WIN32_FIND_DATA wfd;
	h2 = FindFirstFile(fn, &wfd);
	CHECK(h2 != INVALID_HANDLE_VALUE, NULL);
	FindClose(h2);
	CloseHandle(h1);
	CHECK(FindFirstFile(fn, &wfd) == INVALID_HANDLE_VALUE, NULL);

	printf("all done!\n");

	return 0;
}

