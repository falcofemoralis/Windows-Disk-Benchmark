#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <CommCtrl.h>
#include "benchmark.h"

#define ID_BTN 20
#define ID_CB 30
#define ID_TEXT 40

// Для треда
#define PAUSE 2
#define WORKING 1
#define CANCELED 0

// id UI элементов 
#define btn_stop_id ID_BTN
#define btn_pause_id ID_BTN + 1
#define btn_start_id ID_BTN + 2
#define cb_list_files_id ID_CB
#define cb_list_disks_id ID_CB + 1
#define cb_list_buffers_id ID_CB + 2
#define cb_list_testCounts_id ID_CB + 3
#define cb_list_modes_id ID_CB + 4
#define text_read_id ID_TEXT
#define text_write_id ID_TEXT + 1	

LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

// Строковые константы для размещения их в выпадающие списки
const TCHAR* modes[] = { "WRITE_THROUGH", "RANDOM_ACCESS", "SEQUENTIAL"};
const TCHAR* buffNames[] = { "1 KB", "4 KB", "8 KB", "1 MB", "2 MB", "4 MB", "8 MB", "16 MB" };
const TCHAR* fileNames[] = { "128 MB", "256 MB", "512 MB", "1024 MB", "2048 MB"};
const TCHAR* disks[26];
const TCHAR* disksNames[26];
const TCHAR* testCounts[] = { "1", "2", "3", "4", "5" };

// Константы значений для юзер конфига
DWORD buffSizes[] = { 1 * KB, 4 * KB, 8 * KB, 1 * MB, 2 * MB, 4 * MB, 8 * MB, 16 * MB };
unsigned int fileSizes[] = { 128 * MB, 256 * MB, 512 * MB, 1024 * MB, 2048 * MB };

HWND btn_stop, btn_pause, btn_start, cb_list_files, cb_list_disks, cb_list_buffers, cb_list_testCounts, text_read, text_write, pb_progress;
HWND *rb_group_modes;

DWORD mainThreadId; // ID основного потока
HANDLE testThread;
DWORD threadStatus = CANCELED;

// Инициализация всех необходимых первоначальных данных
void init();

int main() {
	//инициализация 
	init();

	// Создание окна
	WNDCLASS wcl;

	memset(&wcl, 0, sizeof(WNDCLASS));
	wcl.lpszClassName = "my window";
	wcl.lpfnWndProc = WndProc;
	wcl.hbrBackground = (HBRUSH)(CreateSolidBrush(RGB(243, 243, 243)));
	RegisterClass(&wcl);

	HWND mainWindow = CreateWindow("my Window", "Disk benchmark", WS_SYSMENU | WS_MINIMIZEBOX, 10, 10, 450, 400, NULL, NULL, NULL, NULL);
	ShowWindow(mainWindow, SW_SHOWNORMAL);

	// Принимает сообщения, а так же не дает программе завершится
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		DispatchMessage(&msg);
	}

	return 0;
}

void getDisks() {
	//определение дисков
	DWORD dr = GetLogicalDrives();

	int countDisk = 0; // Счетчик для записи в массив всех дисков

	for (int i = 0; i < 26; i++)
	{
		bool isFound = ((dr >> i) & 0x00000001); // Все диски представляются битовой маской, если бит 0 установлен в единицу, значит существует диск А и так далее по алфавиту
		if (isFound) {
			//получем букву диска
			char diskChar = i + 65; // Имя диска
			disks[countDisk] = new TCHAR[5]{ diskChar, ':', '\\', '\0' }; // Создание строки типа пути диска "X:\"

			//получаем имя диска
			TCHAR* VolumeName = new TCHAR[MAX_PATH];
			if (!GetVolumeInformation(disks[countDisk], VolumeName, MAX_PATH, NULL, NULL, NULL, NULL, 0));

			//создаем название из буквы диска и имени диска, если у диска нету названия, даем имя Local Disk
			TCHAR* name = new TCHAR[MAX_PATH + 5]{ '(', diskChar, ':', ')', ' ' };
			if (_tcslen(VolumeName) == 0) _tcscat_s(name, MAX_PATH + 5, TEXT("Local Disk"));
			else _tcscat_s(name, MAX_PATH + 5, VolumeName);

			disksNames[countDisk] = name;
			++countDisk;
		}
	}
}

// Установка шрифта
void setFont(HWND hwnd, int size, int weight) {
	HFONT font = CreateFont(size, 0, 0, 0, weight, FALSE, FALSE, FALSE, ANSI_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
	SendMessage(hwnd, WM_SETFONT, WPARAM(font), TRUE);
}

void init() {
	//определение дисков
	getDisks();

	// Инциализация первоначального состояния юзер конфига
	userConfig.bufferSize = buffSizes[0];
	userConfig.countTests = 1;
	userConfig.fileSize = fileSizes[0];
	userConfig.mode = getModeFromType(modes[0]);
	userConfig.disk = disks[0];
}

/////////////////////////////////// Функции для создания графических элементов ///////////////////////////////////


// Структура с параметрами view объектов: x,y - координаты объекта, width,height - размеры view
struct ViewParam {
	DWORD x, y;
	DWORD width, height;
};

void createBox(const char* nameBtn, ViewParam* params, HWND& hwnd, DWORD atr) {
	HWND box;
	box = CreateWindow("Button", nameBtn, WS_CHILD | WS_VISIBLE | BS_GROUPBOX | atr, params->x-20, params->y-20, params->width+20, params->height+20, hwnd, NULL, NULL, NULL);
	setFont(box, 16, FW_MEDIUM);
}

HWND createCombobox(const char* nameBox, ViewParam* params, const TCHAR* values[], DWORD countValues, DWORD id, HWND& hwnd) {
	HWND dropList;
	dropList = CreateWindow("combobox", nameBox, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST, params->x, params->y, params->width, params->height, hwnd, (HMENU)id, NULL, NULL);
	for (DWORD i = 0; i < countValues; ++i)
		SendMessage(dropList, CB_ADDSTRING, 0, LPARAM(values[i]));
	SendMessage(dropList, CB_SETCURSEL, 0, 0);
	setFont(dropList, 16, FW_MEDIUM);
	return dropList;
}

HWND createButton(const char* nameBtn, ViewParam* params, DWORD id, HWND& hwnd) {
	HWND btn;
	btn = CreateWindow("button", nameBtn, WS_VISIBLE | WS_CHILD, params->x, params->y, params->width, params->height, hwnd, (HMENU)id, NULL, NULL);
	setFont(btn, 17, FW_BOLD);
	return btn;
}

HWND createText(const char* nameBtn, ViewParam* params, DWORD id, HWND& hwnd, DWORD isBold, DWORD size) {
	HWND st;
	st = CreateWindow("static", nameBtn, WS_VISIBLE | WS_CHILD | SS_CENTER | BS_VCENTER, params->x, params->y, params->width, params->height, hwnd, (HMENU)id, NULL, NULL);
	setFont(st, size, isBold);
	return st;
}

HWND createProgressBar(ViewParam* params, DWORD id, HWND& hwnd) {
	HWND pb = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_VISIBLE | WS_CHILD, params->x, params->y, params->width, params->height, hwnd, (HMENU)id, NULL, NULL);

	//Установим шаг в 1
	SendMessage(pb, PBM_SETSTEP, (WPARAM)1, 0);

	return pb;
}

HWND* createRadiobtnGroup(const char* nameBtn, ViewParam* params, const TCHAR* values[], DWORD countValues,  DWORD id, HWND& hwnd) {
	HWND *btns = new HWND[countValues];
	DWORD y = params->y;
	for (DWORD i = 0; i < countValues; ++i) {
		btns[i] = CreateWindow("button", modes[i], WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, params->x, y, params->width, params->height, hwnd, (HMENU)id, NULL, NULL);
		setFont(btns[i], 15, FW_MEDIUM);
		y += 20;
	}

	SendMessage(btns[0], BM_SETCHECK, BST_CHECKED, 0);

	return btns;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

	switch (message)
	{

	case WM_CREATE: {
		// Создание графических элементов

		ViewParam params;

		//Диски
		createText("Drive to test:", new ViewParam{ 10, 13, 90, 30 }, NULL, hwnd, FW_MEDIUM, 18);
		cb_list_disks = createCombobox("Диски", new ViewParam{ 110, 10, 310, 400 }, disksNames, sizeof(disksNames) / sizeof(disksNames[0]), cb_list_disks_id, hwnd);

		//Ряд настроек размеров
		createBox("File size", new ViewParam{ 30, 60, 80, 40 }, hwnd, NULL);
		cb_list_files = createCombobox("Размер файла", new ViewParam{ 20, 60, 80, 500 }, fileNames, sizeof(fileNames) / sizeof(fileNames[0]), cb_list_files_id, hwnd);

		createBox("Buffer size", new ViewParam{ 190, 60, 80, 40 }, hwnd, NULL); 
		cb_list_buffers = createCombobox("Размер буфера", new ViewParam{ 180, 60, 80, 500 }, buffNames, sizeof(buffNames) / sizeof(buffNames[0]), cb_list_buffers_id, hwnd);

		createBox("Passes", new ViewParam{ 340, 60, 80, 40 }, hwnd, NULL);
		cb_list_testCounts = createCombobox("Кол-во тестов", new ViewParam{ 330, 60, 80, 500 }, testCounts, sizeof(testCounts) / sizeof(testCounts[0]), cb_list_testCounts_id, hwnd);
	
		//Блок результатов
		createBox("Test results", new ViewParam{ 30, 130, 240, 120 }, hwnd, BS_CENTER);
		createBox("Write", new ViewParam{ 40, 150, 95, 90 }, hwnd, BS_CENTER);
		text_write = createText("0 MB\\c", new ViewParam{ 25, 180, 105, 20 }, text_write_id, hwnd, FW_MEDIUM, 16);
		createBox("Read", new ViewParam{ 165, 150, 95, 90 }, hwnd, BS_CENTER);
		text_read = createText("0 MB\\c", new ViewParam{ 155, 180, 100, 20 }, text_read_id, hwnd, FW_MEDIUM, 16);

		//Блок режимов
		createBox("Modes", new ViewParam{ 300, 130, 120, 120 }, hwnd, BS_CENTER);
		rb_group_modes = createRadiobtnGroup("Modes buttons", new ViewParam{ 285, 140, 125, 20 }, modes, sizeof(modes) / sizeof(modes[0]), NULL, hwnd);

		//кнопки управления
		btn_start = createButton("Start", new ViewParam{ 10, 260, 120, 30 }, btn_start_id, hwnd);
		btn_pause = createButton("Pause", new ViewParam{ 160, 260, 120, 30 }, btn_pause_id, hwnd);
		btn_stop = createButton("Stop", new ViewParam{ 300, 260, 120, 30 }, btn_stop_id, hwnd);

		//прогресс бар
		pb_progress = createProgressBar(new ViewParam{ 10, 300, 410, 30 }, NULL, hwnd);
		break;
	}
	case WM_COMMAND:
		// Если wParam = CBN_SELCHANGE, значит было выбрано одно из полей выпадающего списка
		if (HIWORD(wParam) == CBN_SELCHANGE) {
			int selectedId;
			selectedId = SendMessage(HWND(lParam), CB_GETCURSEL, 0, 0); // Индекс поля выпадающего списка

			if (HWND(lParam) == cb_list_buffers) {
				userConfig.bufferSize = buffSizes[selectedId];
			}
			else if (HWND(lParam) == cb_list_files) {
				userConfig.fileSize = fileSizes[selectedId];
			}
			else if (HWND(lParam) == cb_list_disks) {
				userConfig.disk = disks[selectedId];
			}
			else if (HWND(lParam) == cb_list_testCounts) {
				userConfig.countTests = selectedId + 1;
			}
		}

    	//нажата одна из radio button
		if (HIWORD(wParam) == BN_CLICKED) {
			for (DWORD i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i)
				if (HWND(lParam) == rb_group_modes[i]) {
					userConfig.mode = getModeFromType(modes[i]);
				}	
		}

		// Управление потом тестирования (самим тестом)
		if (HWND(lParam) == btn_start) {
			puts("Resume");
			EnableWindow(btn_start, false);
			testThread = CreateThread(NULL, 0, writeTest, NULL, CREATE_SUSPENDED | THREAD_SUSPEND_RESUME, NULL);
			threadStatus = WORKING;
			ResumeThread(testThread);
		}

		if (HWND(lParam) == btn_pause) {
			Sleep(50);

			if (threadStatus != PAUSE) {
				threadStatus = PAUSE;
				SetWindowText(btn_pause, "Resume");
				SuspendThread(testThread);
			}
			else {
				threadStatus = WORKING;
				SetWindowText(btn_pause, "Pause");
				ResumeThread(testThread);
			}

			puts("paused");
		}

		if (HWND(lParam) == btn_stop) {
			EnableWindow(btn_start, true);
			SetWindowText(btn_pause, "Pause");
			threadStatus = CANCELED;
			ResumeThread(testThread); // Завершаем поток естественным образом
			puts("stoped");
		}


		break;
	case WM_DESTROY:
		PostQuitMessage(0); // Закрытие приложени
		break;
	}

	return DefWindowProcA(hwnd, message, wParam, lParam);
}


