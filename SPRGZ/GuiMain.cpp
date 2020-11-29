#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#include "benchmark.h"
#include "GUIMain.h"

// Строковые константы для размещения их в выпадающие списки
CONST TCHAR* modes[] = { "WRITE_THROUGH", "RANDOM_ACCESS", "SEQUENTIAL"};
CONST TCHAR* buffNames[] = { "1 KB", "4 KB", "8 KB", "1 MB", "4 MB", "8 MB" };
CONST TCHAR* fileNames[] = { "128 MB", "256 MB", "512 MB", "1024 MB", "2048 MB"};
CONST TCHAR* disks[26];
CONST TCHAR* disksNames[26];
CONST TCHAR* testCounts[] = { "1", "2", "3", "4", "5" };

// Константы значений для юзер конфига
DWORD buffSizes[] = { 1 * KB, 4 * KB, 8 * KB, 1 * MB, 4 * MB, 8 * MB};
UINT32 fileSizes[] = { 128 * MB, 256 * MB, 512 * MB, 1024 * MB, 2048 * MB };

// Нужные переменные
HWND btn_stop, btn_pause, btn_startRead, btn_startWrite,
cb_list_files, cb_list_disks, cb_list_buffers, cb_list_testCounts, cb_buffering, text_read, text_write, pb_progress;
HWND *rb_group_modes;

// Переменные потока
HANDLE workingThread;
DWORD threadStatus = CANCELED;

// Настройки теста
Config userConfig;

INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
	//инициализация 
	initConfig();

	// Создание окна
	WNDCLASS wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.lpszClassName = "MainWndClass";
	wc.lpfnWndProc = wndProc;
	wc.hbrBackground = (HBRUSH)(CreateSolidBrush(RGB(243, 243, 243)));
	RegisterClass(&wc);
	HWND mainWindow = CreateWindow("MainWndClass", "Disk benchmark", WS_SYSMENU | WS_MINIMIZEBOX, 10, 10, 450, 400, NULL, NULL, NULL, NULL);
	ShowWindow(mainWindow, SW_SHOWNORMAL);

	// Принимает сообщения, а так же не дает программе завершится
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		switch (msg.message) {
			case SEND_TEST_RESULT: // Установка результата
				TCHAR* res;
				res = (TCHAR*)msg.lParam;
				setResult(res);
				break;
			case SEND_PROGRESS_BAR_UPDATE: // Обновление прогресс бара
				DWORD state;
				state = *((DWORD*)msg.lParam);
				SendMessage(pb_progress, PBM_SETPOS, state, 0);
				break;
		}

		DispatchMessage(&msg);
	}

	return 0;
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: onCreate(hwnd, message, wParam, lParam); break;
		case WM_COMMAND: onCommand(hwnd, message, wParam, lParam); break;
		case WM_DESTROY: PostQuitMessage(0); break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

VOID onCreate(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
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
	rb_group_modes = createRadioBtnGroup("Modes buttons", new ViewParam{ 285, 140, 125, 20 }, modes, sizeof(modes) / sizeof(modes[0]), NULL, hwnd);

	// Чекбокс буферизации
	cb_buffering = createCheckBox("BUFFERING", new ViewParam{ 285, 200, 125, 20 }, cb_buffering_id, hwnd);

	//кнопки управления
	btn_startRead = createButton("Read test", new ViewParam{ 165, 210, 80, 25 }, btn_startRead_id, hwnd);
	btn_startWrite = createButton("Write test", new ViewParam{ 35, 210, 80, 25 }, btn_startWrite_id, hwnd);
	btn_pause = createButton("Pause", new ViewParam{ 90, 260, 120, 30 }, btn_pause_id, hwnd);
	btn_stop = createButton("Stop", new ViewParam{ 230, 260, 120, 30 }, btn_stop_id, hwnd);

	//прогресс бар
	pb_progress = createProgressBar(new ViewParam{ 10, 300, 410, 30 }, NULL, hwnd);
}

VOID onCommand(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	// Если wParam = CBN_SELCHANGE, значит было выбрано одно из полей выпадающего списка
	if (HIWORD(wParam) == CBN_SELCHANGE) {
		DWORD selectedId;
		selectedId = SendMessage(HWND(lParam), CB_GETCURSEL, 0, 0); // Индекс поля выпадающего списка

		if (HWND(lParam) == cb_list_buffers)
			userConfig.bufferSize = buffSizes[selectedId];
		else if (HWND(lParam) == cb_list_files)
			userConfig.fileSize = fileSizes[selectedId];
		else if (HWND(lParam) == cb_list_disks)
			userConfig.disk = disks[selectedId];
		else if (HWND(lParam) == cb_list_testCounts)
			userConfig.countTests = selectedId + 1;
	}

	//нажата одна из radio button
	if (HIWORD(wParam) == BN_CLICKED) {
		for (DWORD i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i)
			if (HWND(lParam) == rb_group_modes[i])
				userConfig.mode = modes[i];
	}

	// Управление потоком тестирования (самим тестом)
	if (HWND(lParam) == btn_startWrite)
		startTest(WRITE_TEST);

	if (HWND(lParam) == btn_startRead)
		startTest(READ_TEST);

	if (HWND(lParam) == btn_pause)
		pauseTest();

	if (HWND(lParam) == btn_stop)
		stopTest();

	if (HWND(lParam) == cb_buffering) {
		BOOL checked = IsDlgButtonChecked(hwnd, cb_buffering_id);
		if (checked) 
			CheckDlgButton(hwnd, cb_buffering_id, BST_UNCHECKED);
		else 
			CheckDlgButton(hwnd, cb_buffering_id, BST_CHECKED);
		userConfig.isBuffering = !checked;
	}
}

//определение дисков
VOID getDisks() {
	
	DWORD dr = GetLogicalDrives();

	DWORD countDisk = 0; // Счетчик для записи в массив всех дисков

	for (DWORD i = 0; i < 26; i++)
	{
		BOOL isFound = ((dr >> i) & 0x00000001); // Все диски представляются битовой маской, если бит 0 установлен в единицу, значит существует диск А и так далее по алфавиту
		if (isFound) {
			//получем букву диска
			TCHAR diskChar = i + 65; // Имя диска
			disks[countDisk] = new TCHAR[5]{ diskChar, ':', '\\', '\0' }; // Создание строки типа пути диска "X:\"

			//получаем имя диска
			TCHAR* VolumeName = new TCHAR[MAX_PATH];
			GetVolumeInformation(disks[countDisk], VolumeName, MAX_PATH, NULL, NULL, NULL, NULL, 0);

			//создаем название из буквы диска и имени диска, если у диска нету названия, даем имя Local Disk
			TCHAR* name = new TCHAR[MAX_PATH + 5]{ '(', diskChar, ':', ')', ' ' };
			if (_tcslen(VolumeName) == 0)
				_tcscat_s(name, MAX_PATH + 5, _T("Local Disk"));
			else 
				_tcscat_s(name, MAX_PATH + 5, VolumeName);

			disksNames[countDisk] = name;
			++countDisk;
		}
	}
}

// Инициализация дефолтный юзерских настроек
VOID initConfig() {
	//определение дисков
	getDisks();

	// Инциализация первоначального состояния юзер конфига
	userConfig.bufferSize = buffSizes[0];
	userConfig.countTests = 1;
	userConfig.fileSize = fileSizes[0];
	userConfig.mode = modes[0];
	userConfig.disk = disks[0];
	userConfig.isBuffering = FALSE;
}

/////////////////////////////////// Управление тестом ///////////////////////////////////
VOID startTest(DWORD typeTest) {
	userConfig.typeTest = typeTest;
	userConfig.parentThreadId = GetCurrentThreadId(); // Получение текущего id потока который будет являться родительским для теста

	workingThread = CreateThread(NULL, 0, testDrive, (LPVOID)&userConfig, CREATE_SUSPENDED | THREAD_SUSPEND_RESUME, NULL); // Создание потока 

	EnableWindow(btn_startWrite, FALSE);
	EnableWindow(btn_startRead, FALSE);
	threadStatus = WORKING;
	ResumeThread(workingThread);
}

VOID pauseTest() {
	Sleep(50);

	// Обработка кнопки "Pause" и "Resume" соответственно, если поток остановлен - восстановить, иначе приостановить
	if (threadStatus != PAUSE) {
		threadStatus = PAUSE;
		SetWindowText(btn_pause, "Resume");
	}
	else {
		threadStatus = WORKING;
		SetWindowText(btn_pause, "Pause");
		ResumeThread(workingThread);
	}
}

VOID stopTest() {
	EnableWindow(btn_startWrite, TRUE);
	EnableWindow(btn_startRead, TRUE);
	SetWindowText(btn_pause, "Pause");
	threadStatus = CANCELED;
	ResumeThread(workingThread); // Завершаем поток естественным образом
}

// Установка результатов 
VOID setResult(TCHAR* res) {
	if (userConfig.typeTest == WRITE_TEST)
		SetWindowText(text_write, res);
	else 
		SetWindowText(text_read, res);

	SendMessage(pb_progress, PBM_SETPOS, 0, 0);
	EnableWindow(btn_startWrite, TRUE);
	EnableWindow(btn_startRead, TRUE);
}

/////////////////////////////////// Функции для создания графических элементов ///////////////////////////////////

// Установка шрифта
VOID setFont(HWND hwnd, DWORD size, DWORD weight) {
	HFONT font = CreateFont(size, 0, 0, 0, weight, FALSE, FALSE, FALSE, ANSI_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
	SendMessage(hwnd, WM_SETFONT, WPARAM(font), TRUE);
}

VOID createBox(CONST TCHAR* nameBtn, ViewParam* params, HWND& hwnd, DWORD atr) {
	HWND box;
	box = CreateWindow("Button", nameBtn, WS_CHILD | WS_VISIBLE | BS_GROUPBOX | atr, params->x - 20, params->y - 20, params->width + 20, params->height + 20, hwnd, NULL, NULL, NULL);
	setFont(box, 16, FW_MEDIUM);
}

HWND createCombobox(CONST TCHAR* nameBox, ViewParam* params, CONST TCHAR* values[], DWORD countValues, DWORD id, HWND& hwnd) {
	HWND dropList;
	dropList = CreateWindow("combobox", nameBox, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST, params->x, params->y, params->width, params->height, hwnd, (HMENU)id, NULL, NULL);
	for (DWORD i = 0; i < countValues; ++i)
		SendMessage(dropList, CB_ADDSTRING, 0, LPARAM(values[i]));
	SendMessage(dropList, CB_SETCURSEL, 0, 0);
	setFont(dropList, 16, FW_MEDIUM);
	return dropList;
}

HWND createButton(CONST TCHAR* nameBtn, ViewParam* params, DWORD id, HWND& hwnd) {
	HWND btn;
	btn = CreateWindow("button", nameBtn, WS_VISIBLE | WS_CHILD, params->x, params->y, params->width, params->height, hwnd, (HMENU)id, NULL, NULL);
	setFont(btn, 17, FW_BOLD);
	return btn;
}

HWND createText(CONST TCHAR* nameBtn, ViewParam* params, DWORD id, HWND& hwnd, DWORD isBold, DWORD size) {
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

HWND* createRadioBtnGroup(CONST TCHAR* nameBtn, ViewParam* params, CONST TCHAR* values[], DWORD countValues, DWORD id, HWND& hwnd) {
	HWND* btns = new HWND[countValues];
	DWORD y = params->y;
	for (DWORD i = 0; i < countValues; ++i) {
		btns[i] = CreateWindow("button", modes[i], WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, params->x, y, params->width, params->height, hwnd, (HMENU)id, NULL, NULL);
		setFont(btns[i], 15, FW_MEDIUM);
		y += 20;
	}

	SendMessage(btns[0], BM_SETCHECK, BST_CHECKED, 0);

	return btns;
}

HWND createCheckBox(CONST TCHAR* text, ViewParam* params, DWORD id, HWND& hwnd) {
	HWND btn;
	btn = CreateWindow("button", text, WS_VISIBLE | WS_CHILD | BS_CHECKBOX, params->x, params->y, params->width, params->height, hwnd, (HMENU)id, NULL, NULL);
	setFont(btn, 15, FW_MEDIUM);
	return btn;
}
