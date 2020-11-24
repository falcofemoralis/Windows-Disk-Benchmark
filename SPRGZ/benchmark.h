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
    CONST TCHAR* disk;
    DWORD countTests;
};
extern Config userConfig;

DWORD WINAPI writeTest(LPVOID param); // Тест на запись
DWORD WINAPI readTest(LPVOID param); // Тест на чтение
INT getModeFromType(CONST TCHAR* type);

