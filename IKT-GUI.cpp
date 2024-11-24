#ifndef UNICODE
#define UNICODE
#endif 

#include <cstdint>
#include <windows.h>
#include <fstream>
#include "resource.h"

static constexpr DWORD mydwstyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

static HWND hwnd;
static int64_t width = 0, height = 0;
static RGBQUAD* imagedata = NULL;
static HBITMAP imagebitmap = NULL;
static bool menuredraw = false;

static void openFile(const wchar_t* path);
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    (void)CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // Register the window class.
    const wchar_t CLASS_NAME[] = L"F100cTomas Image Viewer";

    WNDCLASS wc = { };

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // Create menu
    HMENU menu = CreateMenu();
    HMENU filemenu = CreatePopupMenu();
    AppendMenuW(filemenu, MF_STRING, 1, L"&Open...");
    AppendMenuW(filemenu, MF_STRING, 2, L"&Exit...");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(filemenu), L"File");

    // Create the window.

    int screenwidth  = GetSystemMetrics(SM_CXSCREEN);
    int screenheight = GetSystemMetrics(SM_CYSCREEN);
    RECT rect = {screenwidth / 2 - 150, screenheight / 2, screenwidth / 2 + 150, screenheight / 2};
    AdjustWindowRect(&rect, mydwstyle, TRUE);

    hwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES | WS_EX_APPWINDOW,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"Image Viewer",
        mydwstyle,            // Window style

        // Size and position
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,

        NULL,       // Parent window    
        menu,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    // Run the message loop.

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

bool endsWith(const wchar_t* str, const wchar_t* suffix)
{
    if (!str || !suffix)
        return false;
    size_t lenstr = wcslen(str);
    size_t lensuffix = wcslen(suffix);
    if (lensuffix > lenstr)
        return false;
    return wcsncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}
static char readtxtbyte(char*& file) {
    char byte = 0;
    for (int i = 0; i < 8; ++i) {
        char ch = *(file++);
        if (ch != '0' && ch != '1') {
            MessageBoxExW(NULL, L".txt files can only contain '0' and '1' characters.: ", L"Error", MB_OK | MB_ICONERROR, NULL);
            return (char)0b11111111;
        }
        byte |= (ch == '1' ? 1 : 0) << (7 - i);
    }
    return byte;
}
static INT_PTR CALLBACK QueryDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            wchar_t buffer1[256], buffer2[256];
            GetDlgItemText(hwndDlg, IDC_EDIT_INT1, buffer1, 256);
            GetDlgItemText(hwndDlg, IDC_EDIT_INT2, buffer2, 256);
            if (!(swscanf_s(buffer1, L"%lld", &width) == 1 && swscanf_s(buffer2, L"%lld", &height) == 1)) {
                MessageBoxW(hwndDlg, L"Invalid input", L"Error", MB_OK | MB_ICONERROR);
            }
            EndDialog(hwndDlg, IDOK);
            return TRUE;
        }
        break;
    }
    return FALSE;
}
static void CALLBACK readfileexCallback(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
    CloseHandle(lpOverlapped->hEvent);
}
static void openFile(const wchar_t* path)
{
    // Open the file for reading asynchronously
    HANDLE file = CreateFileW(path, FILE_GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
    // Get the file size
    size_t fileSize;
    {
        LARGE_INTEGER fileSizeLargeInteger;
        GetFileSizeEx(file, &fileSizeLargeInteger);
        fileSize = fileSizeLargeInteger.QuadPart;
    }
    // Files are .txt and store bytes as sequences of '0' and '1', this unnecessarily increases the file size by a factor of 8
    if (!endsWith(path, L".txt")) {
        MessageBoxExW(NULL, L"Files other than .txt files aren't supported yet.", L"Error", MB_OK | MB_ICONERROR, NULL);
        return;
    }
    // Each pixel is 4 'bytes' and each 'byte' is 8 bytes. fileSize is now in pixels.
    if (fileSize % 32 != 0) {
        MessageBoxExW(NULL, L".txt files need to have a multiple of 32 characters.", L"Error", MB_OK | MB_ICONERROR, NULL);
        CloseHandle(file);
        return;
    }
    fileSize /= 32;
    // Querries for image size
    {
        int option;
    retrypoint:
        DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_QUERY_DIALOG), hwnd, QueryDialogProc);
        if (fileSize != width * height) {
            option = MessageBoxExW(NULL, L"The size you entered doesn't match the file size, continue anyway?", L"Error", MB_ABORTRETRYIGNORE | MB_ICONERROR, NULL);
            if (option == IDRETRY) goto retrypoint;
            if (option == IDABORT) {
                CloseHandle(file);
                return;
            }
        }
    }
    // Allocates space for raw color data
    {
        BITMAPINFO bitmapinfo;
        ZeroMemory(&bitmapinfo, sizeof(BITMAPINFO));
        bitmapinfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapinfo.bmiHeader.biWidth = width;
        bitmapinfo.bmiHeader.biHeight = -height;
        bitmapinfo.bmiHeader.biPlanes = 1;
        bitmapinfo.bmiHeader.biBitCount = 32;
        bitmapinfo.bmiHeader.biCompression = BI_RGB;
        imagebitmap = CreateDIBSection(NULL, &bitmapinfo, DIB_RGB_COLORS, reinterpret_cast<void**>(&imagedata), NULL, NULL);
    }
    // The entire file is read into data before it can be parsed
    char* data = new char[fileSize * 32];
    for (size_t i = 0; i < fileSize * 32; i++) data[i] = '0';
    {
        OVERLAPPED overlapped;
        ZeroMemory(&overlapped, sizeof(OVERLAPPED));
        overlapped.hEvent = CreateEventExW(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE);
        (void)ReadFileEx(file, data, fileSize * 32, &overlapped, readfileexCallback);
        (void)WaitForSingleObjectEx(overlapped.hEvent, 200, TRUE);
        CloseHandle(file);
    }
    // Reads only the data found in the file, the rest is filled with black. data is deleted as it is no longer needed.
    {
        size_t imageSize = width * height;
        size_t loopCount = min(fileSize, imageSize);
        char* current = data;
        for (size_t i = 0; i < loopCount; i++) {
            RGBQUAD rgb{};
            rgb.rgbRed = readtxtbyte(current);
            rgb.rgbGreen = readtxtbyte(current);
            rgb.rgbBlue = readtxtbyte(current);
            rgb.rgbReserved = NULL;
            (void)readtxtbyte(current);
            imagedata[i] = rgb;
        }
        for (size_t i = loopCount; i < imageSize; i++) {
            imagedata[i] = { 0, 0, 0, NULL };
        }
        delete[] data;
    }
    // Adjusts the window to match the size of the image and redraws it.
    {
        RECT rect;
        GetWindowRect(hwnd, &rect);
        rect.right = rect.left + width;
        rect.bottom = rect.top + height;
        AdjustWindowRect(&rect, mydwstyle, TRUE);
        MoveWindow(hwnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);
        InvalidateRect(hwnd, NULL, TRUE);
        menuredraw = true;
    }
}
static wchar_t* openFileDialog() {
    wchar_t* out = new wchar_t[MAX_PATH];
    ZeroMemory(out, MAX_PATH);
    OPENFILENAMEW fileDialog;
    ZeroMemory(&fileDialog, sizeof(OPENFILENAMEW));
    fileDialog.lStructSize = sizeof(OPENFILENAMEW);
    fileDialog.hwndOwner = hwnd;
    fileDialog.lpstrFilter = L"All Files\0*.*\0Text Files\0*.TXT\0";
    // fileDialog.lpstrCustomFilter = NULL;
    // fileDialog.nMaxCustFilter = NULL;
    // fileDialog.nFilterIndex = NULL;
    fileDialog.lpstrFile = out;
    fileDialog.nMaxFile = MAX_PATH;
    // fileDialog.lpstrFileTitle = NULL;
    // fileDialog.nMaxFileTitle = NULL;
    // fileDialog.lpstrInitialDir = NULL;
    fileDialog.lpstrTitle = L"Select a File";
    fileDialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    // fileDialog.nFileOffset = NULL;
    // fileDialog.nFileExtension = NULL;
    // fileDialog.lpstrDefExt = NULL;
    // fileDialog.lCustData = NULL;
    // fileDialog.lpfnHook = NULL;
    // fileDialog.lpTemplateName = NULL;
    // fileDialog.pvReserved = NULL;
    // fileDialog.dwReserved - NULL;
    // fileDialog.FlagsEx = NULL;
    if (GetOpenFileNameW(&fileDialog) == 0) {
        delete[] out;
        return NULL;
    }
    return out;
}
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 1:
        {
            wchar_t* path = openFileDialog();
            if (path != NULL) {
                openFile(path);
                delete path;
            }
        }
            break;
        case 2:
            PostQuitMessage(0);
            break;
        default:
            break;
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        // SetGraphicsMode(hdc, GM_ADVANCED);
        HDC image = CreateCompatibleDC(hdc);
        HGDIOBJ old = SelectObject(image, imagebitmap);
        BitBlt(hdc, 0, 0, width, height, image, 0, 0, SRCCOPY);
        SelectObject(image, old);
        DeleteDC(image);
        EndPaint(hwnd, &ps);
        if (menuredraw) DrawMenuBar(hwnd);
    }
    return 0;
    default:
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}