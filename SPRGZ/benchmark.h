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

// Структура конфига, представляет из себя все поля настроек для тестирования диска в приложении
struct Config {
    DWORD bufferSize;
    DWORD mode;
    DWORD32 fileSize;
    const TCHAR* disk;
    DWORD countTests;
};
extern Config userConfig;

DWORD WINAPI writeTest(LPVOID param); // Тест на запись
void readTest(); // Тест на чтение
int getModeFromType(const char* type);

