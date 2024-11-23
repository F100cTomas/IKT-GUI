#ifndef UNICODE
#define UNICODE
#endif 

#include <cstdint>
#include <windows.h>
#include <fstream>

static HWND hwnd;
static uint64_t width = 0, height = 0;
static RGBQUAD* imagedata = NULL;

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

    hwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES | WS_EX_APPWINDOW,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"Image Viewer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 100,

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
static char readtxtbyte(std::ifstream& file) {
    char byte = 0;
    for (int i = 0; i < 8; ++i) {
        char ch = file.get();
        if (ch != '0' && ch != '1') {
            MessageBoxExW(NULL, L".txt files can only contain '0' and '1' characters.", L"Error", MB_OK | MB_ICONERROR, NULL);
            return 0b11111111;
        }
        byte |= (ch == '1' ? 1 : 0) << (7 - i);
    }
    return byte;
}
static void openFile(const wchar_t* path)
{
    DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_QUERY_DIALOG), hwndParent, QueryDialogProc);
    if (!endsWith(path, L".txt")) {
        MessageBoxExW(NULL, L"Files other than .txt files aren't supported yet.", L"Error", MB_OK | MB_ICONERROR, NULL);
        return;
    }
    std::ifstream file{ path };
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg() / 8;
    if (fileSize % 32 != 0) {
        MessageBoxExW(NULL, L".txt files need to have a multiple of 32 characters.", L"Error", MB_OK | MB_ICONERROR, NULL);
        return;
    }
    char* data = new char[fileSize];
    file.seekg(0, std::ios::beg);
    char* pd = data;
    for (size_t i = 0; i < fileSize; i += 4) {
        RGBQUAD rgb;
        rgb.rgbRed   = readtxtbyte(file);
        rgb.rgbGreen = readtxtbyte(file);
        rgb.rgbBlue  = readtxtbyte(file);
        (void)readtxtbyte(file);
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
            wchar_t* path = openFileDialog(hwnd);
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

        // All painting occurs here, between BeginPaint and EndPaint.

        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

        EndPaint(hwnd, &ps);
    }
    return 0;
    default:
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}