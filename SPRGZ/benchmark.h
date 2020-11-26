#include <windows.h>
#include <tchar.h>
#include <string.h>
#include <stdio.h>
#include <strsafe.h>
#include <utility>   
#include <CommCtrl.h>

#define KB 1024
#define MB KB*1024
#define GB MB*1024
#define RESULT pair <DWORD, DOUBLE>

#define WRITE_TEST 50
#define READ_TEST 51

CONST DWORD SIZE_OF_HISTOGRAM = 6;

using namespace std;

// —труктура конфига, представл€ет из себ€ все пол€ настроек дл€ тестировани€ диска в приложении


DWORD testDrive(LPVOID);
RESULT testIteration(HANDLE, DWORD);
DWORD getModeFromType(CONST TCHAR*);
VOID createTestFile(TCHAR[]);
VOID saveResultsGraph(DOUBLE*, DWORD, TCHAR*);
VOID saveResultsOfHistogram(DWORD*, DWORD, TCHAR*);
VOID GetFilledArray(TCHAR*);
VOID filledBuffer(TCHAR*, DWORD);