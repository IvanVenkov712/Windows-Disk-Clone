// Windows Disk Imager.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "WindowsCloneDisk.h"
#include "HandleDisk.h"

#define MAX_LOADSTRING         100
#define IDW_SRCCB              200
#define IDW_DSTCB              201
#define IDW_GOBTN              202
#define IDW_BROWSEBTN          203
#define IDW_PROGRESSBAR        204
#define IDW_STATICPROGRESSVIEW 205
#define MAINWND_WIDTH          800
#define MAINWND_HEIGHT         320

typedef struct _COPYFILETHREADPARAMS
{
	DWORD srcIndex;
	DWORD dstIndex;
	HWND hProgressTracker;
	HWND hTextProgressTracker;
	HWND hButtonToBlock;
	HWND hMsgParentWnd;
} COPYFILETHREADPARAMS;


// Global Variables:
PCBDISKENTRY availableDrives = nullptr;
DWORD availableDrivesCount = 0;
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:


/// <summary>
///		Registers the window class.
/// </summary>
/// <param name="hInstance">
///		Handle to the instance
/// </param>
/// <returns></returns>
ATOM MyRegisterClass(HINSTANCE hInstance);


/// <summary>
///		Saves instance handle and creates main window
/// </summary>
/// <param name="hInstance"></param>
/// <param name="nCmdShow"></param>
/// <returns>
/// </returns>
/// <comment>
///		In this function, the instance handle is saved in a global variable and
///		the main program window is created and displayed.
/// </comment>
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow);


/// <summary>
///		Returns the nearest bigger than or equal to ptr value aligned to the given alignment
/// </summary>
/// <param name="ptr">The given pointer</param>
/// <param name="alignment">The alignment, in bytes. Must be a power of 2.</param>
/// <returns>The aligned pointer</returns>
LPVOID PtrAlignTo(LPCVOID ptr, UINT_PTR alignment);

/// <summary>
///		Creates all the necessary controls for the 
///		program to work with their parent hMain and sets the default program's
///		font to each control. Called in WM_CREATE.
/// </summary>
/// <param name="hMain">
///		-The main window
/// </param>
/// <returns>
///		0 on success or -1 on failure
/// </returns>
LRESULT InitControls(HWND hMain);


/// <summary>
///		Processes messages for the main window.
///		WM_COMMAND  - process the application menu and button clicks
///		WM_CREATE   - add all controls neccesary for the program to run
///		WM_PAINT    - Paint the main window
///		WM_DESTROY  - post a quit message and return
/// </summary>
/// <param name="hWnd"></param>
/// <param name="message"></param>
/// <param name="wParam"></param>
/// <param name="lParam"></param>
/// <returns>LRESULT</returns>
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

/// <summary>
///		Message handler for about box.
/// </summary>
/// <param name="hDlg"></param>
/// <param name="message"></param>
/// <param name="wParam"></param>
/// <param name="lParam"></param>
/// <returns></returns>
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

/// <summary>
///		Callback that clones the contents of the source disk
///		to the destination reporting the progress.
///		In the end tells the user if the operation succeeded
///		showing MessageBox for success or or formatted error message.
///		Called from another thread. Frees threadParams.
/// </summary>
/// <param name="threadParams">
///		COPYFILETHREADPARAMS - Structure containing the source and destination strings and handle to the ProgressBar
/// </param>
/// <returns>BOOL: TRUE if operation succeeded, otherwise FALSE</returns>
DWORD CALLBACK CopyFileThreadProc(LPVOID threadParams);

/// <summary>
///		Shows an error MessageBox containing a formatted
///		error message based on the error code.
///		FormatMesage is used.
/// </summary>
/// <param name="hParent">
///		The parent of the shown error MessageBox.
/// </param>
/// <param name="errorCode">
///		The error code to be used to fromat the message.
/// </param>
/// <returns>
///		The result of the shown MessageBox.
/// </returns>
int ShowErrorMessage(HWND hParent, DWORD errorCode);

int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd
)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	// TODO: Place code here.
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if(!SUCCEEDED(hr)) return(1);
	if (!GetLocalDiskCBEntries(&availableDrives, &availableDrivesCount))
	{
		CoUninitialize();
		return(1);
	}
	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_WINDOWSCLONEDISK, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nShowCmd)) return(FALSE);

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDOWSCLONEDISK));

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	CoUninitialize();
	return msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = WndProc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hInstance;
	wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSCLONEDISK));
	wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_WINDOWSCLONEDISK);
	wcex.lpszClassName  = szWindowClass;
	wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle,
	   WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
	   (GetSystemMetrics(SM_CXSCREEN) - MAINWND_WIDTH) / 2, 
	   (GetSystemMetrics(SM_CYSCREEN) - MAINWND_HEIGHT) / 2,
	   MAINWND_WIDTH, MAINWND_HEIGHT, nullptr, nullptr, hInst, nullptr
   );

   if (!hWnd)
   {
	   return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

LPVOID PtrAlignTo(LPCVOID ptr, UINT_PTR alignment)
{
	if(!(alignment && ((alignment & (alignment - 1)) == 0))) return nullptr;
	UINT_PTR intVal = (UINT_PTR)ptr;
	UINT_PTR remainder = (alignment - 1) & intVal;
	if (remainder != 0)
	{
		intVal += alignment - remainder;
	}
	return (LPVOID)intVal;
}


LRESULT InitControls(HWND hMain)
{
	RECT wndRect{};
	GetClientRect(hMain, &wndRect);
	const int mainWndWidth = wndRect.right - wndRect.left;

	HFONT defFont = CreateFontW(
		20, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
		ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
	);
	if (!defFont) return -1;

	HWND hSrcLabel = CreateWindowW(
		WC_STATICW, L"Choose source disk: ",
		WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
		15, 20, 150, 30,
		hMain, nullptr, hInst, nullptr
	);
	if (!hSrcLabel) return -1;
	SendMessage(hSrcLabel, WM_SETFONT, (WPARAM)defFont, TRUE);

	HWND hSrcCB = CreateWindowW(
		WC_COMBOBOXW, L"",
		WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST | CBS_AUTOHSCROLL,
		170, 20, mainWndWidth - 185, 30,
		hMain, (HMENU)IDW_SRCCB, hInst, nullptr
	);
	if (!hSrcCB) return -1;
	if (!CBSetListToCBDISKENTRYList(hSrcCB, availableDrives, availableDrivesCount)) return -1;
	SendMessage(hSrcCB, WM_SETFONT, (WPARAM)defFont, TRUE);
	
	HWND hDstLabel = CreateWindowW(
		WC_STATICW, L"Choose target disk: ",
		WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
		15, 60, 150, 30,
		hMain, nullptr, hInst, nullptr
	);
	if (!hDstLabel) return -1;
	SendMessage(hDstLabel, WM_SETFONT, (WPARAM)defFont, TRUE);
	HWND hDstCB = CreateWindowW(
		WC_COMBOBOXW, L"",
		WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST | CBS_AUTOHSCROLL,
		170, 60, mainWndWidth - 185, 30,
		hMain, (HMENU)IDW_DSTCB, hInst, nullptr
	);
	if (!hDstCB) return -1;
	SendMessage(hDstCB, WM_SETFONT, (WPARAM)defFont, TRUE);
	if (!CBSetListToCBDISKENTRYList(hDstCB, availableDrives, availableDrivesCount)) return -1;

	HWND hGoButton = CreateWindowW(
		WC_BUTTONW, L"Clone Disk to Source",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_USERBUTTON,
		(mainWndWidth - 200) / 2, 110, 200, 40,
		hMain, (HMENU)IDW_GOBTN, hInst, nullptr
	);
	if (!hGoButton) return -1;
	SendMessage(hGoButton, WM_SETFONT, (WPARAM)defFont, TRUE);

	HWND hProgressBar = CreateWindowW(
		PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE,
		15, 170, mainWndWidth - 30, 20, hMain, (HMENU)IDW_PROGRESSBAR,
		hInst, nullptr
	);
	if (!hProgressBar) return -1;

	HWND hStatic = CreateWindowW(
		WC_STATICW, L"0% completed", 
		WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
		15, 200, mainWndWidth - 30, 30, hMain,
		(HMENU)IDW_STATICPROGRESSVIEW,
		hInst, nullptr
	);
	if (!hStatic) return -1;
	SendMessage(hStatic, WM_SETFONT, (WPARAM)defFont, TRUE);
	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		{
			 return InitControls(hWnd);
		}
	case WM_COMMAND:
		{
			int wmId = LOWORD(wParam);
			// Parse the menu selections:
			switch (wmId)
			{
			case IDM_ABOUT:
				DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
				break;
			case IDM_REFRESH:
				{
					HWND hSrcCB = GetDlgItem(hWnd, IDW_SRCCB);
					HWND hDstCB = GetDlgItem(hWnd, IDW_DSTCB);

					if (GetLocalDiskCBEntries(&availableDrives, &availableDrivesCount))
					{
						CBSetListToCBDISKENTRYList(hSrcCB, availableDrives, availableDrivesCount);
						CBSetListToCBDISKENTRYList(hDstCB, availableDrives, availableDrivesCount);
					}
					break;
				}
			case IDM_EXIT:
				DestroyWindow(hWnd);
				break;
			case IDW_GOBTN:
				switch (HIWORD(wParam))
				{
				case BN_CLICKED:
					{
						BOOL isOK = TRUE;
						HANDLE hThread = INVALID_HANDLE_VALUE;
						LRESULT srcSelected = SendMessageW(GetDlgItem(hWnd, IDW_SRCCB), CB_GETCURSEL, 0, 0);
						LRESULT dstSelected = SendMessageW(GetDlgItem(hWnd, IDW_DSTCB), CB_GETCURSEL, 0, 0);
						if (srcSelected == CB_ERR || dstSelected == CB_ERR)
						{
							MessageBoxW(
								hWnd,
								L"You have to choose a source and a destination from the comboboxes.",
								L"Warning", MB_ICONWARNING
							);
							break;
						}
						int allows = MessageBoxW(
							hWnd,
							L"All the data in the target disk will be deleted. Do you want to continue?",
							L"Warning", MB_ICONWARNING | MB_YESNO
						);
						if (allows == IDNO)
						{
							break;
						}
						
						if (availableDrives[srcSelected].DiskNumber == 
							availableDrives[dstSelected].DiskNumber
						)
						{
							MessageBoxW(
								hWnd,
								L"The destination and the source cannot be the same", 
								L"Warning", MB_ICONWARNING
							);
							break;
						}
						DISKSIZEINFO srcInfo{};
						DISKSIZEINFO dstInfo{};
						BOOL gotSrcSize = GetDiskSizeInfoByIndex(availableDrives[srcSelected].DiskNumber, &srcInfo);
						BOOL gotDstSize = GetDiskSizeInfoByIndex(availableDrives[dstSelected].DiskNumber, &dstInfo);
						if (!(gotSrcSize && gotDstSize))
						{
							MessageBoxW(
								hWnd,
								L"Could not get disk\'s size.",
								L"Error", MB_ICONERROR
							);
							break;
						}
						if (srcInfo.Size.QuadPart > dstInfo.Size.QuadPart)
						{
							int res = MessageBoxW(
								hWnd,
								L"There is a risk of data loss because the destination\'s size is less than the source\'s size. Do you want to continue?",
								L"Warning", MB_ICONWARNING | MB_YESNO
							);
							if (res == IDNO)
							{
								break;
							}
						}
						//Will be freed when the thread terminates or on an error
						COPYFILETHREADPARAMS* params = 
							(COPYFILETHREADPARAMS*)(HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, sizeof(COPYFILETHREADPARAMS)));
						
						if ((isOK = (params != nullptr)))
						{
							params->hProgressTracker = GetDlgItem(hWnd, IDW_PROGRESSBAR);
							params->hButtonToBlock = GetDlgItem(hWnd, IDW_GOBTN);
							params->hTextProgressTracker = GetDlgItem(hWnd, IDW_STATICPROGRESSVIEW);
							params->hMsgParentWnd = hWnd;
							params->srcIndex = availableDrives[srcSelected].DiskNumber;
							params->dstIndex = availableDrives[dstSelected].DiskNumber;

							hThread = CreateThread(
								nullptr, 0, CopyFileThreadProc, params,
								0, nullptr);
							isOK = (hThread != INVALID_HANDLE_VALUE);
						}
						if (!isOK)
						{
							HeapFree(GetProcessHeap(), 0, params);
							MessageBox(hWnd, L"An error occurred", L"Error", MB_ICONERROR);
						}
						break;
					}
				default:
					return DefWindowProc(hWnd, message, wParam, lParam);
				}
				break;
			default:
				return DefWindowProc(hWnd, message, wParam, lParam);
			}
		}
		break;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			// TODO: Add any drawing code that uses hdc here...
			EndPaint(hWnd, &ps);
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

DWORD CALLBACK CopyFileThreadProc(LPVOID threadParams)
{
	BOOL isOK = TRUE;
	COPYFILETHREADPARAMS* p = (COPYFILETHREADPARAMS*)threadParams;

	EnableWindow(p->hButtonToBlock, FALSE);
	SendMessage(p->hProgressTracker, PBM_SETRANGE32, 0, 100);
	SendMessage(p->hProgressTracker, PBM_SETPOS, 0, 0);
	SetWindowText(p->hTextProgressTracker, L"0% completed");

	HANDLE hSrcDisk = OpenDisk(p->srcIndex, GENERIC_READ);
	HANDLE hDstDisk = OpenDisk(p->dstIndex, GENERIC_READ | GENERIC_WRITE);
	if ((isOK = (hSrcDisk != INVALID_HANDLE_VALUE && hDstDisk != INVALID_HANDLE_VALUE)))
	{
		
		if ((isOK = PrepareDiskForWrite(hDstDisk)))
		{
			DISKSIZEINFO srcSizeInfo{};
			DISKSIZEINFO dstSizeInfo{};
			if ((isOK = (GetDiskSizeInfo(hSrcDisk, &srcSizeInfo) && GetDiskSizeInfo(hDstDisk, &dstSizeInfo))))
			{
				DWORD buffSize = 1048576 * 4;
				DWORD copySector = max(srcSizeInfo.SectorSize, dstSizeInfo.SectorSize);
				BYTE* copyBuffer = (BYTE*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)buffSize + copySector);
				BYTE* alignedCopyBuffer = (BYTE*)PtrAlignTo(copyBuffer, copySector);
			
				if ((isOK = (alignedCopyBuffer != nullptr)))
				{
					LONGLONG copySize = min(srcSizeInfo.Size.QuadPart, dstSizeInfo.Size.QuadPart);
					if (copySize < buffSize)
					{
						buffSize = copySize;
					}
					WCHAR textProgressTrackerText[20]{};
					LONGLONG writtenTotal = 0;
					DWORD readBytes = 0;
					DWORD writtenBytes = 0;
					SetLastError(0);
					while (ReadFile(hSrcDisk, alignedCopyBuffer, buffSize, &readBytes, nullptr) && readBytes != 0)
					{
						WriteFile(hDstDisk, alignedCopyBuffer, readBytes, &writtenBytes, nullptr);
						if (writtenBytes != readBytes)
						{
							isOK = FALSE;
							break;
						}
						writtenTotal += writtenBytes;
						DWORD percentsCompleted = writtenTotal * 100 / copySize;
						SendMessage(
							p->hProgressTracker, PBM_SETPOS, percentsCompleted, 0
						);
						wnsprintfW(
							textProgressTrackerText, 20,
							L"%u%% completed", percentsCompleted
						);
						SetWindowTextW(p->hTextProgressTracker, textProgressTrackerText);

						if (copySize - writtenTotal < buffSize)
						{
							buffSize = copySize - writtenTotal;
						}
					}
				}
				HeapFree(GetProcessHeap(), 0, copyBuffer);
			}
			PrepareDiskForUse(hDstDisk);
		}
		CloseHandle(hSrcDisk);
		CloseHandle(hDstDisk);
	}
	if (!isOK || GetLastError() != 0)
	{
		ShowErrorMessage(p->hMsgParentWnd, GetLastError());
	}
	else
	{
		MessageBoxW(p->hMsgParentWnd, L"The disk was cloned successfully.", L"Success", MB_ICONASTERISK);
	}

	SendMessage(p->hProgressTracker, PBM_SETPOS, 100, 0);
	SetWindowText(p->hTextProgressTracker, L"100% completed");
	EnableWindow(p->hButtonToBlock, TRUE);
	HeapFree(GetProcessHeap(), 0, p);
	return isOK;
}

int ShowErrorMessage(HWND hParent, DWORD errorCode)
{
	LPWSTR msgStr = nullptr;
	DWORD formatted = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		nullptr, errorCode, 0, (LPWSTR)&msgStr, 0, nullptr
	);
	if (formatted)
	{
		MessageBoxW(hParent, msgStr, L"Error", MB_ICONERROR);
		HeapFree(GetProcessHeap(), 0, msgStr);
	};
	return 0;
}