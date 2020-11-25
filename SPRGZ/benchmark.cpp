#include <windows.h>
#include <tchar.h>
#include <string.h>
#include <stdio.h>
#include <strsafe.h>
#include <utility>
#include <CommCtrl.h>
#include <math.h>
#include "GUIMain.h"
#include "benchmark.h"

using namespace std;

// Структура конфига, представляет из себя все поля настроек для тестирования диска в приложении 
Config testConfig;

CONST DWORD BUFFER_SIZES[] = { 1 * KB, 4 * KB, 8 * KB, 1 * MB, 2 * MB, 4 * MB, 8 * MB, 16 * MB }; 
CONST DWORD FILE_SIZS[] = { 128 * MB, 256 * MB, 512 * MB, 1024 * MB, 2048 * MB };

DWORD WINAPI testDrive(LPVOID  param) {
    testConfig = *((Config*)param); // id родительского потока

    DOUBLE totalTime = 0, totalmb = 0;

    // Определение полного пути к файлу
    TCHAR fullPath[20] = _T("");
    _tcscat_s(fullPath, testConfig.disk);
    _tcscat_s(fullPath, _T("test.tmp"));

    // Создание хедлера на файл в зависимости от типа теста
    HANDLE file;
    DWORD typeAccess, typeOpen;

    if (testConfig.typeTest == READ_TEST) {
        createTestFile(fullPath);
        typeAccess = GENERIC_READ;
        typeOpen = OPEN_ALWAYS;
    }
    else {
        typeAccess = GENERIC_WRITE;
        typeOpen = CREATE_ALWAYS;
    }

    for (DWORD i = 0; i < testConfig.countTests; i++)
    {
        file = CreateFile(fullPath,
            typeAccess,
            0,
            NULL,
            typeOpen,
            testConfig.mode | FILE_FLAG_NO_BUFFERING,
            NULL);


        // Проверка но то, определен ли хендлер
        if (file == INVALID_HANDLE_VALUE) {
            _tprintf(_T("Terminal failure: Unable to create file for write with error code %d.\n"), GetLastError());
            CloseHandle(file);
            PostThreadMessage(testConfig.parentThreadId, SEND_PROGRESS_BAR_UPDATE, 0, (LPARAM)new int(0));
            return NULL;
        }

        // Запуск записи в файл и подсчет результата (test.first - количество записаных мегбайт, test.second - количество затраченого времени)
        RESULT test = testIteration(file, i);

        //отслеживаем ошибки
        if (test.first == NULL) {
            _tprintf(_T("Terminal failure: Unable to write to file with error code %d.\n"), GetLastError());
            CloseHandle(file);
            PostThreadMessage(testConfig.parentThreadId, SEND_PROGRESS_BAR_UPDATE, 0, (LPARAM)new int(0));
            DeleteFile(fullPath);
            return -1;
        }

        // Подсчет суммарного количества записаных байт и затраченого времени
        totalmb += test.first;
        totalTime += test.second;

        CloseHandle(file);
    }
    DeleteFile(fullPath);

    // Вывод информации про результаты тестирования
    totalmb = ((totalmb / (DOUBLE)1024)) / (DOUBLE)1024; // Перевод в мегабайты

    TCHAR str[20];
    DOUBLE res = (DOUBLE)totalmb / totalTime;
    _stprintf_s(str, _T("%.2lf"), res);
    _tcscat_s(str, _T(" МБ\\с"));
    Sleep(800);

    PostThreadMessage(testConfig.parentThreadId, SEND_TEST_RESULT, 0, (LPARAM)str);
    Sleep(100);
}

RESULT testIteration(HANDLE file, DWORD iteration) {
    // Для подсчет времени
    LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
    LARGE_INTEGER Frequency;
    BOOL bErrorFlag; // Ошибки записи/чтения в файл
     
    //тестовый массив данных
    TCHAR* DataBuffer = new TCHAR[testConfig.bufferSize];

    if (testConfig.typeTest == WRITE_TEST) {
        TCHAR Data[] = _T("NoName");
        DWORD nameDivider = sizeof(Data) / sizeof(Data[0]);
        for (DWORD i = 0; i < testConfig.bufferSize; i++)
            DataBuffer[i] = Data[i % nameDivider];
    }

    DOUBLE totalTime = 0;
    DWORD iterations = (DWORD)(testConfig.fileSize / testConfig.bufferSize) + 1;
    DWORD dwBytesToProcess = testConfig.bufferSize * iterations;
    DWORD dwBytesProcess = 0, sumBytesProcess = 0;

    //начинаем отсчет времени
    QueryPerformanceFrequency(&Frequency);

    // Для отображение прогресса на прогресс баре
    DWORD curProgress = (iterations * iteration * 100);
    DWORD allSteps = iterations * testConfig.countTests;

    DOUBLE* buffersTimes = new DOUBLE[iterations];
    DWORD pbStateCurrent, pbStateLast = 0; // Состояния прогресс бара
    //записываем в файл count раз массива данных
    for (DWORD i = 0; i < iterations; ++i)
    {
        if (threadStatus == CANCELED)
            return make_pair(NULL, NULL);
        
        if (testConfig.typeTest == WRITE_TEST) {
            QueryPerformanceCounter(&StartingTime);
            bErrorFlag = WriteFile(
                file,
                DataBuffer,
                testConfig.bufferSize,
                &dwBytesProcess,
                NULL);
            QueryPerformanceCounter(&EndingTime);
        }
        else {
            QueryPerformanceCounter(&StartingTime);
            bErrorFlag = ReadFile(
                file,
                DataBuffer,
                testConfig.bufferSize,
                &dwBytesProcess,
                NULL);
            QueryPerformanceCounter(&EndingTime);
        }

        // Если возникла ошибка при тестировании - возвращается NULL
        if (bErrorFlag == FALSE) 
            return make_pair(NULL, NULL);

        //подсчитываем время
        ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
        ElapsedMicroseconds.QuadPart *= 1000000;
        ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;

        //Пока что вывод итераций в консоль 
        buffersTimes[i] = ElapsedMicroseconds.QuadPart / (double)1000000;
        sumBytesProcess += dwBytesProcess;
        totalTime += (ElapsedMicroseconds.QuadPart / (double)1000000);

        //установка прогресса - если текущий процент прогресса (целое число) изменился по сравнению с прошлым - происходит изменение прогресс бара
        pbStateCurrent = ((100 * (i + 1) + curProgress) / allSteps);
        if (pbStateCurrent > pbStateLast) {
            PostThreadMessage(testConfig.parentThreadId, SEND_PROGRESS_BAR_UPDATE, 0, (LPARAM)&pbStateCurrent);
            pbStateLast = pbStateCurrent; // Текущее состояние для дальнейшей операции становится прошлым
        }
    }

    // Если количество обработаных байт не совпадает с необходимым - возникла ошибка
    if (sumBytesProcess != dwBytesToProcess)
    {
        _tprintf(_T("Error: dwBytesWritten != dwBytesToWrite\n"));
        return make_pair(NULL, NULL);
    }


    SaveResults(buffersTimes, iterations, iteration);
    return make_pair(sumBytesProcess, totalTime);
}

VOID SaveResults(DOUBLE* arr, DWORD size, DWORD iteration) {
    DWORD dwTemp;
    
    TCHAR fileName[15];
    if (testConfig.typeTest == WRITE_TEST)
        sprintf(fileName, "WriteTest%d.txt", iteration);
    else
        sprintf(fileName, "ReadTest%d.txt", iteration);


    HANDLE hFile = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hFile) return;

    DWORD delta = 0;
    for (DWORD i = 1; i <= size; i++)
    {
        DWORD sizeBuff = log10(i) + 11; // 11 символов на число с плавающей запятой и перенос каретки
        TCHAR* buffer = new TCHAR[sizeBuff];
        sprintf(buffer, "%d %1.6lf\n", i, arr[i]);
        WriteFile(hFile, buffer, sizeBuff * sizeof(TCHAR), &dwTemp, NULL);
    }

    CloseHandle(hFile);
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
	TCHAR* DataBuffer = new TCHAR[testConfig.bufferSize];
    TCHAR Data[] = _T("NoName");
    DWORD divider = sizeof(Data) / sizeof(Data[0]);
    for (DWORD i = 0; i < testConfig.bufferSize; i++)
        DataBuffer[i] = Data[i % divider];

    DWORD iterations = (DWORD)(testConfig.fileSize / testConfig.bufferSize) + 1;
    DWORD dwBytesToWrite = testConfig.bufferSize * iterations;
    DWORD dwBytesWritten = 0, sumWritten = 0;

    DOUBLE* buffersTimes = new DOUBLE[iterations];

    //записываем в файл count раз массива данных
    for (DWORD i = 0; i < iterations; ++i)
    {
        bErrorFlag = WriteFile(
            testFile,
            DataBuffer,
            testConfig.bufferSize,
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

// Преобразование строки в аргумент флага (Потому что в меню берется тектовое поле)
DWORD getModeFromType(CONST TCHAR* type) {
    if (!_tcscmp(type, "RANDOM_ACCESS"))
        return FILE_FLAG_RANDOM_ACCESS;
    else if (!_tcscmp(type, "WRITE_THROUGH"))
        return FILE_FLAG_WRITE_THROUGH;
    else if (!_tcscmp(type, "SEQUENTIAL"))
        return FILE_FLAG_SEQUENTIAL_SCAN;
}