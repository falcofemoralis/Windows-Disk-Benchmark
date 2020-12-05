#include "GUIMain.h"
#include "benchmark.h"

// Структура конфига, представляет из себя все поля настроек для тестирования диска в приложении 
Config testConfig;
CONST DWORD SIZE_INTERVAL = 50 * MB;

/* Тестирования, открывается в отдельном потоке
* Параметры:
* param - пустой указатель по которому можно получить данные переданные с потока
*/
DWORD WINAPI testDrive(LPVOID  param) {
    testConfig = *((Config*)param);

    DWORD typeAccess, typeOpen, bufferFlag, modeFlag;
    DOUBLE totalTime = 0, totalmb = 0;
    HANDLE file;

    // Определение полного пути к файлу
    TCHAR fullPath[20];
    _tcscpy_s(fullPath, testConfig.disk);
    _tcscat_s(fullPath, _T("test.tmp"));

    // Создание хедлера на файл в зависимости от типа теста
    if (testConfig.typeTest == READ_TEST) {
        createTestFile(fullPath);
        typeAccess = GENERIC_READ;
        typeOpen = OPEN_ALWAYS;
    }
    else {
        typeAccess = GENERIC_WRITE;
        typeOpen = CREATE_ALWAYS;
    }

    bufferFlag = testConfig.isBuffering ? NULL : FILE_FLAG_NO_BUFFERING;
    modeFlag = getModeFromType(testConfig.mode);

    for (DWORD i = 0; i < testConfig.countTests; i++) {
        file = CreateFile(fullPath,
            typeAccess,
            0,
            NULL,
            typeOpen,
            modeFlag | bufferFlag,
            NULL);

        // Проверка но то, определен ли хендлер
        if (file == INVALID_HANDLE_VALUE) {
            _tprintf(_T("Terminal failure: Unable to create file for write with error code %d.\n"), GetLastError());
            CloseHandle(file);
            PostThreadMessage(testConfig.parentThreadId, SEND_PROGRESS_BAR_UPDATE, 0, (LPARAM)new DWORD(0));
            return -1;
        }

        // Запуск записи в файл и подсчет результата (test.first - количество записаных мегбайт, test.second - количество затраченого времени)
        RESULT test = testIteration(file, i);

        //отслеживаем ошибки
        if (test.first == NULL) {
            _tprintf(_T("Terminal failure: Unable to write to file with error code %d.\n"), GetLastError());
            CloseHandle(file);
            PostThreadMessage(testConfig.parentThreadId, SEND_PROGRESS_BAR_UPDATE, 0, (LPARAM)new DWORD(0));
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
    totalTime /= 1000; // Перевод в секунды

    TCHAR str[20];
    DOUBLE res = (DOUBLE)totalmb / totalTime;
    _stprintf_s(str, _T("%.2lf"), res);
    _tcscat_s(str, _T(" МБ\\с"));
    Sleep(500);

    PostThreadMessage(testConfig.parentThreadId, SEND_TEST_RESULT, 0, (LPARAM)str);
    Sleep(100);
    return 0;
}

/* Тестирования одного прохода по файлу
* Параметры:
* file - хендл созданого файла
* iteration - номер итерации прохода по файду
*/
RESULT testIteration(HANDLE file, DWORD iteration) {
    // Для подсчет времени
    LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
    LARGE_INTEGER Frequency;
    BOOL bErrorFlag; // Ошибки записи/чтения в файл
     
    //тестовый массив данных
    TCHAR* DataBuffer = new TCHAR[testConfig.bufferSize];

    // Если идет тест на запись - происходит заполнение буфера
    if (testConfig.typeTest == WRITE_TEST)
        fillBuffer(DataBuffer, testConfig.bufferSize);
    
    // Для подсчета времени и количества байт
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
    for (DWORD i = 0; i < iterations; ++i) {
        if (threadStatus == CANCELED)
            return make_pair(NULL, NULL);

        // Если поток остановлен - останавливается текущий поток (Останавливается "мягко" для того чтобы остановка не произошла на самой записи/чтении)
        if (threadStatus == PAUSE)
            SuspendThread(GetCurrentThread());
        
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

        // Сохранение в в секундах
        buffersTimes[i] = ElapsedMicroseconds.QuadPart / (DOUBLE)1000;
        sumBytesProcess += dwBytesProcess;
        totalTime += buffersTimes[i];

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

    TCHAR fileName[30];
    if (testConfig.typeTest == WRITE_TEST)
        sprintf(fileName, "WriteTest%d", iteration);
    else
        sprintf(fileName, "ReadTest%d", iteration);

    saveResults(buffersTimes, fileName, iterations, TYPE_GRAPH);
    saveResults(buffersTimes, fileName, iterations, TYPE_HISTOGRAM);

    return make_pair(sumBytesProcess, totalTime);
}

/* Создание тестового файла для чтения
* Параметры:
* fileName - полный путь к файлу
*/
VOID createTestFile(TCHAR* fileName) {
    //создаем файл "test.bin", после закрытия хендла файл будет удален
    HANDLE testFile = CreateFile(fileName,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        getModeFromType(testConfig.mode),
        NULL);

    //тестовый массив данных
    DWORD bufferSize = 32 * MB;
	TCHAR* DataBuffer = new TCHAR[bufferSize];
    fillBuffer(DataBuffer, bufferSize);

    // Переменные для тестирования
    DWORD iterations = (DWORD)(testConfig.fileSize / bufferSize) + 1;
    DWORD dwBytesToWrite = bufferSize * iterations;
    DWORD dwBytesWritten = 0, sumWritten = 0;

    //записываем в файл count раз массива данных
    BOOL bErrorFlag;
    for (DWORD i = 0; i < iterations; ++i) {
        bErrorFlag = WriteFile(
            testFile,
            DataBuffer,
            bufferSize,
            &dwBytesWritten,
            NULL);
        if (bErrorFlag == FALSE) return;

        sumWritten += dwBytesWritten;
    }

    if (sumWritten != dwBytesToWrite) {
        _tprintf(_T("Error: dwBytesWritten != dwBytesToWrite\n"));
        return;
    }      
    
    CloseHandle(testFile);
}

/* Сохранение результатов для построение графика
* Параметры:
* valuesArray - массив данных
* fileName - имя файла
* size - размер массива
* type - тип сохраняемых данных
*/
VOID saveResults(DOUBLE* valuesArray, TCHAR* fileName, DWORD size, DWORD type) {
    CONST TCHAR* saveResultsTypes[2] = { "Graph", "Histogram" };
    // Необходимые переменные
    DWORD dwTemp;
    TCHAR buffer[30];
    TCHAR* newFileName = new TCHAR[70];

    sprintf(newFileName, "%s_%dKB_%s_%s_%s.csv\0", testConfig.isBuffering ? "B" : "N", testConfig.bufferSize / 1024, fileName, saveResultsTypes[type], testConfig.mode);

    // Создаем файл, куда будут записанны значения
    HANDLE hFile = CreateFile(newFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hFile) return;
    
    if (type == TYPE_GRAPH) {
        DOUBLE totalTime = 0;
        DOUBLE outputTime = 0;
        DWORD totaSize = 0;
        DWORD tmpBuf = 0;

        // Вывод интервалов по SIZE_INTERVAL байт
        for (DWORD i = 1; i <= size; ++i) {
            if (tmpBuf >= SIZE_INTERVAL) {
                tmpBuf -= SIZE_INTERVAL; // Вычитание интервала, остается остаток в виде "лишних" для подсчета байт
                totaSize += SIZE_INTERVAL;

                DOUBLE percentTime = ((DOUBLE(testConfig.bufferSize - tmpBuf)) / testConfig.bufferSize) * valuesArray[i];
                outputTime = totalTime + percentTime;
                totalTime = valuesArray[i] - percentTime;

                sprintf(buffer, "%d;%.6lf\n\0", totaSize, outputTime);
                WriteFile(hFile, buffer, _tcslen(buffer) * sizeof(TCHAR), &dwTemp, NULL);
            }

            totalTime += valuesArray[i];
            tmpBuf += testConfig.bufferSize;
        }
    }
    else {
        CONST DWORD MAX_ITERATIONS = 4, MAX_RANGES = 5;
        DWORD iterations[MAX_ITERATIONS];
        DOUBLE ranges[MAX_RANGES];
        ZeroMemory(&iterations, sizeof(iterations));

        std::sort(valuesArray, valuesArray + size);

        DOUBLE sum = 0;
        for (DWORD i = 0; i < size; ++i)
            sum += valuesArray[i];

        DOUBLE average = sum / size; // Среднее значение
        DOUBLE range = (average - valuesArray[0]) * 0.5; // Диапазон от среднего значения

        ranges[0] = valuesArray[0]; // Минимум
        ranges[1] = average - range; // Левая граница
        ranges[2] = average + range; // Правая граница
        ranges[3] = average * 3; // Отклонение среднего (взято как 200% пока что)
        ranges[4] = valuesArray[size-1]; // Максимум

        for (DWORD i = 0; i < size; ++i)
            for (DWORD j = 1; j <= MAX_RANGES; ++j)
                if (valuesArray[i] <= ranges[j]) {
                    ++iterations[j - 1];
                    break;
                }
        
        for (DWORD i = 0; i < MAX_ITERATIONS; ++i) {
            if (iterations[i] == 0)
                continue;
            sprintf(buffer, "%3.2lf;%.3lf-%.3lf\n\0", ((DOUBLE)iterations[i] / size) * 100, ranges[i], ranges[i + 1]);
            WriteFile(hFile, buffer, _tcslen(buffer) * sizeof(TCHAR), &dwTemp, NULL);
        }
    }

    CloseHandle(hFile);
}

/* Заполнение буфера
* Параметры:
* dataBuffer - исходный массив буфера который будет заполнятся
* sizeBuffer - размер буфера (массива)
*/
VOID fillBuffer(TCHAR* dataBuffer, DWORD sizeBuffer) {
    //тестовый массив данных
    TCHAR Data[] = _T("0x10");
    DWORD divider = sizeof(Data) / sizeof(Data[0]) - 1;
    for (DWORD i = 0; i < sizeBuffer; i++)
        dataBuffer[i] = Data[i % divider];
}

/* Перевод строкового обозначения режима
* Параметры:
* type - сам строковый режим
*/
DWORD getModeFromType(CONST TCHAR* type) {
    if (!_tcscmp(type, "RANDOM_ACCESS"))
        return FILE_FLAG_RANDOM_ACCESS;
    else if (!_tcscmp(type, "WRITE_THROUGH"))
        return FILE_FLAG_WRITE_THROUGH;
    else if (!_tcscmp(type, "SEQUENTIAL"))
        return FILE_FLAG_SEQUENTIAL_SCAN;
}

// Функция аналог С++ для создания пары (опять же без шаблона)
pair make_pair(DWORD first, DOUBLE second) {
    pair newPair = { first, second };
    return newPair;
}