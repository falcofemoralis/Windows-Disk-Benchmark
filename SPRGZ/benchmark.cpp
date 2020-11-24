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

CONST DWORD BUFFER_SIZES[] = { 1 * KB, 4 * KB, 8 * KB, 1 * MB, 2 * MB, 4 * MB, 8 * MB, 16 * MB }; 
CONST DWORD FILE_SIZS[] = { 128 * MB, 256 * MB, 512 * MB, 1024 * MB, 2048 * MB };

DWORD WINAPI writeTest(LPVOID);
RESULT writeToFile(HANDLE, DWORD);
DWORD WINAPI readTest(LPVOID);
RESULT readFromFile(HANDLE, DWORD, DWORD);
VOID ExitTestThread(HANDLE&);
VOID SaveResults(DOUBLE*, DWORD, TCHAR[]);
VOID createTestFile(TCHAR[]);

VOID SaveResults(DOUBLE* arr, DWORD size, TCHAR lpFileName[]) {
    DWORD dwTemp;
    
    HANDLE hFile = CreateFile(lpFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hFile) return;

    DWORD delta = 0;
    for (DWORD i = 0; i < size; i++)
    {
        if (i % 10 == 0) delta++;
        DWORD size = 10 + delta;
        TCHAR* buffer = new TCHAR[size];
        sprintf(buffer, "%d %lf\n", i, arr[i]);
        WriteFile(hFile, buffer, size, &dwTemp, NULL);

    }

    CloseHandle(hFile);
}

VOID ExitTestThread(HANDLE& handle) {
    SendMessage(pb_progress, PBM_SETPOS, 0, 0);
    CloseHandle(handle);
    EnableWindow(btn_startWrite, true);
    EnableWindow(btn_startRead, true);
    return;
}

DWORD WINAPI writeTest(LPVOID  param) {

    DWORD parentThreadId = *((DWORD*)param); // id родительского потока

    DOUBLE totalTime = 0, totalmb = 0;

    // Определение полного пути к файлу
    TCHAR fullPath[20] = _T("");
    _tcscat_s(fullPath, userConfig.disk);
    _tcscat_s(fullPath, _T("test.tmp"));

    //создаем файл "test.bin", после закрытия хендла файл будет удален
    HANDLE writeFile = CreateFile(fullPath,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        userConfig.mode | FILE_FLAG_NO_BUFFERING | FILE_FLAG_DELETE_ON_CLOSE, //FILE_FLAG_DELETE_ON_CLOSE
        NULL);

    //если файл не создался
    if (writeFile == INVALID_HANDLE_VALUE) {
        _tprintf(_T("Terminal failure: Unable to create file for write with error code %d.\n"), GetLastError());
        ExitTestThread(writeFile);
        return NULL;
    }

    _tprintf(_T("Testing %s with buffer size %d kb, file size %d kb, mode %d, countsTests %d\n"),
        fullPath,
        userConfig.bufferSize / 1024,
        userConfig.fileSize / 1024, 
        userConfig.mode ,
        userConfig.countTests);

    for (DWORD i = 0; i < userConfig.countTests; i++)
    {
        // Запуск записи в файл и подсчет результата (test.first - количество записаных мегбайт, test.second - количество затраченого времени)
        RESULT test = writeToFile(writeFile, i);

        //отслеживаем ошибки
        if (test.first == NULL) {
            _tprintf(_T("Terminal failure: Unable to write to file with error code %d.\n"), GetLastError());
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
    _tcscat_s(str, _T(" МБ\\с"));
    Sleep(800);
    PostThreadMessage(parentThreadId, SEND_TEST_RESULT, 0, (LPARAM)str);

    ExitTestThread(writeFile);
}

RESULT writeToFile(HANDLE writeFile, DWORD countTest) {
    LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
    LARGE_INTEGER Frequency;
    BOOL bErrorFlag;

    //тестовый массив данных
    char* DataBuffer = new char[userConfig.bufferSize];
    TCHAR Data[] = _T("Vladyslav");

    for (int i = 0; i < userConfig.bufferSize; i++)
        DataBuffer[i] = Data[i % 9];

    DOUBLE totalTime = 0;
    DWORD iterations = (DWORD) (userConfig.fileSize / userConfig.bufferSize) + 1;
    DWORD dwBytesToWrite = userConfig.bufferSize * iterations;
    DWORD dwBytesWritten = 0, sumWritten = 0;

    //начинаем отсчет времени
    QueryPerformanceFrequency(&Frequency);

    // Для отображение прогресса на прогресс баре
    DWORD curProgress = (iterations * countTest * 100);
    DWORD allSteps = iterations * userConfig.countTests;

    DOUBLE *buffersTimes = new DOUBLE[iterations];

    //записываем в файл count раз массива данных
    for (int i = 0; i < iterations; ++i)
    {
        if (threadStatus == CANCELED)
            ExitTestThread(writeFile);
        
        QueryPerformanceCounter(&StartingTime);

        bErrorFlag = WriteFile(
            writeFile,
            DataBuffer,
            userConfig.bufferSize,
            &dwBytesWritten,
            NULL);

        QueryPerformanceCounter(&EndingTime);

        if (bErrorFlag == FALSE) return make_pair(NULL, NULL);

        //подсчитываем время
        ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
        ElapsedMicroseconds.QuadPart *= 1000000;
        ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;

		//Пока что вывод итераций в консоль
        buffersTimes[i] = ElapsedMicroseconds.QuadPart / (double)1000000;
        sumWritten += dwBytesWritten;
        totalTime += (ElapsedMicroseconds.QuadPart / (double)1000000);

        //установка прогресса
        SendMessage(pb_progress, PBM_SETPOS, ((100 * (i + 1) + curProgress) / allSteps), 0);
    }

    if (sumWritten != dwBytesToWrite)
    {
        _tprintf(_T("Error: dwBytesWritten != dwBytesToWrite\n"));
        return make_pair(NULL, NULL);
    }

    TCHAR fileName[15];
    sprintf(fileName, "WriteTest%d.txt", countTest);
    SaveResults(buffersTimes, iterations, fileName);
    return make_pair(sumWritten, totalTime);
}


VOID createTestFile(TCHAR fullPath[]) {
    //создаем файл "test.bin", после закрытия хендла файл будет удален
    HANDLE testFile = CreateFile(fullPath,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        NULL,
        NULL);


    BOOL bErrorFlag;

    //тестовый массив данных
    char* DataBuffer = new char[userConfig.bufferSize];
    TCHAR Data[] = _T("NoName");

    for (int i = 0; i < userConfig.bufferSize; i++)
        DataBuffer[i] = Data[i % 9];

    DWORD iterations = (DWORD)(userConfig.fileSize / userConfig.bufferSize) + 1;
    DWORD dwBytesToWrite = userConfig.bufferSize * iterations;
    DWORD dwBytesWritten = 0, sumWritten = 0;

    DOUBLE* buffersTimes = new DOUBLE[iterations];

    //записываем в файл count раз массива данных
    for (int i = 0; i < iterations; ++i)
    {
        bErrorFlag = WriteFile(
            testFile,
            DataBuffer,
            userConfig.bufferSize,
            &dwBytesWritten,
            NULL);
        if (bErrorFlag == FALSE) return;

        sumWritten += dwBytesWritten;
    }

    
    if (sumWritten != dwBytesToWrite)
    {
        _tprintf(_T("Error: dwBytesWritten != dwBytesToWrite\n"));
        return;
    }      
    
    CloseHandle(testFile);
}

DWORD WINAPI readTest(LPVOID param) {
    DOUBLE totalTime = 0, totalmb = 0;
    
    TCHAR fullPath[20] = _T("");
    _tcscat_s(fullPath, userConfig.disk);
    _tcscat_s(fullPath, _T("test.tmp"));

    createTestFile(fullPath);

    //создаем файл "test.bin", после закрытия хендла файл будет удален
    HANDLE readFile = CreateFile(fullPath,
        GENERIC_READ,
        0,
        NULL,
        OPEN_ALWAYS,
        userConfig.mode | FILE_FLAG_NO_BUFFERING | FILE_FLAG_DELETE_ON_CLOSE, //FILE_FLAG_DELETE_ON_CLOSE
        NULL);

    //если файл не создался
    if (readFile == INVALID_HANDLE_VALUE) {
        _tprintf(_T("Terminal failure: Unable to create file for write with error code %d.\n"), GetLastError());
        ExitTestThread(readFile);
        return NULL;
    }

    //Вывод информации о тесте
    _tprintf(_T("Testing %s with buffer size %d kb, file size %d kb, mode %d, countsTests %d\n"),
        fullPath,
        userConfig.bufferSize / 1024,
        userConfig.fileSize / 1024,
        userConfig.mode,
        userConfig.countTests);

    for (DWORD i = 0; i < userConfig.countTests; i++)
    {
        // Запуск записи в файл и подсчет результата (test.first - количество записаных мегбайт, test.second - количество затраченого времени)
        RESULT test = readFromFile(readFile, userConfig.bufferSize, i);

        //отслеживаем ошибки
        if (test.first == NULL) {
            _tprintf(_T("Terminal failure: Unable to read file with error code %d.\n"), GetLastError());
            ExitTestThread(readFile);
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
    _tcscat_s(str, _T(" МБ\\с"));
    Sleep(800);
    SetWindowText(text_read, str);

    ExitTestThread(readFile);
    return 0;
}

RESULT readFromFile(HANDLE readFile, DWORD buffer_size, DWORD countTest) {

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
    DWORD curProgress = (iterations * countTest * 100);
    DWORD allSteps = iterations * userConfig.countTests;

    DOUBLE* buffersTimes = new DOUBLE[iterations];

    //читаем файл count раз массива данных
    for (int i = 0; i < iterations; ++i)
    {
        if (threadStatus == CANCELED)
            ExitTestThread(readFile);

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

        buffersTimes[i] = ElapsedMicroseconds.QuadPart / (double)1000000;
        sumRead += dwBytesRead;
        totalTime += (ElapsedMicroseconds.QuadPart / (double)1000000);

        //установка прогресса
        SendMessage(pb_progress, PBM_SETPOS, ((100 * (i + 1) + curProgress) / allSteps), 0);
    }

    //Проверяем, чтобы количество считанных байт было равно количеству, заявленному тестом
    if (sumRead != dwBytesToRead)
    {
        _tprintf(_T("Error: sumRead != dwBytesToRead\n"));
        return make_pair(NULL, NULL);
    }

    TCHAR fileName[15];
    sprintf(fileName, "ReadTest%d.txt", countTest);
    SaveResults(buffersTimes, iterations, fileName);
    return make_pair(sumRead, totalTime);
}

// Преобразование строки в аргумент флага (Потому что в меню берется тектовое поле)
DWORD getModeFromType(const TCHAR* type) {
    if (!_tcscmp(type, "RANDOM_ACCESS"))
        return FILE_FLAG_RANDOM_ACCESS;
    else if (!_tcscmp(type, "WRITE_THROUGH"))
        return FILE_FLAG_WRITE_THROUGH;
    else if (!_tcscmp(type, "SEQUENTIAL"))
        return FILE_FLAG_SEQUENTIAL_SCAN;
}