#include <windows.h>
#include <tchar.h>
#include <string.h>
#include <stdio.h>
#include <strsafe.h>
#include <utility>   

#define KB 1024
#define MB KB*1024
#define GB MB*1024
#define RESULT pair <DWORD, DOUBLE>

using namespace std;

struct Config {
    int bufferSize;
    int type;
    const char* diskPath;
};
extern Config userConfig;

void writeTest();
RESULT writeToFile(HANDLE, DWORD);
void readTest();
int getModeFromType(const char* type);

