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

using namespace std;

// —труктура конфига, представл€ет из себ€ все пол€ настроек дл€ тестировани€ диска в приложении
struct Config {
    DWORD bufferSize;
    DWORD mode;
    DWORD fileSize;
    const TCHAR* disk;
    DWORD countTests;
};
extern Config userConfig;

DWORD WINAPI writeTest(LPVOID);
RESULT writeToFile(HANDLE, DWORD, DWORD);
DWORD WINAPI readTest(LPVOID);
RESULT readFromFile(HANDLE, DWORD, DWORD);
VOID ExitTestThread(HANDLE&);
VOID SaveResults(DOUBLE*, DWORD, TCHAR[]);
VOID createTestFile(TCHAR[]);
DWORD getModeFromType(const TCHAR* type);
