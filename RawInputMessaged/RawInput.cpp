///////////////////////////////////////////////////////////////////////////////
//
// Raw Input API sample showing joystick support
//
// Author: Alexander Böcken
// Date:   04/22/2011
//
// Copyright 2011 Alexander Böcken
//
///////////////////////////////////////////////////////////////////////////////


#include <Windows.h>
#include <tchar.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <hidsdi.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>


#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))
#define WC_MAINFRAME	TEXT("MainFrame")
#define MAX_BUTTONS		128
#define CHECK(exp)		{ if(!(exp)) goto Error; }
#define SAFE_FREE(p)	{ if(p) { HeapFree(hHeap, 0, p); (p) = NULL; } }

static HWND g_hWnd;

void ParseRawInput(PRAWINPUT pRawInput);

static const char *hex = "0123456789ABCDEF";

static LRESULT CALLBACK SDL_HelperWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	char buf[1024];
	switch (msg)
	{
		case WM_INPUT_DEVICE_CHANGE:
		{
			HANDLE hDevice = (HANDLE)lParam;
			switch (wParam) {
			case GIDC_ARRIVAL:
				sprintf_s(buf, "Device %08p: Added\n", hDevice);
				break;
			case GIDC_REMOVAL:
				sprintf_s(buf, "Device %08p: Removed\n", hDevice);
				break;
			default:
				return 0;
			}
			OutputDebugStringA(buf);
		}
		return 0;
		case WM_INPUT:
		{
			PRAWINPUT pRawInput;
			UINT      bufferSize;
			HANDLE    hHeap;

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &bufferSize, sizeof(RAWINPUTHEADER));

			hHeap = GetProcessHeap();
			pRawInput = (PRAWINPUT)HeapAlloc(hHeap, 0, bufferSize);
			if (!pRawInput)
				return 0;

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, pRawInput, &bufferSize, sizeof(RAWINPUTHEADER));
			sprintf_s(buf, "Device %08p: ", pRawInput->header.hDevice);
			char *out = buf + strlen(buf);
			for (unsigned int ii = 0; ii < pRawInput->data.hid.dwSizeHid; ii++) {
				unsigned char byte = pRawInput->data.hid.bRawData[ii];
				sprintf_s(out, 3, "%02X", byte);
				out += 2;
			}
			*out++ = '\n';
			*out++ = 0;
			OutputDebugStringA(buf);
			// &pRawInput->data.hid.bRawData[1] is the state packet that SDL's hidapi knows how to read already
			ParseRawInput(pRawInput);

			HeapFree(hHeap, 0, pRawInput);

			InvalidateRect(g_hWnd, NULL, TRUE);
			UpdateWindow(g_hWnd);
		}
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}


static WCHAR *SDL_HelperWindowClassName = TEXT("SDLHelperWindowInputCatcher");
static WCHAR *SDL_HelperWindowName = TEXT("SDLHelperWindowInputMsgWindow");
static ATOM SDL_HelperWindowClass = 0;
static HWND SDL_HelperWindow;
int SDL_HelperWindowCreate(void)
{
	HINSTANCE hInstance = GetModuleHandle(NULL);
	WNDCLASS wce = {};

	/* Make sure window isn't created twice. */
	if (SDL_HelperWindow != NULL) {
		return 0;
	}

	/* Create the class. */
	wce.lpfnWndProc = SDL_HelperWindowProc;
	wce.lpszClassName = (LPCWSTR)SDL_HelperWindowClassName;
	wce.hInstance = hInstance;

	/* Register the class. */
	SDL_HelperWindowClass = RegisterClass(&wce);
	if (SDL_HelperWindowClass == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		assert(!"Unable to create Helper Window Class");
		return -1;
	}

	/* Create the window. */
	SDL_HelperWindow = CreateWindowEx(0, SDL_HelperWindowClassName,
		SDL_HelperWindowName,
		WS_OVERLAPPED, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, HWND_MESSAGE, NULL,
		hInstance, NULL);
	if (SDL_HelperWindow == NULL) {
		UnregisterClass(SDL_HelperWindowClassName, hInstance);
		assert(!"Unable to create Helper Window");
		return -1;
	}

	return 0;
}



//
// Global variables
//

BOOL bButtonStates[MAX_BUTTONS];
LONG lAxisX;
LONG lAxisY;
LONG lAxisZ;
LONG lAxisRz;
LONG lHat;
INT  g_NumberOfButtons;


void ParseRawInput(PRAWINPUT pRawInput)
{
	PHIDP_PREPARSED_DATA pPreparsedData;
	HIDP_CAPS            Caps;
	PHIDP_BUTTON_CAPS    pButtonCaps;
	PHIDP_VALUE_CAPS     pValueCaps;
	USHORT               capsLength;
	UINT                 bufferSize;
	HANDLE               hHeap;
	USAGE                usage[MAX_BUTTONS];
	ULONG                i, usageLength, value;

	pPreparsedData = NULL;
	pButtonCaps    = NULL;
	pValueCaps     = NULL;
	hHeap          = GetProcessHeap();

	//
	// Get the preparsed data block
	//

	CHECK( GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, NULL, &bufferSize) == 0 );
	CHECK( pPreparsedData = (PHIDP_PREPARSED_DATA)HeapAlloc(hHeap, 0, bufferSize) );
	CHECK( (int)GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, pPreparsedData, &bufferSize) >= 0 );

	//
	// Get the joystick's capabilities
	//

	// Button caps
	CHECK( HidP_GetCaps(pPreparsedData, &Caps) == HIDP_STATUS_SUCCESS )
	CHECK( pButtonCaps = (PHIDP_BUTTON_CAPS)HeapAlloc(hHeap, 0, sizeof(HIDP_BUTTON_CAPS) * Caps.NumberInputButtonCaps) );

	capsLength = Caps.NumberInputButtonCaps;
	CHECK( HidP_GetButtonCaps(HidP_Input, pButtonCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS )
	g_NumberOfButtons = pButtonCaps->Range.UsageMax - pButtonCaps->Range.UsageMin + 1;

	// Value caps
	CHECK( pValueCaps = (PHIDP_VALUE_CAPS)HeapAlloc(hHeap, 0, sizeof(HIDP_VALUE_CAPS) * Caps.NumberInputValueCaps) );
	capsLength = Caps.NumberInputValueCaps;
	CHECK( HidP_GetValueCaps(HidP_Input, pValueCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS )

	//
	// Get the pressed buttons
	//

	usageLength = g_NumberOfButtons;
	CHECK(
		HidP_GetUsages(
			HidP_Input, pButtonCaps->UsagePage, 0, usage, &usageLength, pPreparsedData,
			(PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid
		) == HIDP_STATUS_SUCCESS );

	ZeroMemory(bButtonStates, sizeof(bButtonStates));
	for(i = 0; i < usageLength; i++)
		bButtonStates[usage[i] - pButtonCaps->Range.UsageMin] = TRUE;

	//
	// Get the state of discrete-valued-controls
	//

	for(i = 0; i < Caps.NumberInputValueCaps; i++)
	{
		CHECK(
			HidP_GetUsageValue(
				HidP_Input, pValueCaps[i].UsagePage, 0, pValueCaps[i].Range.UsageMin, &value, pPreparsedData,
				(PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid
			) == HIDP_STATUS_SUCCESS );

		switch(pValueCaps[i].Range.UsageMin)
		{
		case 0x30:	// X-axis
			lAxisX = (LONG)(value - 32768) / 256;
			break;

		case 0x31:	// Y-axis
			lAxisY = (LONG)(value - 32768) / 256;
			break;

		case 0x33:
			lAxisZ = (LONG)(value - 32768) / 256;
			break;

		case 0x34:
			lAxisRz = (LONG)(value - 32768) / 256;
			break;

		case 0x39:	// Hat Switch
			lHat = value;
			break;
		}
	}

	//
	// Clean up
	//

Error:
	SAFE_FREE(pPreparsedData);
	SAFE_FREE(pButtonCaps);
	SAFE_FREE(pValueCaps);
}


void DrawButton(HDC hDC, int i, int x, int y, BOOL bPressed)
{
	HBRUSH hOldBrush, hBr;
	TCHAR  sz[4];
	RECT   rc;

	if(bPressed)
	{
		hBr       = CreateSolidBrush(RGB(192, 0, 0));
		hOldBrush = (HBRUSH)SelectObject(hDC, hBr);
	}

	rc.left   = x;
	rc.top    = y;
	rc.right  = x + 30;
	rc.bottom = y + 30;
	Ellipse(hDC, rc.left, rc.top, rc.right, rc.bottom);
	_stprintf_s(sz, ARRAY_SIZE(sz), TEXT("%d"), i);
	DrawText(hDC, sz, -1, &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

	if(bPressed)
	{
		SelectObject(hDC, hOldBrush);
		DeleteObject(hBr);
	}
}


void DrawCrosshair(HDC hDC, int x, int y, LONG xVal, LONG yVal)
{
	Rectangle(hDC, x, y, x + 256, y + 256);
	MoveToEx(hDC, x + xVal - 5 + 128, y + yVal + 128, NULL);
	LineTo(hDC, x + xVal + 5 + 128, y + yVal + 128);
	MoveToEx(hDC, x + xVal + 128, y + yVal - 5 + 128, NULL);
	LineTo(hDC, x + xVal + 128, y + yVal + 5 + 128);
}


void DrawDPad(HDC hDC, int x, int y, LONG value)
{
	LONG i;

	for(i = 0; i < 8; i++)
	{
		HBRUSH hOldBrush;
		HBRUSH hBr;
		int xPos = (int)(sin(-2 * M_PI * i / 8 + M_PI) * 80.0) + 80;
		int yPos = (int)(cos(2 * M_PI * i / 8 + M_PI) * 80.0) + 80;

		if(value == i)
		{
			hBr       = CreateSolidBrush(RGB(192, 0, 0));
			hOldBrush = (HBRUSH)SelectObject(hDC, hBr);
		}

		Ellipse(hDC, x + xPos, y + yPos, x + xPos + 20, y + yPos + 20);

		if(value == i)
		{
			SelectObject(hDC, hOldBrush);
			DeleteObject(hBr);
		}
	}
}


LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
	case WM_PAINT:
		{
			//
			// Draw the buttons and axis-values
			//

			PAINTSTRUCT ps;
			HDC         hDC;
			int         i;

			hDC = BeginPaint(hWnd, &ps);
			SetBkMode(hDC, TRANSPARENT);

			for(i = 0; i < g_NumberOfButtons; i++)
				DrawButton(hDC, i+1, 20 + i * 40, 20, bButtonStates[i]);
			DrawCrosshair(hDC, 20, 100, lAxisX, lAxisY);
			DrawCrosshair(hDC, 296, 100, lAxisZ, lAxisRz);
			DrawDPad(hDC, 600, 140, lHat);

			EndPaint(hWnd, &ps);
		}
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void CALLBACK tick(HWND hWnd, UINT Arg2, UINT_PTR Arg3, DWORD Arg4)
{
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	HWND hWnd;
	MSG msg;
	WNDCLASSEX wcex;


	SDL_HelperWindowCreate();

	//
	// Register window class
	//

	wcex.cbSize        = sizeof(WNDCLASSEX);
	wcex.cbClsExtra    = 0;
	wcex.cbWndExtra    = 0;
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wcex.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hInstance     = hInstance;
	wcex.lpfnWndProc   = WindowProc;
	wcex.lpszClassName = WC_MAINFRAME;
	wcex.lpszMenuName  = NULL;
	wcex.style         = CS_HREDRAW | CS_VREDRAW;

	if(!RegisterClassEx(&wcex))
		return -1;

	//
	// Create window
	//

	g_hWnd = hWnd = CreateWindow(WC_MAINFRAME, TEXT("Joystick using Raw Input API"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
	ShowWindow(hWnd, nShowCmd);
	UpdateWindow(hWnd);


	//
// Register for joystick devices
//

	RAWINPUTDEVICE rid[2] = {};

	rid[0].usUsagePage = 1;
	rid[0].usUsage = 4;	// Joystick
	rid[0].dwFlags = RIDEV_DEVNOTIFY | RIDEV_INPUTSINK; // Receive messages when in background
	rid[0].hwndTarget = SDL_HelperWindow;

	rid[1].usUsagePage = 1;
	rid[1].usUsage = 5;	// Gamepad - e.g. XBox 360 or XBox One controllers
	rid[1].dwFlags = RIDEV_DEVNOTIFY | RIDEV_INPUTSINK; // Receive messages when in background
	rid[1].hwndTarget = SDL_HelperWindow;

	if (!RegisterRawInputDevices(&rid[0], 2, sizeof(RAWINPUTDEVICE)))
		return -1;

	//
	// Message loop
	//
	SetTimer(hWnd, 1, 16, tick);

	while(GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}
