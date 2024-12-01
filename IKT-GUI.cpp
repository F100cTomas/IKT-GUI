#ifndef UNICODE
#define UNICODE
#endif 

#include <cstdint>
#include <windows.h>
#include <wincodec.h>
#include <cstdio>
#include "resource.h"
#pragma comment(lib, "Windowscodecs.lib")


enum class ImageFormat {
    invalid,
    txt,
    bin,
};

enum class ColorFormat {
    RGBA, RGB, ARGB, BGRA, BGR, ABGR, BAGR,
    GrayScale,
    CMY, CMYK, HSL, HSLA, HSV, HSVA,
    Python, Invalid,
};

typedef RGBQUAD(*ColorFormatDecoder)(const char*& data);
typedef void(*ColorFormatEncoder)(char*& data, RGBQUAD color);

static constexpr DWORD mydwstyle = WS_OVERLAPPEDWINDOW;

static HWND hwnd;
static int64_t width = 0, height = 0;
static int64_t oldwidth = 0, oldheight = 0;
static RGBQUAD* imagedata = NULL;
static HBITMAP imagebitmap = NULL;
static bool menuredraw = false;
static ColorFormat colorformat = ColorFormat::Invalid;
static int windowwidth = 300, windowheight = 0;

static bool endsWith(const wchar_t* str, const wchar_t* suffix);
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK ColorQueryDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    (void)CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    
    // Register the window class.
    const wchar_t CLASS_NAME[] = L"F100cTomas Image Viewer";

    size_t ssize = sizeof(long double);

    WNDCLASS wc = { };

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCE(IDI_ICON1));

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
    // MessageBoxExW(NULL, L"File saved successfully", L"Success", MB_OK | MB_ICONINFORMATION, NULL);
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
static bool decidecolorformat(const wchar_t* buffer) {
    if (_wcsicmp(buffer, L"RGBA") == 0 || *buffer == L'\0') colorformat = ColorFormat::RGBA;
    else if (_wcsicmp(buffer, L"RGB") == 0) colorformat = ColorFormat::RGB;
    else if (_wcsicmp(buffer, L"ARGB") == 0) colorformat = ColorFormat::ARGB;
    else if (_wcsicmp(buffer, L"BGRA") == 0) colorformat = ColorFormat::BGRA;
    else if (_wcsicmp(buffer, L"BGR") == 0) colorformat = ColorFormat::BGR;
    else if (_wcsicmp(buffer, L"ABGR") == 0) colorformat = ColorFormat::ABGR;
    else if (_wcsicmp(buffer, L"BAGR") == 0) colorformat = ColorFormat::BAGR;
    else if (_wcsicmp(buffer, L"GS") == 0 || _wcsicmp(buffer, L"GrayScale") == 0 || _wcsicmp(buffer, L"GreyScale") == 0 || _wcsicmp(buffer, L"Gray") == 0 || _wcsicmp(buffer, L"Grey") == 0) colorformat = ColorFormat::GrayScale;
    else if (_wcsicmp(buffer, L"CMY") == 0) colorformat = ColorFormat::CMY;
    else if (_wcsicmp(buffer, L"CMYK") == 0) colorformat = ColorFormat::CMYK;
    else if (_wcsicmp(buffer, L"HSL") == 0) colorformat = ColorFormat::HSL;
    else if (_wcsicmp(buffer, L"HSLA") == 0) colorformat = ColorFormat::HSLA;
    else if (_wcsicmp(buffer, L"HSV") == 0) colorformat = ColorFormat::HSV;
    else if (_wcsicmp(buffer, L"HSVA") == 0) colorformat = ColorFormat::HSVA;
    else if (_wcsicmp(buffer, L"=(") == 0) colorformat = ColorFormat::Python;
    else return false;
    return true;
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
            if (swscanf_s(buffer1, L"%lld", &width) == 1 && swscanf_s(buffer2, L"%lld", &height) == 1 && decidecolorformat(buffer3)) {
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
static INT_PTR CALLBACK ColorQueryDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            wchar_t buffer[256];
            GetDlgItemTextW(hwndDlg, IDC_EDIT_CM, buffer, 256);
            if (decidecolorformat(buffer)) {
                EndDialog(hwndDlg, IDOK);
                return TRUE;
            }
        }
        break;
    }
    return FALSE;
}
static size_t get_pixelSize(ColorFormat cf) {
    switch (cf)
    {
    case ColorFormat::Invalid:
    default:
    case ColorFormat::RGBA:
    case ColorFormat::ARGB:
    case ColorFormat::BGRA:
    case ColorFormat::ABGR:
    case ColorFormat::BAGR:
    case ColorFormat::CMYK:
    case ColorFormat::HSLA:
    case ColorFormat::HSVA:
        return 4;
    case ColorFormat::RGB:
    case ColorFormat::BGR:
    case ColorFormat::CMY:
    case ColorFormat::HSL:
    case ColorFormat::HSV:
        return 3;
    case ColorFormat::GrayScale:
        return 1;
    case ColorFormat::Python:
        return 4 * sizeof(long double);
    }
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
static uint8_t hueToRgb(uint8_t p, uint8_t q, uint8_t t) {
    if (t < 42) return p + (t * (q - p)) / 42;
    if (t < 128) return q;
    if (t < 170) return p + ((170 - t) * (q - p)) / 42;
    return p;
}
static uint8_t pythonHueToRgb(long double p, long double q, long double t) {
    if (t < 0.0L) t += 360.0L;
    if (t > 360.0L) t -= 360.0L;
    if (t < 60.0L) return (p + ((q - p) * t) / 60.0L) * 255;
    if (t < 180.0L) return q * 255;
    if (t < 240.0L) return (p + ((q - p) * (240.0L - t)) / 60.0L) * 255;
    return p * 255;
}
static RGBQUAD RGBAdecoder(const char*& data) {
    RGBQUAD rgb{};
    rgb.rgbRed  = *(data++);
    rgb.rgbGreen = *(data++);
    rgb.rgbBlue = *(data++);
    data++;
    return rgb;
}
static RGBQUAD RGBdecoder(const char*& data) {
    RGBQUAD rgb{};
    rgb.rgbRed = *(data++);
    rgb.rgbGreen = *(data++);
    rgb.rgbBlue = *(data++);
    return rgb;
}
static RGBQUAD ARGBdecoder(const char*& data) {
    RGBQUAD rgb{};
    data++;
    rgb.rgbRed = *(data++);
    rgb.rgbGreen = *(data++);
    rgb.rgbBlue = *(data++);
    return rgb;
}
static RGBQUAD BGRAdecoder(const char*& data) {
    RGBQUAD rgb{};
    rgb.rgbBlue = *(data++);
    rgb.rgbGreen = *(data++);
    rgb.rgbRed = *(data++);
    data++;
    return rgb;
}
static RGBQUAD BGRdecoder(const char*& data) {
    RGBQUAD rgb{};
    rgb.rgbBlue = *(data++);
    rgb.rgbGreen = *(data++);
    rgb.rgbRed = *(data++);
    return rgb;
}
static RGBQUAD ABGRdecoder(const char*& data) {
    RGBQUAD rgb{};
    data++;
    rgb.rgbBlue = *(data++);
    rgb.rgbGreen = *(data++);
    rgb.rgbRed = *(data++);
    return rgb;
}
static RGBQUAD BAGRdecoder(const char*& data) {
    RGBQUAD rgb{};
    rgb.rgbBlue = *(data++);
    data++;
    rgb.rgbGreen = *(data++);
    rgb.rgbRed = *(data++);
    return rgb;
}
static RGBQUAD GSdecoder(const char*& data) {
    RGBQUAD rgb{};
    rgb.rgbRed = *data;
    rgb.rgbGreen = *data;
    rgb.rgbBlue = *data;
    data++;
    return rgb;
}
static RGBQUAD CMYdecoder(const char*& data) {
    RGBQUAD rgb{};
    rgb.rgbRed = 0xFF - *(data++);
    rgb.rgbGreen = 0xFF - *(data++);
    rgb.rgbBlue = 0xFF - *(data++);
    return rgb;
}
static RGBQUAD CMYKdecoder(const char*& data) {
    RGBQUAD rgb = CMYdecoder(data);
    unsigned char black = *(data++);
    rgb.rgbRed -= black;
    rgb.rgbGreen -= black;
    rgb.rgbBlue -= black;
    return rgb;
}
static RGBQUAD HSLdecoder(const char*& data) {
    const uint8_t h = *(data++), s = *(data++), l = *(data++);
    if (s == 0) {
        RGBQUAD rgb{};
        rgb.rgbRed = l;
        rgb.rgbGreen = l;
        rgb.rgbBlue = l;
        return rgb;
    }
    const uint8_t q = l < 128 ? l + (l*s) / 255 : l + s - (l*s) / 255;
    const uint8_t p = 2 * l - q;
    RGBQUAD rgb{};
    rgb.rgbRed = hueToRgb(p, q, h + 87);
    rgb.rgbGreen = hueToRgb(p, q, h);
    rgb.rgbBlue = hueToRgb(p, q, h - 87);
    return rgb;
}
static RGBQUAD HSLAdecoder(const char*& data) {
    RGBQUAD rgb = HSLdecoder(data);
    data++;
    return rgb;
}
static RGBQUAD HSVdecoder(const char*& data) {
    const uint8_t h = *(data++), s = *(data++), v = *(data++);
    if (s == 0) {
        RGBQUAD rgb;
        rgb.rgbRed = v;
        rgb.rgbGreen = v;
        rgb.rgbBlue = v;
        return rgb;
    }
    const uint8_t i = h / 42;
    const uint8_t ff = (h % 42) * 6;
    const uint8_t p = (v * (255 - s)) / 255;
    const uint8_t q = (v * ((255 * 255) - (s * ff))) / (255 * 255);
    const uint8_t t = (v * ((255 * 255) - (s * (255 - ff)))) / (255 * 255);
    RGBQUAD rgb;
    switch (i) {
    case 0:
        rgb.rgbRed = v;
        rgb.rgbGreen = t;
        rgb.rgbBlue = p;
        break;
    case 1:
        rgb.rgbRed = q;
        rgb.rgbGreen = v;
        rgb.rgbBlue = p;
        break;
    case 2:
        rgb.rgbRed = p;
        rgb.rgbGreen = v;
        rgb.rgbBlue = t;
        break;

    case 3:
        rgb.rgbRed = p;
        rgb.rgbGreen = q;
        rgb.rgbBlue = v;
        break;
    case 4:
        rgb.rgbRed = t;
        rgb.rgbGreen = p;
        rgb.rgbBlue = v;
        break;
    case 5:
    default:
        rgb.rgbRed = v;
        rgb.rgbGreen = p;
        rgb.rgbBlue = q;
        break;
    }
    return rgb;
}
static RGBQUAD HSVAdecoder(const char*& data) {
    RGBQUAD rgb = HSVdecoder(data);
    data++;
    return rgb;
}
static RGBQUAD Pythondecoder(const char*& data) {
    const long double*& fdata = reinterpret_cast<const long double*&>(data);
    const long double h = *(fdata++), s = *(fdata++), l = *(fdata++);
    fdata++;
    if (s == 0) {
        RGBQUAD rgb{};
        rgb.rgbRed = l * 255;
        rgb.rgbGreen = l * 255;
        rgb.rgbBlue = l * 255;
        return rgb;
    }
    const long double q = l < 0.5L ? l * (1 + s) : l + s - (l * s);
    const long double p = 2 * l - q;
    RGBQUAD rgb{};
    rgb.rgbRed = pythonHueToRgb(p, q, h + 120);
    rgb.rgbGreen = pythonHueToRgb(p, q, h);
    rgb.rgbBlue = pythonHueToRgb(p, q, h - 120);
    return rgb;
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
    HANDLE file = CreateFileW(path, FILE_GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
    // Get the file size
    size_t fileSize;
    {
        LARGE_INTEGER fileSizeLargeInteger;
        GetFileSizeEx(file, &fileSizeLargeInteger);
        fileSize = fileSizeLargeInteger.QuadPart;
    }
    // fileSize is now in bytes.
    if (fmt == ImageFormat::txt) {
        if (fileSize % 8 != 0) {
            MessageBoxExW(NULL, L".txt files need to be a multiple of 8 bytes", L"Error", MB_OK | MB_ICONERROR, NULL);
            CloseHandle(file);
            return;
        }
        fileSize /= 8;
    }
    // Querries for image dimensions
    size_t pixelSize;
    {
        int option;
    retrypoint:
        DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_QUERY_DIALOG), hwnd, QueryDialogProc);
        pixelSize = get_pixelSize(colorformat);
        if (fileSize != width * height * pixelSize) {
            option = MessageBoxExW(NULL, L"The size or color model you entered doesn't match the file size, continue anyway?", L"Error", MB_ABORTRETRYIGNORE | MB_ICONERROR, NULL);
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
        if (imagebitmap != NULL) DeleteObject(imagebitmap);
        imagebitmap = CreateDIBSection(NULL, &bitmapinfo, DIB_RGB_COLORS, reinterpret_cast<void**>(&imagedata), NULL, NULL);
    }
    // The entire file is read into data before it can be parsed
    char* data = new char[fileSize * (fmt == ImageFormat::txt ? 8 : 1)];
    {
        OVERLAPPED overlapped;
        ZeroMemory(&overlapped, sizeof(OVERLAPPED));
        overlapped.hEvent = CreateEventExW(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE);
        (void)ReadFileEx(file, data, fileSize * (fmt == ImageFormat::txt ? 8 : 1), &overlapped, readfileexCallback);
        (void)WaitForSingleObjectEx(overlapped.hEvent, 1000, TRUE);
        CloseHandle(file);
    }
    // Converts to binary data
    if (fmt == ImageFormat::txt) {
        char* current = data;
        for (size_t i = 0; i < fileSize; i++)
            data[i] = readtxtbyte(current);
    }
    // fileSize is now in pixels
    fileSize /= pixelSize;
    // Reads only the data found in the file, overflow repeats the image. data is deleted as it is no longer needed.
    {
        ColorFormatDecoder decoder;
        switch (colorformat)
        {
        case ColorFormat::Invalid:
        default:
        case ColorFormat::RGBA:
            decoder = RGBAdecoder;
            break;
        case ColorFormat::RGB:
            decoder = RGBdecoder;
            break;
        case ColorFormat::ARGB:
            decoder = ARGBdecoder;
            break;
        case ColorFormat::BGRA:
            decoder = BGRAdecoder;
            break;
        case ColorFormat::BGR:
            decoder = BGRdecoder;
            break;
        case ColorFormat::ABGR:
            decoder = ABGRdecoder;
            break;
        case ColorFormat::BAGR:
            decoder = BAGRdecoder;
            break;
        case ColorFormat::GrayScale:
            decoder = GSdecoder;
            break;
        case ColorFormat::CMY:
            decoder = CMYdecoder;
            break;
        case ColorFormat::CMYK:
            decoder = CMYKdecoder;
            break;
        case ColorFormat::HSL:
            decoder = HSLdecoder;
            break;
        case ColorFormat::HSLA:
            decoder = HSLAdecoder;
            break;
        case ColorFormat::HSV:
            decoder = HSVdecoder;
            break;
        case ColorFormat::HSVA:
            decoder = HSVAdecoder;
            break;
        case ColorFormat::Python:
            decoder = Pythondecoder;
            break;
        }
        size_t imageSize = width * height;
        size_t loopCount = min(fileSize, imageSize);
        const char* current = data;
        for (size_t i = 0; i < loopCount; i++)
            imagedata[i] = decoder(current);
        for (size_t i = loopCount; i < imageSize; i++)
            imagedata[i] = imagedata[i % loopCount];
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
static void RGBAencoder(char*& data, RGBQUAD color) {
    *(data++) = color.rgbRed;
    *(data++) = color.rgbGreen;
    *(data++) = color.rgbBlue;
    *(data++) = 0x00;
}
static void RGBencoder(char*& data, RGBQUAD color) {
    *(data++) = color.rgbRed;
    *(data++) = color.rgbGreen;
    *(data++) = color.rgbBlue;
}
static void ARGBencoder(char*& data, RGBQUAD color) {
    *(data++) = 0x00;
    *(data++) = color.rgbRed;
    *(data++) = color.rgbGreen;
    *(data++) = color.rgbBlue;
}
static void BGRAencoder(char*& data, RGBQUAD color) {
    *(data++) = color.rgbBlue;
    *(data++) = color.rgbGreen;
    *(data++) = color.rgbRed;
    *(data++) = 0x00;
}
static void BGRencoder(char*& data, RGBQUAD color) {
    *(data++) = color.rgbBlue;
    *(data++) = color.rgbGreen;
    *(data++) = color.rgbRed;
}
static void ABGRencoder(char*& data, RGBQUAD color) {
    *(data++) = 0x00;
    *(data++) = color.rgbBlue;
    *(data++) = color.rgbGreen;
    *(data++) = color.rgbRed;
}
static void BAGRencoder(char*& data, RGBQUAD color) {
    *(data++) = color.rgbBlue;
    *(data++) = 0x00;
    *(data++) = color.rgbGreen;
    *(data++) = color.rgbRed;
}
static void GSencoder(char*& data, RGBQUAD color) {
    *(data++) = (color.rgbRed + color.rgbGreen + color.rgbBlue) / 3;
}
static void CMYencoder(char*& data, RGBQUAD color) {
    *(data++) = 0xFF - color.rgbRed;
    *(data++) = 0xFF - color.rgbGreen;
    *(data++) = 0xFF - color.rgbBlue;
}
static void CMYKencoder(char*& data, RGBQUAD color) {
    unsigned char c = 0xFF - color.rgbRed;
    unsigned char m = 0xFF - color.rgbGreen;
    unsigned char y = 0xFF - color.rgbBlue;
    unsigned char k = min(c, min(m, y));
    *(data++) = c - k;
    *(data++) = m - k;
    *(data++) = y - k;
    *(data++) = k;
}
static void HSLencoder(char*& data, RGBQUAD color) {
    const uint8_t r = color.rgbRed, g = color.rgbGreen, b = color.rgbBlue;
    const uint8_t max = max(max(r, g), b),  min = min(min(r, g), b);
    const uint8_t l = (max + min) / 2;
    if (max == min) {
        *(data++) = 0;
        *(data++) = 0;
        *(data++) = l;
        return;
    }
    const uint8_t d = max - min;
    const uint8_t s = (l > 127) ? 255 * d / (2 * 255 - max - min) : 255 * d / (max + min);
    const uint8_t h = (max == r) ? ((g - b) * 42) / d : (max == g) ? ((b - r) * 42) / d + 84 : ((r - g) * 42) / d + 168;
    *(data++) = h;
    *(data++) = s;
    *(data++) = l;
    return;
}
static void HSLAencoder(char*& data, RGBQUAD color) {
    HSLencoder(data, color);
    *(data++) = 0x00;
    return;
}
static void HSVencoder(char*& data, RGBQUAD color) {
    const uint8_t r = color.rgbRed, g = color.rgbGreen, b = color.rgbBlue;
    const uint8_t max = max(max(r, g), b), min = min(min(r, g), b);
    const uint8_t d = max - min;
    if (d == 0)
    {
        *(data++) = 0;
        *(data++) = 0;
        *(data++) = max;
        return;
    }
    const uint8_t s = 255 * d / max;
    const uint8_t h = (max == r) ? ((g - b) * 42) / d : (max == g) ? ((b - r) * 42) / d + 84 : ((r - g) * 42) / d + 168;
    *(data++) = h;
    *(data++) = s;
    *(data++) = max;
    return;
}
static void HSVAencoder(char*& data, RGBQUAD color) {
    HSVencoder(data, color);
    *(data++) = 0x00;
    return;
}
static void Pythonencoder(char*& data, RGBQUAD color) {
    const uint8_t r = color.rgbRed, g = color.rgbGreen, b = color.rgbBlue;
    const uint8_t max = max(max(r, g), b), min = min(min(r, g), b);
    const uint16_t l = max + min;
    long double*& fdata = reinterpret_cast<long double*&>(data);

    if (max == min) {
        *(fdata++) = 0.0L;
        *(fdata++) = 0.0L;
        *(fdata++) = l / 512.0L;
        *(fdata++) = 0.0L;
        return;
    }

    const uint8_t d = max - min;
    const long double s = (l > 255) ? d / static_cast<long double>(2 * 255 - max - min) : d / static_cast<long double>(max + min);
    const long double h = (max == r) ? ((g - b) * 60) / static_cast<long double>(d) + (g < b ? 3600.L : 0.0L) : (max == g) ? ((b - r) * 60) / static_cast<long double>(d) + 120 : ((r - g) * 60) / static_cast<long double>(d) + 240;
    *(fdata++) = h;
    *(fdata++) = s;
    *(fdata++) = l / 512.0L;
    *(fdata++) = 0.0L;
    return;
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
    DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_COLORMODEL_DIALOG), hwnd, ColorQueryDialogProc);
    // Create or open the file for writing asynchronously
    HANDLE file = CreateFileW(path, FILE_GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
    size_t fileSize = width * height; // in pixels
    size_t pixelSize = get_pixelSize(colorformat);
    char* data = new char[fileSize * pixelSize];
    // The image is parsed and copied to a new buffer
    {
        ColorFormatEncoder encoder;
        switch (colorformat)
        {
        case ColorFormat::Invalid:
        default:
        case ColorFormat::RGBA:
            encoder = RGBAencoder;
            break;
        case ColorFormat::RGB:
            encoder = RGBencoder;
            break;
        case ColorFormat::ARGB:
            encoder = ARGBencoder;
            break;
        case ColorFormat::BGRA:
            encoder = BGRAencoder;
            break;
        case ColorFormat::BGR:
            encoder = BGRencoder;
            break;
        case ColorFormat::ABGR:
            encoder = ABGRencoder;
            break;
        case ColorFormat::BAGR:
            encoder = BAGRencoder;
            break;
        case ColorFormat::GrayScale:
            encoder = GSencoder;
            break;
        case ColorFormat::CMY:
            encoder = CMYencoder;
            break;
        case ColorFormat::CMYK:
            encoder = CMYKencoder;
            break;
        case ColorFormat::HSL:
            encoder = HSLencoder;
            break;
        case ColorFormat::HSLA:
            encoder = HSLAencoder;
            break;
        case ColorFormat::HSV:
            encoder = HSVencoder;
            break;
        case ColorFormat::HSVA:
            encoder = HSVAencoder;
            break;
        case ColorFormat::Python:
            encoder = Pythonencoder;
            break;
        }
        char* current = data;
        for (size_t i = 0; i < fileSize; ++i)
            encoder(current, imagedata[i]);
    }
    if (fmt == ImageFormat::txt) {
        char* olddata = data;
        data = new char[fileSize * pixelSize * 8];
        char* current = data;
        for (size_t i = 0; i < fileSize * pixelSize; ++i) {
            writetxtbyte(current, olddata[i]);
        }
        delete[] olddata;
    }
    // Saving the image
    {
        OVERLAPPED overlapped;
        ZeroMemory(&overlapped, sizeof(OVERLAPPED));
        overlapped.hEvent = CreateEventExW(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE);
        (void)WriteFileEx(file, data, fileSize * pixelSize * (fmt == ImageFormat::txt ? 8 : 1), &overlapped, writefileexCallback);
        (void)WaitForSingleObjectEx(overlapped.hEvent, 1000, TRUE);
        delete[] data;
    }
    CloseHandle(file);
    // MessageBoxExW(NULL, L"File saved successfully", L"Success", MB_OK | MB_ICONINFORMATION, NULL);
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
        SetStretchBltMode(hdc, HALFTONE);
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