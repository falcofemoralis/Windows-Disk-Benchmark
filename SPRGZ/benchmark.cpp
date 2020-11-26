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

/* Тестирования, открывается в отдельном потоке
* Параметры:
* param - пустой указатель по которому можно получить данные переданные с потока
*/
DWORD WINAPI testDrive(LPVOID  param) {
	testConfig = *((Config*)param); // id родительского потока

	DOUBLE totalTime = 0, totalmb = 0;

	// Определение полного пути к файлу
	TCHAR fullPath[20] = _T("");
	_tcscat_s(fullPath, testConfig.disk);
	_tcscat_s(fullPath, _T("test.tmp"));

	// Создание хедлера на файл в зависимости от типа теста
	HANDLE file;
	DWORD typeAccess, typeOpen, bufferFlag;

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

	for (DWORD i = 0; i < testConfig.countTests; i++)
	{
		file = CreateFile(fullPath,
			typeAccess,
			0,
			NULL,
			typeOpen,
			testConfig.mode | bufferFlag,
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
		filledBuffer(DataBuffer, testConfig.bufferSize);

	DOUBLE totalTime = 0;
	DWORD iterations = (DWORD)(testConfig.fileSize / testConfig.bufferSize) + 1;
	DWORD dwBytesToProcess = testConfig.bufferSize * iterations;
	DWORD dwBytesProcess = 0, sumBytesProcess = 0;

	//начинаем отсчет времени
	QueryPerformanceFrequency(&Frequency);

	// Для отображение прогресса на прогресс баре
	DWORD curProgress = (iterations * iteration * 100);
	DWORD allSteps = iterations * testConfig.countTests;

	// Для гистограмы
	DOUBLE baseTime = 0.001;
	DWORD counter = 0;
	DWORD* iterationsPerTime = new DWORD[SIZE_OF_HISTOGRAM];

	DOUBLE* buffersTimes = new DOUBLE[iterations];
	DWORD pbStateCurrent, pbStateLast = 0; // Состояния прогресс бара
	//записываем в файл count раз массива данных
	for (DWORD i = 0; i < iterations; ++i)
	{
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

		//Пока что вывод итераций в консоль 
		buffersTimes[i] = ElapsedMicroseconds.QuadPart / (double)1000000;
		sumBytesProcess += dwBytesProcess;
		totalTime += (ElapsedMicroseconds.QuadPart / (double)1000000);

		// Запись времени для гистограмы
		if (counter < SIZE_OF_HISTOGRAM && totalTime > baseTime) {
			iterationsPerTime[counter] = i + 1;
			baseTime *= 10;
			++counter;
		}

		//установка прогресса - если текущий процент прогресса (целое число) изменился по сравнению с прошлым - происходит изменение прогресс бара
		pbStateCurrent = ((100 * (i + 1) + curProgress) / allSteps);
		if (pbStateCurrent > pbStateLast) {
			PostThreadMessage(testConfig.parentThreadId, SEND_PROGRESS_BAR_UPDATE, 0, (LPARAM)& pbStateCurrent);
			pbStateLast = pbStateCurrent; // Текущее состояние для дальнейшей операции становится прошлым
		}
	}

	if (counter == SIZE_OF_HISTOGRAM - 1)
		iterationsPerTime[counter] = iterations;

	// Если количество обработаных байт не совпадает с необходимым - возникла ошибка
	if (sumBytesProcess != dwBytesToProcess)
	{
		_tprintf(_T("Error: dwBytesWritten != dwBytesToWrite\n"));
		return make_pair(NULL, NULL);
	}

	TCHAR fileName[30];
	if (testConfig.typeTest == WRITE_TEST)
		sprintf(fileName, "%dKB__WriteTest%d", testConfig.bufferSize / 1024, iteration);
	else
		sprintf(fileName, "%dKB__ReadTest%d", testConfig.bufferSize / 1024, iteration);

	saveResultsGraph(buffersTimes, iterations, fileName);
	saveResultsOfHistogram(iterationsPerTime, counter, fileName);
	return make_pair(sumBytesProcess, totalTime);
}

/* Создание тестового файла для чтения
* Параметры:
* fileName - полный путь к файлу
*/
VOID createTestFile(TCHAR fileName[]) {
	//создаем файл "test.bin", после закрытия хендла файл будет удален
	HANDLE testFile = CreateFile(fileName,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		testConfig.mode,
		NULL);

	//тестовый массив данных
	DWORD bufferSize = 32 * MB;
	TCHAR* DataBuffer = new TCHAR[bufferSize];
	filledBuffer(DataBuffer, bufferSize);

	// Переменные для тестирования
	DWORD iterations = (DWORD)(testConfig.fileSize / bufferSize) + 1;
	DWORD dwBytesToWrite = bufferSize * iterations;
	DWORD dwBytesWritten = 0, sumWritten = 0;

	//записываем в файл count раз массива данных
	BOOL bErrorFlag;
	for (DWORD i = 0; i < iterations; ++i)
	{
		bErrorFlag = WriteFile(
			testFile,
			DataBuffer,
			bufferSize,
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

/* Сохранение результатов для построение графика
* Параметры:
* data - массив данных
* size - размер массива
* fileName - имя файла
*/
VOID saveResultsGraph(DOUBLE* data, DWORD size, TCHAR* fileName) {
	DWORD dwTemp;
	TCHAR* newFileName = new TCHAR[100];
	_tcscpy(newFileName, fileName);
	_tcscat(newFileName, "_Graph.csv\0");

	HANDLE hFile = CreateFile(newFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == hFile)
		return;

	DWORD delta = 0;
	for (DWORD i = 1; i <= size; i++)
	{
		DWORD sizeBuff = log10(i) + 11; // 11 символов на число с плавающей запятой и перенос каретки
		TCHAR* buffer = new TCHAR[sizeBuff];
		sprintf(buffer, "%d;%1.6lf\n", i, data[i - 1]);
		WriteFile(hFile, buffer, sizeBuff * sizeof(TCHAR), &dwTemp, NULL);
		delete[] buffer;
	}

	CloseHandle(hFile);
}

/* Сохранение результатов для построение графика
* Параметры:
* data - массив данных
* size - размер массива
* fileName - имя файла
*/
VOID saveResultsOfHistogram(DWORD* data, DWORD size, TCHAR* fileName) {
	CONST TCHAR* interval[SIZE_OF_HISTOGRAM] = { "1 мс", "10 мс", "100 мс", "1 с" , "10 с", ">10 c" };
	TCHAR* newFileName = new TCHAR[100];
	_tcscpy(newFileName, fileName);
	_tcscat(newFileName, "_Histo.csv\0");

	// Создаем файл, куда будут записанны значения
	HANDLE hFile = CreateFile(newFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == hFile)
		return;

	// Необходимые переменные
	DWORD dwTemp;
	for (DWORD i = 1; i <= size; ++i)
	{
		DWORD digits = log10(data[i - 1]) + 3 + _tcslen(interval[i - 1]);
		TCHAR* buffer = new TCHAR[digits];
		sprintf(buffer, "%s;%d\n", interval[i - 1], data[i - 1]);
		WriteFile(hFile, buffer, digits * sizeof(TCHAR), &dwTemp, NULL);
		delete[] buffer;
	}

	CloseHandle(hFile);
}

VOID filledBuffer(TCHAR* dataBuffer, DWORD sizeBuffer) {
	//тестовый массив данных
	TCHAR Data[] = _T("0x10");
	DWORD divider = sizeof(Data) / sizeof(Data[0]) - 1;
	for (DWORD i = 0; i < sizeBuffer; i++)
		dataBuffer[i] = Data[i % divider];
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