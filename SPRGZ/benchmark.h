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


using namespace std;

// ��������� �������, ������������ �� ���� ��� ���� �������� ��� ������������ ����� � ����������


DWORD testDrive(LPVOID);
RESULT testIteration(HANDLE, DWORD);
DWORD getModeFromType(CONST TCHAR*);
VOID SaveResults(DOUBLE*, DWORD, DWORD);
VOID createTestFile(TCHAR[]);
