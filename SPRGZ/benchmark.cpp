#include <windows.h>
#include <tchar.h>
#include <string.h>
#include <stdio.h>
#include <strsafe.h>
#include <utility>
#include <CommCtrl.h>
#include "GUIMain.h"

#define KB 1024
#define MB KB*1024
#define GB MB*1024
#define RESULT pair <DWORD, DOUBLE>

using namespace std;

// Структура конфига, представляет из себя все поля настроек для тестирования диска в приложении
struct Config {
    DWORD bufferSize;
    DWORD mode;
    DWORD fileSize;
    const TCHAR* disk;
    DWORD countTests;
};
Config userConfig;

const DWORD BUFFER_SIZES[] = { 1 * KB, 4 * KB, 8 * KB, 1 * MB, 2 * MB, 4 * MB, 8 * MB, 16 * MB }; //16 по варианту
const DWORD FILE_SIZS[] = { 128 * MB, 256 * MB, 512 * MB, 1024 * MB };

DWORD WINAPI writeTest(LPVOID param);
RESULT writeToFile(HANDLE, DWORD, DWORD);
DWORD WINAPI readTest(HANDLE, TCHAR *);
RESULT readFileFunc(HANDLE, DWORD, DWORD);
void ExitTestThread(HANDLE& writeFile);

// ДЛЯ ТЕСТА, если необходимо что то затестить без UI, снимаете комменты с этого мейна и комментите в GUI

//int __cdecl _tmain(int argc, TCHAR* argv[])
//{
//    userConfig.bufferSize = 16 * MB;
//    userConfig.mode = FILE_FLAG_RANDOM_ACCESS;
//    userConfig.countTests = 5;
//    userConfig.disk = "C:\\";
//    writeTest();
//    //readTest();
//    system("Pause");
//} 

DWORD WINAPI writeTest(LPVOID param) {

    puts("Started");
    // Определение полного пути к файлу
    TCHAR fullPath[20] = _T("");
    _tcscat_s(fullPath, userConfig.disk);
    _tcscat_s(fullPath, TEXT("test.bin"));

    //создаем файл "test.bin", после закрытия хендла файл будет удален
    HANDLE writeFile = CreateFile(fullPath,
        GENERIC_WRITE | GENERIC_READ,
        0,
        NULL,
        CREATE_ALWAYS,
        userConfig.mode | FILE_FLAG_NO_BUFFERING | FILE_FLAG_DELETE_ON_CLOSE, //FILE_FLAG_DELETE_ON_CLOSE
        NULL);

    //если файл не создался
    if (writeFile == INVALID_HANDLE_VALUE) {
        _tprintf(TEXT("Terminal failure: Unable to create file for write with error code %d.\n"), GetLastError());
        ExitTestThread(writeFile);
        return NULL;
    }

    DOUBLE totalTime = 0, totalmb = 0;

    _tprintf(TEXT("Testing %s with buffer size %d kb, file size %d kb, mode %d, countsTests %d\n"),
        fullPath,
        userConfig.bufferSize / 1024,
        userConfig.fileSize / 1024, 
        userConfig.mode ,
        userConfig.countTests);
    for (DWORD i = 0; i < userConfig.countTests; i++)
    {
        // Запуск записи в файл и подсчет результата (test.first - количество записаных мегбайт, test.second - количество затраченого времени)
        RESULT test = writeToFile(writeFile, userConfig.bufferSize, i);

        //отслеживаем ошибки
        if (test.first == NULL) {
            _tprintf(TEXT("Terminal failure: Unable to write to file with error code %d.\n"), GetLastError());
            ExitTestThread(writeFile);
        }

        // Подсчет суммарного количества записаных байт и затраченого времени
        totalmb += test.first;
        totalTime += test.second;
    }

    //выводим информацию про результаты тестирования
    totalmb = ((totalmb / (DOUBLE)1024)) / (DOUBLE)1024; // Перевод в мегабайты

    TCHAR str[20];
    DOUBLE res = (DOUBLE)totalmb / totalTime;
    _stprintf_s(str, _T("%.2lf"), res);
    _tcscat_s(str, TEXT(" МБ\\с"));
    Sleep(800);
    SetWindowText(text_write, str);
    
    readTest(writeFile, fullPath);
    puts("Ended Write Test");
}

RESULT writeToFile(HANDLE writeFile, DWORD buffer_size, DWORD iter) {
    LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
    LARGE_INTEGER Frequency;
    BOOL bErrorFlag;

    //тестовый массив данных
    char* DataBuffer = new char[buffer_size];
    for (int i = 0; i < buffer_size; i++)
        DataBuffer[i] = 't';

    DOUBLE totalTime = 0;
    DWORD iterations = userConfig.fileSize / buffer_size;
    DWORD dwBytesToWrite = buffer_size * iterations;
    DWORD dwBytesWritten = 0, sumWritten = 0;

    //начинаем отсчет времени
    QueryPerformanceFrequency(&Frequency);

    // Для отображение прогресса на прогресс баре
    DWORD curProgress = (iterations * iter * 100);
    DWORD allSteps = iterations * userConfig.countTests;

    //записываем в файл count раз массива данных
    for (int i = 0; i < iterations; ++i)
    {
        if (threadStatus == CANCELED)
            ExitTestThread(writeFile);
        
        puts("Working");
        QueryPerformanceCounter(&StartingTime);

        bErrorFlag = WriteFile(
            writeFile,
            DataBuffer,
            buffer_size,
            &dwBytesWritten,
            NULL);

        QueryPerformanceCounter(&EndingTime);

        if (bErrorFlag == FALSE) return make_pair(NULL, NULL);

        //подсчитываем время
        ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
        ElapsedMicroseconds.QuadPart *= 1000000;
        ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;

        sumWritten += dwBytesWritten;
        totalTime += (ElapsedMicroseconds.QuadPart / (double)1000000);

        //установка прогресса
        SendMessage(pb_progress, PBM_SETPOS, ((100 * (i + 1) + curProgress) / allSteps), 0);
    }

    if (sumWritten != dwBytesToWrite)
    {
        _tprintf(TEXT("Error: dwBytesWritten != dwBytesToWrite\n"));
        return make_pair(NULL, NULL);
    }
    return make_pair(sumWritten, totalTime);
}

void ExitTestThread(HANDLE& writeFile) {
    SendMessage(pb_progress, PBM_SETPOS, 0, 0);
    CloseHandle(writeFile);
    EnableWindow(btn_start, true);
    ExitThread(0);
}

DWORD WINAPI readTest(HANDLE writeFile, TCHAR fullPath[]) {

    puts("Started Read Test");   

    DOUBLE totalTime = 0, totalmb = 0;
    
    //Вывод информации о тесте
    _tprintf(TEXT("Testing %s with buffer size %d kb, file size %d kb, mode %d, countsTests %d\n"),
        fullPath,
        userConfig.bufferSize / 1024,
        userConfig.fileSize / 1024,
        userConfig.mode,
        userConfig.countTests);
    for (DWORD i = 0; i < userConfig.countTests; i++)
    {
        // Запуск записи в файл и подсчет результата (test.first - количество записаных мегбайт, test.second - количество затраченого времени)
        RESULT test = readFileFunc(writeFile, userConfig.bufferSize, i);

        //отслеживаем ошибки
        if (test.first == NULL) {
            _tprintf(TEXT("Terminal failure: Unable to read file with error code %d.\n"), GetLastError());
            ExitTestThread(writeFile);
        }

        // Подсчет суммарного количества записаных байт и затраченого времени
        totalmb += test.first;
        totalTime += test.second;
    }

    //выводим информацию про результаты тестирования
    totalmb = ((totalmb / (DOUBLE)1024)) / (DOUBLE)1024; // Перевод в мегабайты

    TCHAR str[20];
    DOUBLE res = (DOUBLE)totalmb / totalTime;
    _stprintf_s(str, _T("%.2lf"), res);
    _tcscat_s(str, TEXT(" МБ\\с"));
    Sleep(800);
    SetWindowText(text_read, str);

    ExitTestThread(writeFile);
    puts("Ended");
    return 0;
}

RESULT readFileFunc(HANDLE readFile, DWORD buffer_size, DWORD iter) {

    SetFilePointer(readFile,NULL, NULL, FILE_BEGIN);
    LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
    LARGE_INTEGER Frequency;
    BOOL bErrorFlag=1;

    char* DataBuffer = new char[buffer_size];

    DOUBLE totalTime = 0;
    DWORD iterations = userConfig.fileSize / buffer_size;
    DWORD dwBytesToRead = buffer_size * iterations;
    DWORD dwBytesRead = 0, sumRead = 0;

    //начинаем отсчет времени
    QueryPerformanceFrequency(&Frequency);

    // Для отображение прогресса на прогресс баре
    DWORD curProgress = (iterations * iter * 100);
    DWORD allSteps = iterations * userConfig.countTests;

    //читаем файл count раз массива данных
    for (int i = 0; i < iterations; ++i)
    {
        if (threadStatus == CANCELED)
            ExitTestThread(readFile);

        puts("Working");
        QueryPerformanceCounter(&StartingTime);

        bErrorFlag = ReadFile(
            readFile,
            DataBuffer,
            buffer_size,
            &dwBytesRead,
            NULL);

        QueryPerformanceCounter(&EndingTime);

        if (bErrorFlag == FALSE) return make_pair(NULL, NULL);

        //подсчитываем время
        ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
        ElapsedMicroseconds.QuadPart *= 1000000;
        ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;

        sumRead += dwBytesRead;
        totalTime += (ElapsedMicroseconds.QuadPart / (double)1000000);

        //установка прогресса
        SendMessage(pb_progress, PBM_SETPOS, ((100 * (i + 1) + curProgress) / allSteps), 0);
    }

    //Проверяем, чтобы количество считанных байт было равно количеству, заявленному тестом
    if (sumRead != dwBytesToRead)
    {
        _tprintf(TEXT("Error: sumRead != dwBytesToRead\n"));
        return make_pair(NULL, NULL);
    }
    return make_pair(sumRead, totalTime);
}

// Преобразование строки в аргумент флага (Потому что в меню берется тектовое поле)
int getModeFromType(const char* type) {
    if (!strcmp(type, "RANDOM_ACCESS"))
        return FILE_FLAG_RANDOM_ACCESS;
    else if (!strcmp(type, "WRITE_THROUGH"))
        return FILE_FLAG_WRITE_THROUGH;
    else if (!strcmp(type, "SEQUENTIAL"))
        return FILE_FLAG_SEQUENTIAL_SCAN;
    else if (!strcmp(type, "NO_BUFFERING"))
        return FILE_FLAG_NO_BUFFERING;
}