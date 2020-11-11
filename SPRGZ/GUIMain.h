#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <CommCtrl.h>

#define PAUSE 2
#define WORKING 1
#define CANCELED 0

extern HWND pb_progress, text_read, text_write, btn_start;
extern DWORD threadStatus;

