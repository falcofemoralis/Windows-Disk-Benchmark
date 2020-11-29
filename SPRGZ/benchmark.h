#include <windows.h>
#include <tchar.h>
#include <string.h>
#include <stdio.h>
#include <strsafe.h>
#include <CommCtrl.h>
#include <corecrt_math.h>
#include <algorithm>

#define KB 1024
#define MB KB*1024
#define GB MB*1024
#define RESULT pair

#define WRITE_TEST 50
#define READ_TEST 51

#define TYPE_GRAPH 0
#define TYPE_HISTOGRAM 1

// Аналог структуры пары из С++ (правда не шаблон)
struct pair {
	DWORD first;
	DOUBLE second;
};

DWORD testDrive(LPVOID);
RESULT testIteration(HANDLE, DWORD);
DWORD getModeFromType(CONST TCHAR*);
VOID createTestFile(TCHAR*);
VOID saveResults(DOUBLE*, TCHAR*, DWORD, DWORD);
VOID fillBuffer(TCHAR*, DWORD);
pair make_pair(DWORD, DOUBLE);