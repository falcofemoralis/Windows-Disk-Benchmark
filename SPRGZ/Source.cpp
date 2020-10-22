#include <windows.h>
#include <tchar.h>
#include <string.h>
#include <stdio.h>
#include <strsafe.h>

#define BUFFER_SIZE 99999
#define COUNT 10000

int __cdecl _tmain(int argc, TCHAR* argv[])
{
    LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
    LARGE_INTEGER Frequency;
    HANDLE hFile;

    //тестовый массив данных
    char DataBuffer[BUFFER_SIZE];
    for(int i = 0; i < BUFFER_SIZE; i++) {
        DataBuffer[i] = 't';
    }

    DWORD dwBytesToWrite = (DWORD)strlen(DataBuffer);
    DWORD dwBytesWritten = 0;
    BOOL bErrorFlag = FALSE;

    //создаем файл
    hFile = CreateFile("test.bin",              
        GENERIC_WRITE,         
        0,                     
        NULL,                  
        CREATE_NEW,             
        FILE_FLAG_DELETE_ON_CLOSE, 
        NULL);                 

    //в случае ошибки создания файла
    if (hFile == INVALID_HANDLE_VALUE)
    {
        _tprintf(TEXT("Terminal failure: Unable to create file for write with error code %d.\n"), GetLastError());
        return 0;
    }

    //запускаем тест
    _tprintf(TEXT("Testing...\n"));
    QueryPerformanceFrequency(&Frequency);
    QueryPerformanceCounter(&StartingTime);

    //записывает count раз в файл
    for (int i = 0; i < COUNT;i++) {
        bErrorFlag = WriteFile(
            hFile,          
            DataBuffer,      
            dwBytesToWrite,  
            &dwBytesWritten, 
            NULL);           
    }
  
    QueryPerformanceCounter(&EndingTime);

    //измеряем время
    ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
    ElapsedMicroseconds.QuadPart *= 1000000;
    ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;

    if (FALSE == bErrorFlag)
    {
        _tprintf(TEXT("Terminal failure: Unable to write to file with error code %d.\n"), GetLastError());
    }
    else
    {
        if (dwBytesWritten != dwBytesToWrite)
        {
            // This is an error because a synchronous write that results in
            // success (WriteFile returns TRUE) should write all data as
            // requested. This would not necessarily be the case for
            // asynchronous writes.
            printf("Error: dwBytesWritten != dwBytesToWrite\n");
        }
        else
        {
            _tprintf(TEXT("Wrote %d bytes successfully.\n"), dwBytesWritten * COUNT);
            double time = (ElapsedMicroseconds.QuadPart / (double) 1000000); //to seconds
            double totalmb =  (((dwBytesWritten * COUNT) / (double) 1024)) / (double) 1024;
            _tprintf(TEXT("Wrote %lf mb successfully.\n"), totalmb);
            _tprintf(TEXT("Elapsed time = %lf seconds\n"), time);
            _tprintf(TEXT("Your write speed is %lf mb/sec"), (double) totalmb/time);
        }
    }

    CloseHandle(hFile);
}