#ifndef UNICODE
#define UNICODE
#endif 

#include <cstdint>
#include <windows.h>
#include <wincodec.h>
#include <fstream>
#include "resource.h"
#pragma comment(lib, "Windowscodecs.lib")


enum class ImageFormat {
    invalid,
    txt,
    bin,
};

static constexpr DWORD mydwstyle = WS_OVERLAPPEDWINDOW;

static HWND hwnd;
static int64_t width = 0, height = 0;
static int64_t oldwidth = 0, oldheight = 0;
static RGBQUAD* imagedata = NULL;
static HBITMAP imagebitmap = NULL;
static bool menuredraw = false;

static int windowwidth = 300, windowheight = 0;

static bool endsWith(const wchar_t* str, const wchar_t* suffix);
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
    AppendMenuW(filemenu, MF_STRING, 2, L"&Save as...");
    AppendMenuW(filemenu, MF_STRING, 3, L"&Exit...");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(filemenu), L"File");

    // Create the window.

    int screenwidth  = GetSystemMetrics(SM_CXSCREEN);
    int screenheight = GetSystemMetrics(SM_CYSCREEN);
    RECT rect = {screenwidth / 2 - 150, screenheight / 2, screenwidth / 2 + 150, screenheight / 2};
    AdjustWindowRect(&rect, mydwstyle, TRUE);
    windowwidth = rect.right - rect.left;
    windowheight = rect.bottom - rect.top;

    hwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES | WS_EX_APPWINDOW,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"Image Viewer",
        mydwstyle,            // Window style

        // Size and position
        rect.left, rect.top, windowwidth, windowheight,

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

static bool openwicfile(const wchar_t* path) {
    IWICImagingFactory* pFactory;
    {
        MULTI_QI mqi{&IID_IWICImagingFactory, NULL, NULL};
        if (FAILED(CoCreateInstanceEx(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, NULL, 1, &mqi))) {
            MessageBoxExW(NULL, L"Failed to initialize WIC.", L"Error", MB_OK | MB_ICONERROR, NULL);
            return false;
        }
        pFactory = reinterpret_cast<IWICImagingFactory*>(mqi.pItf);
    }
    IWICBitmapDecoder* pDecoder;
    if (FAILED(pFactory->CreateDecoderFromFilename(path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pDecoder))) {
        MessageBoxExW(NULL, L"Image format not supported.", L"Error", MB_OK | MB_ICONERROR, NULL);
        pFactory->Release();
        return false;
    }
    IWICBitmapFrameDecode* pFrameDecode;
    if (FAILED(pDecoder->GetFrame(0, &pFrameDecode))) {
        MessageBoxExW(NULL, L"WIC error.", L"Error", MB_OK | MB_ICONERROR, NULL);
        return false;
    }
    {
        UINT iwidth, iheight;
        if (FAILED(pFrameDecode->GetSize(&iwidth, &iheight))) {
            MessageBoxExW(NULL, L"WIC error.", L"Error", MB_OK | MB_ICONERROR, NULL);
            return false;
        }
        width = iwidth;
        height = iheight;
    }
    IWICFormatConverter* pConverter;
    if (FAILED(pFactory->CreateFormatConverter(&pConverter))) {
        pFrameDecode->Release();
        pDecoder->Release();
        pFactory->Release();
        return false; // Could not create format converter
    }
    if (FAILED(pConverter->Initialize(pFrameDecode, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeMedianCut))) {
        pConverter->Release();
        pFrameDecode->Release();
        pDecoder->Release();
        pFactory->Release();
        return false; // Could not convert image format
    }
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
    if (FAILED(pConverter->CopyPixels(NULL, width * sizeof(RGBQUAD), width * height * sizeof(RGBQUAD), reinterpret_cast<BYTE*>(imagedata)))) {
        MessageBoxExW(NULL, L"Failed to copy pixels.", L"Error", MB_OK | MB_ICONERROR, NULL);
        return false;
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
    pDecoder->Release();
    pFrameDecode->Release();
    pFactory->Release();
    return true;
}

static bool savewicfile(const wchar_t* path) {
    IWICImagingFactory* pFactory;
    {
        MULTI_QI mqi{ &IID_IWICImagingFactory, NULL, NULL };
        if (FAILED(CoCreateInstanceEx(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, NULL, 1, &mqi))) {
            MessageBoxExW(NULL, L"Failed to initialize WIC.", L"Error", MB_OK | MB_ICONERROR, NULL);
            return false;
        }
        pFactory = reinterpret_cast<IWICImagingFactory*>(mqi.pItf);
    }
    IWICStream* pStream;
    if (FAILED(pFactory->CreateStream(&pStream))) {
        MessageBoxExW(NULL, L"WIC error.", L"Error", MB_OK | MB_ICONERROR, NULL);
        return false;
    }
    if (FAILED(pStream->InitializeFromFilename(path, GENERIC_WRITE))) {
        pStream->Release();
        pFactory->Release();
        return false;
    }
    IWICBitmapEncoder* pEncoder;
    {
        GUID guid;
        if (endsWith(path, L".png")) guid = GUID_ContainerFormatPng;
        else if (endsWith(path, L".jpg") || endsWith(path, L".jpeg")) guid = GUID_ContainerFormatJpeg;
        else if (endsWith(path, L".bmp")) guid = GUID_ContainerFormatBmp;
        else if (endsWith(path, L".tif") || endsWith(path, L".tiff")) guid = GUID_ContainerFormatTiff;
        else if (endsWith(path, L".wdp")) guid = GUID_ContainerFormatWmp;
        else {
            MessageBoxExW(NULL, L"Image format not supported.", L"Error", MB_OK | MB_ICONERROR, NULL);
            pStream->Release();
            pFactory->Release();
            return false;
        }
        if (FAILED(pFactory->CreateEncoder(guid, nullptr, &pEncoder))) {
            MessageBoxExW(NULL, L"Image format not supported.", L"Error", MB_OK | MB_ICONERROR, NULL);
            pStream->Release();
            pFactory->Release();
            return false;
        }
        if (FAILED(pEncoder->Initialize(pStream, WICBitmapEncoderNoCache))) {
            MessageBoxExW(NULL, L"WIC error.", L"Error", MB_OK | MB_ICONERROR, NULL);
            pStream->Release();
            pFactory->Release();
            return false;
        }
    }
    IWICBitmapFrameEncode* pFrameEncode;
    if (FAILED(pEncoder->CreateNewFrame(&pFrameEncode, nullptr))) {
        MessageBoxExW(NULL, L"WIC error.", L"Error", MB_OK | MB_ICONERROR, NULL);
        pEncoder->Release();
        pStream->Release();
        pFactory->Release();
        return false;
    }
    if (FAILED(pFrameEncode->Initialize(nullptr))) {
        MessageBoxExW(NULL, L"WIC error.", L"Error", MB_OK | MB_ICONERROR, NULL);
        pFrameEncode->Release();
        pEncoder->Release();
        pStream->Release();
        pFactory->Release();
        return false;
    }
    BYTE* data = new BYTE[width * height * 3];
    {
        size_t loopCount = width * height;
        BYTE* current = data;
        for (size_t i = 0; i < loopCount; i++) {
            RGBQUAD rgb = imagedata[i];
            *(current++) = rgb.rgbBlue;
            *(current++) = rgb.rgbGreen;
            *(current++) = rgb.rgbRed;
        }
    }
    pFrameEncode->SetSize(width, height);
    {
        GUID guid = GUID_WICPixelFormat24bppRGB;
        pFrameEncode->SetPixelFormat(&guid);
    }
    if (FAILED(pFrameEncode->WritePixels(height, width * 3, width * height * 3, data))) {
        MessageBoxExW(NULL, L"WIC error.", L"Error", MB_OK | MB_ICONERROR, NULL);
        pFrameEncode->Release();
        pEncoder->Release();
        pStream->Release();
        pFactory->Release();
        delete[] data;
        return false;
    }
    delete[] data;
    if (FAILED(pFrameEncode->Commit())) {
        MessageBoxExW(NULL, L"WIC error.", L"Error", MB_OK | MB_ICONERROR, NULL);
        pFrameEncode->Release();
        pEncoder->Release();
        pStream->Release();
        pFactory->Release();
        return false;
    }
    if (FAILED(pEncoder->Commit())) {
        MessageBoxExW(NULL, L"WIC errror.", L"Error", MB_OK | MB_ICONERROR, NULL);
        pFrameEncode->Release();
        pEncoder->Release();
        pStream->Release();
        pFactory->Release();
        return false;
    }
    pFrameEncode->Release();
    pEncoder->Release();
    pStream->Release();
    pFactory->Release();
    MessageBoxExW(NULL, L"File saved successfully", L"Success", MB_OK | MB_ICONINFORMATION, NULL);
    return true;
}

static bool endsWith(const wchar_t* str, const wchar_t* suffix)
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
            MessageBoxExW(NULL, L".txt files can only contain '0' and '1' characters. ", L"Error", MB_OK | MB_ICONERROR, NULL);
            return (char)0b11111111;
        }
        byte |= (ch == '1' ? 1 : 0) << (7 - i);
    }
    return byte;
}
static void writetxtbyte(char*& file, char byte) {
    for (int i = 0; i < 8; ++i)
        *(file++) = (byte & (1 << (7 - i))) ? '1' : '0';
}
static INT_PTR CALLBACK QueryDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            oldwidth = width;
            oldheight = height;
            wchar_t buffer1[256], buffer2[256], buffer3[256];
            GetDlgItemTextW(hwndDlg, IDC_EDIT_INT1, buffer1, 256);
            GetDlgItemTextW(hwndDlg, IDC_EDIT_INT2, buffer2, 256);
            GetDlgItemTextW(hwndDlg, IDC_EDIT_INT3, buffer3, 256);
            if (swscanf_s(buffer1, L"%lld", &width) == 1 && swscanf_s(buffer2, L"%lld", &height) == 1) {
                EndDialog(hwndDlg, IDOK);
                return TRUE;
            }
            MessageBoxW(hwndDlg, L"Invalid input", L"Error", MB_OK | MB_ICONERROR);
            width = oldwidth;
            height = oldheight;
        }
        break;
    }
    return FALSE;
}
static void CALLBACK readfileexCallback(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
    CloseHandle(lpOverlapped->hEvent);
}
static void CALLBACK writefileexCallback(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
    CloseHandle(lpOverlapped->hEvent);
}
static void readtxtfile(HANDLE file, size_t fileSize) {
    // The entire file is read into data before it can be parsed
    char* data = new char[fileSize * 32];
    for (size_t i = 0; i < fileSize * 32; i++) data[i] = '0';
    {
        OVERLAPPED overlapped;
        ZeroMemory(&overlapped, sizeof(OVERLAPPED));
        overlapped.hEvent = CreateEventExW(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE);
        (void)ReadFileEx(file, data, fileSize * 32, &overlapped, readfileexCallback);
        (void)WaitForSingleObjectEx(overlapped.hEvent, 1000, TRUE);
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
            imagedata[i] = imagedata[i % loopCount];
        }
        delete[] data;
    }
}
static void readbinfile(HANDLE file, size_t fileSize) {
    // The entire file is read into data before it can be parsed
    char* data = new char[fileSize * 4];
    for (size_t i = 0; i < fileSize * 4; i++) data[i] = '0';
    {
        OVERLAPPED overlapped;
        ZeroMemory(&overlapped, sizeof(OVERLAPPED));
        overlapped.hEvent = CreateEventExW(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE);
        (void)ReadFileEx(file, data, fileSize * 4, &overlapped, readfileexCallback);
        (void)WaitForSingleObjectEx(overlapped.hEvent, 1000, TRUE);
        CloseHandle(file);
    }
    // Reads only the data found in the file, the rest is filled with black. data is deleted as it is no longer needed.
    {
        size_t imageSize = width * height;
        size_t loopCount = min(fileSize, imageSize);
        char* current = data;
        for (size_t i = 0; i < loopCount; i++) {
            RGBQUAD rgb{};
            rgb.rgbRed = *(current++);
            rgb.rgbGreen = *(current++);
            rgb.rgbBlue = *(current++);
            rgb.rgbReserved = NULL;
            current++;
            imagedata[i] = rgb;
        }
        for (size_t i = loopCount; i < imageSize; i++) {
            imagedata[i] = imagedata[i % loopCount];
        }
        delete[] data;
    }
}
static void openFile(const wchar_t* path)
{
    // .txt files store bytes as sequences of '0' and '1', this unnecessarily increases the file size by a factor of 8
    ImageFormat fmt = endsWith(path, L".bin") ? ImageFormat::bin : endsWith(path, L".txt") ? ImageFormat::txt : ImageFormat::invalid;
    if (fmt == ImageFormat::invalid) {
        openwicfile(path);
        return;
    }
    // Open the file for reading asynchronously
    HANDLE file = CreateFileW(path, FILE_GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
    // Get the file size
    size_t fileSize;
    {
        LARGE_INTEGER fileSizeLargeInteger;
        GetFileSizeEx(file, &fileSizeLargeInteger);
        fileSize = fileSizeLargeInteger.QuadPart;
    }
    // fileSize is now in pixels.
    switch (fmt)
    {
    case ImageFormat::txt:
        // Each pixel is 4 'bytes' and each 'byte' is 8 bytes. 
        if (fileSize % 32 != 0) {
            MessageBoxExW(NULL, L".txt files need to be a multiple of 32 bytes", L"Error", MB_OK | MB_ICONERROR, NULL);
            CloseHandle(file);
            return;
        }
        fileSize /= 32;
        break;
    case ImageFormat::bin:
        if (fileSize % 4 != 0) {
            MessageBoxExW(NULL, L".bin files need to be a multiple of 4 bytes", L"Error", MB_OK | MB_ICONERROR, NULL);
            CloseHandle(file);
            return;
        }
        fileSize /= 4;
        break;
    default:
        break;
    }
    // Querries for image dimensions
    {
        int option;
    retrypoint:
        DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_QUERY_DIALOG), hwnd, QueryDialogProc);
        if (fileSize != width * height) {
            option = MessageBoxExW(NULL, L"The size you entered doesn't match the file size, continue anyway?", L"Error", MB_ABORTRETRYIGNORE | MB_ICONERROR, NULL);
            if (option == IDRETRY) {
                width = oldwidth;
                height = oldheight;
                goto retrypoint;
            }
            if (option == IDABORT) {
                width = oldwidth;
                height = oldheight;
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
    // Read the file
    switch (fmt)
    {
    case ImageFormat::txt:
        readtxtfile(file, fileSize);
        break;
    case ImageFormat::bin:
        readbinfile(file, fileSize);
        break;
    default:
        break;
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
static void writetxtfile(HANDLE file) {
    // Get the file size in pixels
    size_t fileSize = width * height;
    // The image is parsed and copied to a new buffer
    char* data = new char[fileSize * 32];
    {
        char* current = data;
        for (size_t i = 0; i < fileSize; ++i) {
            RGBQUAD rgb = imagedata[i];
            writetxtbyte(current, rgb.rgbRed);
            writetxtbyte(current, rgb.rgbGreen);
            writetxtbyte(current, rgb.rgbBlue);
            writetxtbyte(current, 0x00);
        }
    }
    // Saving the image
    {
        OVERLAPPED overlapped;
        ZeroMemory(&overlapped, sizeof(OVERLAPPED));
        overlapped.hEvent = CreateEventExW(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE);
        (void)WriteFileEx(file, data, fileSize * 32, &overlapped, writefileexCallback);
        (void)WaitForSingleObjectEx(overlapped.hEvent, 1000, TRUE);
        delete[] data;
    }
}
static void writebinfile(HANDLE file) {
    // Get the file size in pixels
    size_t fileSize = width * height;
    // The image is parsed and copied to a new buffer
    char* data = new char[fileSize * 32];
    {
        char* current = data;
        for (size_t i = 0; i < fileSize; ++i) {
            RGBQUAD rgb = imagedata[i];
            *(current++) = rgb.rgbRed;
            *(current++) = rgb.rgbGreen;
            *(current++) = rgb.rgbBlue;
            *(current++) = 0x00;
        }
    }
    // Saving the image
    {
        OVERLAPPED overlapped;
        ZeroMemory(&overlapped, sizeof(OVERLAPPED));
        overlapped.hEvent = CreateEventExW(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE);
        (void)WriteFileEx(file, data, fileSize * 32, &overlapped, writefileexCallback);
        (void)WaitForSingleObjectEx(overlapped.hEvent, 1000, TRUE);
        delete[] data;
    }
}
static void saveFile(const wchar_t* path)
{
    // Image must first be opened before it is saved
    if (imagedata == nullptr) {
        MessageBoxExW(NULL, L"Open a file first before saving it", L"Unable to save", MB_OK | MB_ICONWARNING, NULL);
        return;
    }
    ImageFormat fmt = endsWith(path, L".bin") ? ImageFormat::bin : endsWith(path, L".txt") ? ImageFormat::txt : ImageFormat::invalid;
    if (fmt == ImageFormat::invalid) {
        savewicfile(path);
        return;
    }
    // Create or open the file for writing asynchronously
    HANDLE file = CreateFileW(path, FILE_GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
    switch (fmt)
    {
    case ImageFormat::txt:
        writetxtfile(file);
        break;
    case ImageFormat::bin:
        writebinfile(file);
        break;
    default:
        break;
    }
    CloseHandle(file);
    MessageBoxExW(NULL, L"File saved successfully", L"Success", MB_OK | MB_ICONINFORMATION, NULL);
}
static wchar_t* saveFileDialog() {
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
    if (GetSaveFileNameW(&fileDialog) == 0) {
        delete[] out;
        return NULL;
    }
    return out;
}
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SIZE:
        windowwidth = LOWORD(lParam);
        windowheight = HIWORD(lParam);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 1:
        {
            wchar_t* path = openFileDialog();
            if (path != NULL) {
                openFile(path);
                delete[] path;
            }
        }
            break;
        case 2: {
            wchar_t* path = saveFileDialog();
            if (path != NULL) {
                saveFile(path);
                delete[] path;
            }
        }
            break;
        case 3:
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
        if (width == 0 || height == 0) {
            EndPaint(hwnd, &ps);
            if (menuredraw) DrawMenuBar(hwnd);
            return 0;
        }
        HDC image = CreateCompatibleDC(hdc);
        HGDIOBJ old = SelectObject(image, imagebitmap);
        if (windowwidth * height > windowheight * width ) {
            int displaywidth = width * windowheight / height;
            RECT rect = { 0, 0, windowwidth, windowheight };
            FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            StretchBlt(hdc, (windowwidth - displaywidth) / 2, 0, displaywidth, windowheight, image, 0, 0, width, height, SRCCOPY);
        }
        else {
            int displayheight = height * windowwidth / width;
            RECT rect = { 0, 0, windowwidth, windowheight };
            FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            StretchBlt(hdc, 0, (windowheight - displayheight) / 2, windowwidth, displayheight, image, 0, 0, width, height, SRCCOPY);
        }
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