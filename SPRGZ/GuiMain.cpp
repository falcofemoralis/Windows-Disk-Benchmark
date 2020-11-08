#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

#define ID_BT1 1
#define ID_BT2 2

#define cb_list_types_id 1
#define cb_list_buffers_id 2

#define KB 1024
#define MB KB*1024
#define GB MB*1024

LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

const char* types[] = { "WRITE_THROUGH", "RANDOM_ACCESS", "SEQUENTIAL", "Etc", "NO_BUFFERING"};
const char* buffNames[] = { "1 KB", "4 KB", "8 KB", "1 MB", "2 MB", "4 MB", "8 MB", "16 MB" };
int buffSizes[] = { 1 * KB, 4 * KB, 8 * KB, 1 * MB, 2 * MB, 4 * MB, 8 * MB, 16 * MB };

struct Config {
	int bufferSize;
	const char* type;
	const char* diskPath;
};

Config userConfig;


int main() {
	WNDCLASS wcl;

	memset(&wcl, 0, sizeof(WNDCLASS));
	wcl.lpszClassName = "my window";
	wcl.lpfnWndProc = WndProc;
	RegisterClass(&wcl);

	HWND hwnd;
	hwnd = CreateWindow("my Window", "Window Name", WS_OVERLAPPEDWINDOW, 10, 10, 640, 480, NULL, NULL, NULL, NULL);

	ShowWindow(hwnd, SW_SHOWNORMAL);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		DispatchMessage(&msg);
	}

	return 0;
}

// Структура с параметрами view объектов: x,y - координаты объекта, width,height - размеры view
struct ViewParam {
	int x, y;
	int width, height;
};

// Функция создания Combobox
void createCombobox(const char* nameBox, ViewParam& params, const char* values[], int countVelues, int id, HWND& hwnd) {
	HWND dropList;
	dropList = CreateWindow("combobox", nameBox, WS_VISIBLE | WS_CHILD | CBS_DROPDOWN, params.x, params.y, params.width, params.height, hwnd, (HMENU)id, NULL, NULL);

	for (int i = 0; i < countVelues; ++i)
		SendMessage(dropList, CB_ADDSTRING, 0, LPARAM(values[i]));
	SendMessage(dropList, CB_SETCURSEL, 1, 0);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

	switch (message) {
		case WM_CREATE:
			ViewParam params;
			params = { 10, 10, 150, 500 };
			createCombobox("Типы", params, types, sizeof(types) / sizeof(types[0]), cb_list_types_id, hwnd);
			params = { 200, 10, 150, 500 };
			createCombobox("Размер буфера", params, buffNames, sizeof(buffNames) / sizeof(buffNames[0]), cb_list_buffers_id, hwnd);
			break;
		case WM_COMMAND:

			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				int selectedId = SendMessage(HWND(lParam), CB_GETCURSEL, 0, 0);
				switch (LOWORD(wParam))
				{
				case cb_list_types_id:
					userConfig.type = types[selectedId];
					break;
				case cb_list_buffers_id:
					userConfig.bufferSize = buffSizes[selectedId];
					break;
				}
			}

			break;
		case WM_DESTROY:
			PostQuitMessage(0);
		break;
	}

	return DefWindowProcA(hwnd, message, wParam, lParam);
}
