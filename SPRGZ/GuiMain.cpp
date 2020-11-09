#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <CommCtrl.h>
#include "benchmark.h"

// id UI элементов 
#define cb_list_disks_id 1
#define cb_list_buffers_id 2
#define cb_list_testCounts_id 3
#define cb_list_modes_id 4
#define btn_stop_id 5
#define btn_pause_id 6
#define btn_start_id 7
#define cb_list_files_id 8

LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

// Строковые константы для размещения их в выпадающие списки
const TCHAR* modes[] = { "WRITE_THROUGH", "RANDOM_ACCESS", "SEQUENTIAL", "NO_BUFFERING"};
const TCHAR* buffNames[] = { "1 KB", "4 KB", "8 KB", "1 MB", "2 MB", "4 MB", "8 MB", "16 MB" };
const TCHAR* fileNames[] = { "128 MB", "256 MB", "512 MB", "1024 MB"};
const TCHAR* disks[26];
const TCHAR* testCounts[] = { "1", "2", "3", "4", "5" };

// Константы значений для юзер конфига
int buffSizes[] = { 1 * KB, 4 * KB, 8 * KB, 1 * MB, 2 * MB, 4 * MB, 8 * MB, 16 * MB };
int fileSizes[] = { 128 * MB, 256 * MB, 512 * MB, 1024 * MB };


int main() {
	//определение дисков
	getDisks();

	// Создание окна
	WNDCLASS wcl;

	memset(&wcl, 0, sizeof(WNDCLASS));
	wcl.lpszClassName = "my window";
	wcl.lpfnWndProc = WndProc;
	wcl.hbrBackground = (HBRUSH)(CreateSolidBrush(RGB(243, 243, 243)));

	RegisterClass(&wcl);

	HWND hwnd;
	hwnd = CreateWindow("my Window", "Disk benchmark", WS_SYSMENU | WS_MINIMIZEBOX, 10, 10, 550, 480, NULL, NULL, NULL, NULL);

	ShowWindow(hwnd, SW_SHOWNORMAL);

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
		bool find = ((dr >> i) & 0x00000001); // Все диски представляются битовой маской, если бит 0 установлен в единицу, значит существует диск А и так далее по алфавиту
		if (find) {
			char diskChar = i + 65; // Имя диска
			disks[countDisk] = new TCHAR[5]{ diskChar, ':', '\\', '\0' }; // Создание строки типа пути диска "X:\"
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


//////////////////////////////////////////////////////////////////////////////////////////// Функции для создания графических элементов ////////////////////////////////////////////////////////////////////////////////////////////

// Структура с параметрами view объектов: x,y - координаты объекта, width,height - размеры view
struct ViewParam {
	int x, y;
	int width, height;
};

void createBox(const char* nameBtn, ViewParam* params, HWND& hwnd) {
	HWND box;
	box = CreateWindow("Button", nameBtn, WS_CHILD | WS_VISIBLE | BS_GROUPBOX, params->x-20, params->y-20, params->width+20, params->height+20, hwnd, NULL, NULL, NULL);
	setFont(box, 16, FW_MEDIUM);
}

void createCombobox(const char* nameBox, ViewParam* params, const TCHAR* values[], int countValues, DWORD id, HWND& hwnd) {
	HWND dropList;
	dropList = CreateWindow("combobox", nameBox, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST, params->x, params->y, params->width, params->height, hwnd, (HMENU)id, NULL, NULL);

	for (int i = 0; i < countValues; ++i)
		SendMessage(dropList, CB_ADDSTRING, 0, LPARAM(values[i]));
	SendMessage(dropList, CB_SETCURSEL, 0, 0);
	setFont(dropList, 16, FW_MEDIUM);
}

void createButton(const char* nameBtn, ViewParam* params, DWORD id, HWND& hwnd) {
	HWND btn;
	btn = CreateWindow("button", nameBtn, WS_VISIBLE | WS_CHILD, params->x, params->y, params->width, params->height, hwnd, (HMENU)id, NULL, NULL);
	setFont(btn, 17, FW_BOLD);
}

void createText(const char* nameBtn, ViewParam* params, DWORD id, HWND& hwnd, int isBold, int size) {
	HWND stc;
	stc = CreateWindow("static", nameBtn, WS_VISIBLE | WS_CHILD | SS_CENTER, params->x, params->y, params->width, params->height, hwnd, (HMENU)id, NULL, NULL);
	setFont(stc, size, isBold);
}

void createProgressBar(ViewParam* params, DWORD id, HWND& hwnd) {
	HWND progBar = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_VISIBLE | WS_CHILD, params->x, params->y, params->width, params->height, hwnd, (HMENU)id, NULL, NULL);

	//Установим диапазон 0-2
	SendMessage(progBar, PBM_SETRANGE, 0, (LPARAM)MAKELONG(0, 2));

	//Установим шаг
	SendMessage(progBar, PBM_SETSTEP, (WPARAM)1, 0); //шаг 1

	//Увеличиваем индикатор на 1 деление
	SendMessage(progBar, PBM_STEPIT, 0, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

	switch (message)
	{
	
	case WM_CREATE: {
		ViewParam params;

		// Создание графических элементов
		createCombobox("Диски", new ViewParam{ 10, 10, 180, 400 }, disks, sizeof(disks) / sizeof(disks[0]), cb_list_disks_id, hwnd);
		createCombobox("Кол-во тестов", new ViewParam{ 200, 10, 30, 500 }, testCounts, sizeof(testCounts) / sizeof(testCounts[0]), cb_list_testCounts_id, hwnd);
		createCombobox("Размер буфера", new ViewParam{ 240, 10, 60, 500 }, buffNames, sizeof(buffNames) / sizeof(buffNames[0]), cb_list_buffers_id, hwnd);
		createCombobox("Размер файла", new ViewParam{ 310, 10, 60, 500 }, fileNames, sizeof(fileNames) / sizeof(fileNames[0]), cb_list_files_id, hwnd);
		createCombobox("Типы", new ViewParam{ 380, 10, 140, 500 }, modes, sizeof(modes) / sizeof(modes[0]), cb_list_modes_id, hwnd);

		createButton("Старт", new ViewParam{ 400, 40, 120, 30 }, btn_start_id, hwnd);
		createButton("Пауза", new ViewParam{ 400, 80, 120, 30 }, btn_pause_id, hwnd);
		createButton("Стоп", new ViewParam{ 400, 120, 120, 30 }, btn_stop_id, hwnd);

		createBox("Результаты теста", new ViewParam{ 30, 60, 360, 180 }, hwnd); // Визаулизация рамки

		createText("Чтение", new ViewParam{ 50, 70, 100, 20 }, NULL, hwnd, FW_HEAVY, 20);
		createText("Запись", new ViewParam{ 250, 70, 100, 20 }, NULL, hwnd, FW_HEAVY, 20);
		createText("130 MB/s", new ViewParam{ 50, 100, 100, 20 }, NULL, hwnd, FW_MEDIUM, 18);
		createText("250 MB/s", new ViewParam{ 250, 100, 100, 20 }, NULL, hwnd, FW_MEDIUM, 18);

		createProgressBar(new ViewParam{ 10, 250, 510, 30 }, NULL, hwnd);
		break;
	}
	case WM_COMMAND:
		// Если wParam = CBN_SELCHANGE, значит было выбрано одно из полей выпадающего списка
		if (HIWORD(wParam) == CBN_SELCHANGE)
		{
			int selectedId = SendMessage(HWND(lParam), CB_GETCURSEL, 0, 0); // Индекс поля выпадающего списка
			
			// Опредление выпадающего списка
			switch (LOWORD(wParam))
			{
			// Установка настроек в зависимости от выбора пользователя
			case cb_list_modes_id:
				userConfig.mode = getModeFromType(modes[selectedId]);
				break;
			case cb_list_buffers_id:
				userConfig.bufferSize = buffSizes[selectedId];
				break;
			case cb_list_files_id:
				userConfig.fileSize = fileSizes[selectedId];
				break;
			case cb_list_disks_id:
				userConfig.disk = disks[selectedId];
				break;
			}
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0); // Закрытие приложени
		break;
	}

	return DefWindowProcA(hwnd, message, wParam, lParam);
}


