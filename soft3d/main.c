// soft3d by Andrej Suvorau, 2019

#include "soft3d.h"

#include <stdio.h>
#include <Windows.h>

#define BACKBUFFER_WIDTH 800
#define BACKBUFFER_HEIGHT 600

extern void potato_init();
extern void potato_update();
extern void potato_destroy();

extern DepthColorBuffer backbuffer;

HWND hwnd;
HBITMAP bitmap;
HDC hdc;
HDC src;

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			SetTimer(hwnd, 1, USER_TIMER_MINIMUM, NULL);
			break;
		case WM_TIMER:
			InvalidateRect(hwnd, NULL, FALSE);
			break;
		case WM_PAINT: {
			LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
			LARGE_INTEGER Frequency;

			QueryPerformanceFrequency(&Frequency);
			QueryPerformanceCounter(&StartingTime);

			potato_update();

			QueryPerformanceCounter(&EndingTime);
			ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;

			ElapsedMicroseconds.QuadPart *= 1000000;
			ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;

			char buffer[512] = { 0 };
			sprintf_s(buffer, 512, "soft3d %f ms %f fps", ElapsedMicroseconds.QuadPart / 1000.f, 1000000.f / ElapsedMicroseconds.QuadPart);

			SetWindowTextA(hwnd, buffer);

			BITMAPINFO info = {0};
			info.bmiHeader.biSize = sizeof(info.bmiHeader);
			info.bmiHeader.biWidth = BACKBUFFER_WIDTH;
			info.bmiHeader.biHeight = -BACKBUFFER_HEIGHT;
			info.bmiHeader.biPlanes = 1;
			info.bmiHeader.biBitCount = 32;
			info.bmiHeader.biCompression = BI_RGB;

			SetDIBits(src, bitmap, 0, BACKBUFFER_HEIGHT, (void*)backbuffer.data, &info, DIB_RGB_COLORS);
			SelectObject(src, bitmap);
			BitBlt(hdc, 0, 0, BACKBUFFER_WIDTH, BACKBUFFER_HEIGHT, src, 0, 0, SRCCOPY);

			return DefWindowProc(hwnd, message, wParam, lParam);
		}
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}

ATOM RegisterWindowClass(HINSTANCE hInstance, LPCWSTR lpzClassName) {
	WNDCLASS wcWindowClass		= { 0 };
	wcWindowClass.lpfnWndProc	= (WNDPROC)WndProc;
	wcWindowClass.style			= CS_HREDRAW | CS_VREDRAW;
	wcWindowClass.hInstance		= hInstance;
	wcWindowClass.lpszClassName = lpzClassName;
	wcWindowClass.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcWindowClass.hbrBackground	= (HBRUSH)COLOR_APPWORKSPACE;
	return RegisterClass(&wcWindowClass);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	LPCWSTR lpzClass = TEXT("soft3d");
	if (!RegisterWindowClass(hInstance, lpzClass)) {
		return EXIT_FAILURE;
	}

	const DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;

	RECT clientArea = { 0 };
	clientArea.right = BACKBUFFER_WIDTH;
	clientArea.bottom = BACKBUFFER_HEIGHT;
	if (!AdjustWindowRect(&clientArea, style, FALSE)) {
		return EXIT_FAILURE;
	}

	hwnd = CreateWindow(lpzClass, TEXT("soft3d"), style, CW_USEDEFAULT, CW_USEDEFAULT, clientArea.right, clientArea.bottom, NULL, NULL, hInstance, NULL);
	if (!hwnd) {
		return EXIT_FAILURE;
	}

	hdc = GetDC(hwnd);
	if (!hdc) {
		return EXIT_FAILURE;
	}

	potato_init();

	bitmap = CreateBitmap(BACKBUFFER_WIDTH, BACKBUFFER_HEIGHT, 1, 32, (void*)backbuffer.data);
	if (!bitmap) {
		return EXIT_FAILURE;
	}

	src = CreateCompatibleDC(hdc);
	if (!src) {
		return EXIT_FAILURE;
	}

	MSG msg = { 0 };
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	potato_destroy();

	DeleteObject(bitmap);
	DeleteDC(src);
	DestroyWindow(hwnd);

	return (int)msg.wParam;
}
