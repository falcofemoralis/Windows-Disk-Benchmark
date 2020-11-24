#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <CommCtrl.h>

extern HWND pb_progress, text_read, text_write, btn_startWrite, btn_startRead;
extern DWORD threadStatus;

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
#define btn_startRead_id ID_BTN + 2
#define btn_startWrite_id ID_BTN + 3
#define cb_list_files_id ID_CB
#define cb_list_disks_id ID_CB + 1
#define cb_list_buffers_id ID_CB + 2
#define cb_list_testCounts_id ID_CB + 3
#define cb_list_modes_id ID_CB + 4
#define text_read_id ID_TEXT
#define text_write_id ID_TEXT + 1	


// Сообщения между тредами
#define SEND_TEST_RESULT WM_APP + 1

LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

/////////////////////////////////// Функции для создания графических элементов ///////////////////////////////////
// Структура с параметрами view объектов: x,y - координаты объекта, width,height - размеры view
struct ViewParam {
	DWORD x, y;
	DWORD width, height;
};
void createBox(const TCHAR* nameBtn, ViewParam* params, HWND& hwnd, DWORD atr);
HWND createCombobox(const TCHAR* nameBox, ViewParam* params, const TCHAR* values[], DWORD countValues, DWORD id, HWND& hwnd);
HWND createButton(const TCHAR* nameBtn, ViewParam* params, DWORD id, HWND& hwnd);
HWND createText(const TCHAR* nameBtn, ViewParam* params, DWORD id, HWND& hwnd, DWORD isBold, DWORD size);
HWND createProgressBar(ViewParam* params, DWORD id, HWND& hwnd);
HWND* createRadiobtnGroup(const TCHAR* nameBtn, ViewParam* params, const TCHAR* values[], DWORD countValues, DWORD id, HWND& hwnd);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Инициализация всех необходимых первоначальных данных
void init();
void drawMainWindow(HWND hwnd);
void startTest(DWORD(*test)(LPVOID param));
void pauseTest();
void stopTest();