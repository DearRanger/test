#include <windows.h>
#include <commctrl.h>
#include <d3d11.h>
#include <mfapi.h>
#include <mfcaptureengine.h>
#include <shlwapi.h>
#include <new>
#include <wincodec.h>

#include <atlconv.h>
#include <atlbase.h>
#include <sstream>

#include <algorithm>
#include <iomanip>
#include <ctime>

#include "ocr/ocr.h"

#pragma warning(disable:4996)
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")

#pragma comment(lib, "lib/jsoncpp.lib")
#pragma comment(lib, "lib/libcrypto.lib")
#pragma comment(lib, "lib/libcurl.dll.a")

#define CHECK( val )		_check((val), #val, __FILE__, __LINE__)
template <typename T> void _check(T err, const char* const func, const char* const file, const int line) {
    if (err < 0) {
        wchar_t buf[200];
        ZeroMemory(buf, sizeof(buf));
        wsprintf(buf, TEXT("Error Code is: %d \r\n line number is: %d"), err,  __LINE__);
        MessageBox(NULL, buf, TEXT("ERROR"), MB_ICONERROR);
    }
}

template <typename T> void SafeRelease(T **ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    } else
        return;
}

std::string app_id = "27189478";
std::string api_key = "Ao5vidUiyjCycdua7G0t0rRi";
std::string secret_key = "a3IkBZcIxpqRDdmKQDa1YvUt7G4MKeRw";

const wchar_t winClass[] = TEXT("THE_BQCDZ_MAIN_CLASS");
const wchar_t preClass[] = TEXT("THE_BQCDZ_PREVIEW_CLASS");
const wchar_t winName[] = TEXT("深圳市百千成电子有限公司-文字识别平台-V1.0");
const wchar_t preName[] = TEXT("THE_BQCDZ_PREVIEW_NAME");

#define IDR_WINCLASS			(1000)
#define IDR_PRECLASS		(1001)
#define IDR_WINEDIT				(1002)

#define MESSAGE_ENGINE_INITIALIZED			(WM_USER + 2000)
#define MESSAGE_ENGINE_PREVIEW				(WM_USER + 2001)
#define MESSAGE_ENGINE_HANDLE				(WM_USER + 2002)
#define MESSAGE_PREVIEW_HANDLE			(WM_USER + 2003)
#define MESSAGE_ENGINE_TAKE_PHOTO		(WM_USER + 2004)
#define MESSAGE_ENGINE_PHOTO_OCR		(WM_USER + 2005)


LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK PreviewProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

std::string UTF8ToGBK(const char* strUTF8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, strUTF8, -1, NULL, 0);
    wchar_t* wszGBK = new wchar_t[len + 1];
    memset(wszGBK, 0, len * 2 + 2);
    MultiByteToWideChar(CP_UTF8, 0, strUTF8, -1, wszGBK, len);
    len = WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, NULL, 0, NULL, NULL);
    char* szGBK = new char[len + 1];
    memset(szGBK, 0, len + 1);
    WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, szGBK, len, NULL, NULL);
    std::string strTemp(szGBK);
    if (wszGBK) delete[] wszGBK;
    if (szGBK) delete[] szGBK;
    return strTemp;
}
std::wstring GBK2Wide(const std::string& strAnsi) {
    int nWide = ::MultiByteToWideChar(CP_ACP, 0, strAnsi.c_str(), strAnsi.size(), NULL, 0);

    std::unique_ptr<wchar_t[]> buffer(new wchar_t[nWide + 1]);
    if (!buffer) {
        return L"";
    }

    MultiByteToWideChar(CP_ACP, 0, strAnsi.c_str(), strAnsi.size(), buffer.get(), nWide);
    buffer[nWide] = L'\0';

    return buffer.get();
}

std::string GetTime(void) {
    auto time = std::time(nullptr);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%F-%T");
    auto s = ss.str();
    std::replace(s.begin(), s.end(), ':', '-');
    s.erase(std::remove(s.begin(), s.end(), '-'), s.end());
    s += ".jpg";
    return s;
}

void D3D11DeviceInitialize(void) {
    ID3D11Device *pDevice = NULL;
    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };
    ID3D11DeviceContext *pContext = NULL;
    D3D_FEATURE_LEVEL level;
    CHECK(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_VIDEO_SUPPORT, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &pDevice, &level, &pContext));
    ID3D10Multithread *pMultithread = NULL;
    CHECK(pDevice->QueryInterface(IID_PPV_ARGS(&pMultithread)));
    CHECK(pMultithread->SetMultithreadProtected(TRUE));
    IMFDXGIDeviceManager *pDeviceManager = NULL;
    UINT token = 0;
    CHECK(MFCreateDXGIDeviceManager(&token, &pDeviceManager));
    CHECK(pDeviceManager->ResetDevice(pDevice, token));

    IMFAttributes *pAttributes = NULL;
    CHECK(MFCreateAttributes(&pAttributes, 1));
    CHECK(pAttributes->SetUnknown(MF_CAPTURE_ENGINE_D3D_MANAGER, pDeviceManager));

    SafeRelease(&pAttributes);
    SafeRelease(&pDeviceManager);
    SafeRelease(&pMultithread);
    SafeRelease(&pDevice);
    SafeRelease(&pContext);
}

class CaptureEngineCB :public IMFCaptureEngineOnEventCallback {
    long mRef;
    HWND mhWnd;
  public:
    CaptureEngineCB(HWND hwnd) :mRef(1) {
        mhWnd = hwnd;
    }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        static const QITAB qit[] = {
            QITABENT(CaptureEngineCB, IMFCaptureEngineOnEventCallback),
            {0}
        };
        return QISearch(this, qit, riid, ppv);
    }
    STDMETHODIMP_(ULONG) AddRef() {
        return InterlockedIncrement(&mRef);
    }
    STDMETHODIMP_(ULONG) Release() {
        LONG cRef = InterlockedDecrement(&mRef);
        if (cRef == 0) {
            delete this;
        }
        return cRef;
    }
    STDMETHODIMP OnEvent(_In_ IMFMediaEvent* pEvent) {
        GUID guidType;
        pEvent->GetExtendedType(&guidType);
        //HWND hWndPreview = FindWindow(preClass, preName);
        if (guidType == MF_CAPTURE_ENGINE_INITIALIZED) {
            SendMessage(mhWnd, MESSAGE_ENGINE_INITIALIZED, NULL, NULL);
        } else if (guidType == MF_CAPTURE_ENGINE_PREVIEW_STARTED) {
            SendMessage(mhWnd, MESSAGE_ENGINE_PREVIEW, NULL, NULL);
        } else if (guidType == MF_CAPTURE_ENGINE_PREVIEW_STOPPED) {

        } else if (guidType == MF_CAPTURE_ENGINE_RECORD_STARTED) {

        } else if (guidType == MF_CAPTURE_ENGINE_RECORD_STOPPED) {

        } else if (guidType == MF_CAPTURE_ENGINE_PHOTO_TAKEN) {
            SendMessage(mhWnd, MESSAGE_ENGINE_PHOTO_OCR, NULL, NULL);
        } else {
        }
        return S_OK;
    }
};

void EngineInitialize(HWND hwnd) {
    IMFCaptureEngineClassFactory *pFactory = NULL;
    CHECK(CoCreateInstance(CLSID_MFCaptureEngineClassFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory)));
    IMFCaptureEngine *pEngine = NULL;
    CHECK(pFactory->CreateInstance(CLSID_MFCaptureEngine, IID_PPV_ARGS(&pEngine)));
    SendMessage(hwnd, MESSAGE_ENGINE_HANDLE, (WPARAM)pEngine, NULL);
    CaptureEngineCB *pCallback = new(std::nothrow)CaptureEngineCB(hwnd);
    IMFAttributes *pAttributes = NULL;
    CHECK(MFCreateAttributes(&pAttributes, 1));
    CHECK(pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID));
    IMFActivate **ppDevices = NULL;
    UINT32 count = 0;
    CHECK(MFEnumDeviceSources(pAttributes, &ppDevices, &count));
    if (count < 1) {
        MessageBox(NULL, NULL, TEXT("There is no camera ......"), MB_ICONERROR);
        PostQuitMessage(0);
    } else {
        CHECK(pEngine->Initialize(pCallback, pAttributes, NULL, ppDevices[count - 1]));
    }

    SafeRelease(ppDevices);
    SafeRelease(&pAttributes);
    SafeRelease(&pFactory);
}
INT WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ INT nCmdShow) {
    InitCommonControls();
    CHECK(CoInitialize(NULL));
	//注册窗口wc，wp
    WNDCLASS wc, wp;
    ZeroMemory(&wc, sizeof(wc));
    ZeroMemory(&wp, sizeof(wp));

    wc.style = CS_HREDRAW | CS_VREDRAW;  //指定窗口类的样式，其中CS_HREDRAW和CS_VREDRAW分别表示在水平和垂直方向上重绘窗口
    wc.lpfnWndProc = WindowProc;  //指定窗口类的消息处理函数，即窗口过程（window procedure）
    wc.cbClsExtra = 0;  //指定窗口类的额外类内存大小，此处为0
    wc.cbWndExtra = sizeof(long);  //指定窗口类的额外窗口内存大小，此处为long类型的大小
    wc.hInstance = hInstance;  //指定窗口类所属的实例句柄，即程序的句柄
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);  //指定窗口类的图标，此处为应用程序图标
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);  //指定窗口类的光标，此处为箭头光标
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);  //指定窗口类的背景画刷，此处为白色
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_WINCLASS); //指定窗口类的菜单资源，此处为IDR_WINCLASS
    wc.lpszClassName = winClass;  //指定窗口类的类名，即程序中唯一的标识符
    CHECK(RegisterClass(&wc));  //向系统注册窗口并且检查是否出错
	//创建窗口
    HWND hWndWin = CreateWindow(
		winClass, //窗口类的名称 此名称定义要创建的窗口的类型
		winName,  //如果窗口具有标题栏，则文本将显示在标题栏中，此处为之前定义的宽字符串"深圳市百千成电子有限公司-文字识别平台-V1.0"
		(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME) ^ WS_THICKFRAME,  //设置窗口样式
		0, 0, 1280, 720,  //设置窗口位置
		NULL, //顶级窗口，父窗口
		NULL, //无菜单
		hInstance,  //实例句柄
		NULL
	);
    ShowWindow(hWndWin, nCmdShow); //显示窗口
    UpdateWindow(hWndWin); //更新窗口

    wp.style = CS_HREDRAW | CS_VREDRAW;
    wp.lpfnWndProc = PreviewProc;
    wp.cbClsExtra = 0;
    wp.cbWndExtra = sizeof(long);
    wp.hInstance = hInstance;
    wp.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wp.hCursor = LoadCursor(NULL, IDC_ARROW);
    wp.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wp.lpszMenuName = MAKEINTRESOURCE(IDR_PRECLASS);
    wp.lpszClassName = preClass;

    CHECK(RegisterClass(&wp));
	//创建窗口
    HWND hWndPre = CreateWindow(
		preClass, 
		preName, 
		WS_CHILDWINDOW | WS_VISIBLE, 
		640, 0, 640, 360, 
		hWndWin, //设置父窗口
		NULL,  //无菜单
		hInstance, 
		NULL
	);
    ShowWindow(hWndPre, nCmdShow);
    UpdateWindow(hWndPre);

    D3D11DeviceInitialize();
    EngineInitialize(hWndPre);
    SendMessage(hWndWin, MESSAGE_PREVIEW_HANDLE, (WPARAM)hWndPre, NULL);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);  //消息翻译
        DispatchMessage(&msg);  //消息分发
    }
    CoUninitialize(); //关闭当前线程的COM库,卸载线程加载的所有dll,释放任何其他的资源,关闭在线程上维护所有的RPC连接
    return msg.wParam;
}

LRESULT CALLBACK WinEditProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM IParam, UINT_PTR uldSubclass, DWORD_PTR dwRefData) {
    static HWND hPreview;
    switch (message) {
    case MESSAGE_PREVIEW_HANDLE: {
        hPreview = reinterpret_cast<HWND>(wParam);
    }
    break;
    case WM_KEYDOWN: {
        if (wParam == 13) {
            SendMessage(hPreview, MESSAGE_ENGINE_TAKE_PHOTO, NULL, NULL);
        }
    }
    break;
    case WM_NCDESTROY: {
        RemoveWindowSubclass(hWnd, &WinEditProc, 1);
    }
    break;
    default:
        break;
    }
    return DefSubclassProc(hWnd, message, wParam, IParam);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static HWND hEdit;  //定义一个静态的 HWND 类型变量 hEdit，用于创建一个编辑框控件

    switch (message) {
    case WM_CREATE: {   //创建一个编辑框控件。
        hEdit = CreateWindow(
			TEXT("edit"), 
			TEXT(""), 
			WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 
			440, 600, 400, 30, 
			hWnd, 
			(HMENU)IDR_WINEDIT, 
			GetModuleHandle(NULL), 
			NULL
		);
        SetWindowSubclass(hEdit, &WinEditProc, 1, 0);  //关联 WinEditProc 子类过程与编辑框控件 hEdit ，这样在编辑框控件的消息处理中就可以使用 WinEditProc 函数处理特定的消息

        SetFocus(hEdit);
    }
    break;
    case MESSAGE_PREVIEW_HANDLE: {
        HWND hPreview = reinterpret_cast<HWND>(wParam);
        if (hPreview != NULL)
            SendMessage(hEdit, MESSAGE_PREVIEW_HANDLE, (WPARAM)hPreview, NULL);
    }
    break;
    case WM_DESTROY: {
        PostQuitMessage(0);
    }
    break;
    default:
        break;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}
LRESULT CALLBACK PreviewProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM IParam) {
    static IMFCaptureEngine *pEngine = NULL;
    static std::string photoFileName;
    switch (message) {
    case MESSAGE_ENGINE_HANDLE: {
        pEngine = reinterpret_cast<IMFCaptureEngine*> (wParam);
        //MessageBox(hWnd, TEXT("MESSAGE_ENGINE_HANDLE"), TEXT("Handle"), MB_ICONERROR);
    }
    break;
    case WM_DESTROY: {
        SafeRelease(&pEngine);
        PostQuitMessage(0);
    }
    break;
    case MESSAGE_ENGINE_PREVIEW: {
        //MessageBox(hWnd, TEXT("MESSAGE_ENGINE_PREVIEW"), TEXT("Preview"), MB_ICONERROR);
    }
    break;
    case MESSAGE_ENGINE_INITIALIZED: {
        IMFCaptureSource *pSource = NULL;
        CHECK(pEngine->GetSource(&pSource));
        IMFMediaType *pSrcMediaType = NULL;
        CHECK(pSource->GetCurrentDeviceMediaType((DWORD)MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_VIDEO_PREVIEW, &pSrcMediaType));

        IMFMediaType *pDesMediaType = NULL;
        ////Copy SrcMediaType to DesMediaType
        CHECK(MFCreateMediaType(&pDesMediaType));
        CHECK(pDesMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
        CHECK(pDesMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32));
        PROPVARIANT var;
        PropVariantInit(&var);

        CHECK(pSrcMediaType->GetItem(MF_MT_FRAME_SIZE, &var));
        CHECK(pDesMediaType->SetItem(MF_MT_FRAME_SIZE, var));
        PropVariantClear(&var);

        CHECK(pSrcMediaType->GetItem(MF_MT_FRAME_RATE, &var));
        CHECK(pDesMediaType->SetItem(MF_MT_FRAME_RATE, var));
        PropVariantClear(&var);

        CHECK(pSrcMediaType->GetItem(MF_MT_PIXEL_ASPECT_RATIO, &var));
        CHECK(pDesMediaType->SetItem(MF_MT_PIXEL_ASPECT_RATIO, var));
        PropVariantClear(&var);

        CHECK(pSrcMediaType->GetItem(MF_MT_INTERLACE_MODE, &var));
        CHECK(pDesMediaType->SetItem(MF_MT_INTERLACE_MODE, var));
        PropVariantClear(&var);

        CHECK(pDesMediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE));

        IMFCaptureSink *pSink = NULL;
        CHECK(pEngine->GetSink(MF_CAPTURE_ENGINE_SINK_TYPE_PREVIEW, &pSink));

        IMFCapturePreviewSink *pPreview = NULL;
        pSink->QueryInterface(IID_PPV_ARGS(&pPreview));
        pPreview->SetRenderHandle(hWnd);

        DWORD dStreamIndex;
        pPreview->AddStream((DWORD)MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_VIDEO_PREVIEW, pDesMediaType, NULL, &dStreamIndex);
        pEngine->StartPreview();


        SafeRelease(&pPreview);
        SafeRelease(&pSink);
        SafeRelease(&pDesMediaType);
        SafeRelease(&pSrcMediaType);
        SafeRelease(&pSource);
    }
    break;
    case MESSAGE_ENGINE_TAKE_PHOTO: {
        IMFCaptureSink *pPhotoSink = NULL;
        CHECK(pEngine->GetSink(MF_CAPTURE_ENGINE_SINK_TYPE_PHOTO, &pPhotoSink));
        IMFCapturePhotoSink *pPhoto = NULL;
        CHECK(pPhotoSink->QueryInterface(IID_PPV_ARGS(&pPhoto)));
        IMFCaptureSource *pSource = NULL;
        CHECK(pEngine->GetSource(&pSource));
        IMFMediaType *pSrcType = NULL;
        CHECK(pSource->GetCurrentDeviceMediaType((DWORD)MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_PHOTO, &pSrcType));
        IMFMediaType *pDesType = NULL;
        CHECK(MFCreateMediaType(&pDesType));
        CHECK(pDesType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Image));
        CHECK(pDesType->SetGUID(MF_MT_SUBTYPE, GUID_ContainerFormatJpeg));
        PROPVARIANT var;
        PropVariantInit(&var);
        pSrcType->GetItem(MF_MT_FRAME_SIZE, &var);
        pDesType->SetItem(MF_MT_FRAME_SIZE, var);
        PropVariantClear(&var);
        pPhoto->RemoveAllStreams();
        DWORD nIndex;
        pPhoto->AddStream((DWORD)MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_PHOTO, pDesType, NULL, &nIndex);
        std::string fileName = GetTime();
        LPCWSTR photoName = CA2T(fileName.c_str());
        pPhoto->SetOutputFileName(photoName);

        pEngine->TakePhoto();
        photoFileName = fileName;

        SafeRelease(&pDesType);
        SafeRelease(&pSrcType);
        SafeRelease(&pSource);
        SafeRelease(&pPhoto);
        SafeRelease(&pPhotoSink);
    }
    break;
    case MESSAGE_ENGINE_PHOTO_OCR: {
        aip::Ocr client(app_id, api_key, secret_key);
        std::string image;
        aip::get_file_content(photoFileName.c_str(), &image);
        Json::Value result;
        result = client.general_basic(image, aip::null);
        std::map<std::string, std::string> options;
        options["language_type"] = "CHN_ENG";
        options["detect_direction"] = "true";
        options["detect_language"] = "true";
        options["probability"] = "true";
        result = client.general_basic(image, options);
        Json::Value res = result["words_result"];
        std::string last;
        for (int i = 0; i < res.size(); i++) {
            last += UTF8ToGBK(res[i]["words"].asString().c_str());
            //last += res[i]["words"].asString();
        }

        std::wstring output = GBK2Wide(last);

        MessageBox(hWnd, output.c_str(), TEXT("Result:"), MB_USERICON);


        //MessageBox(hWnd, TEXT("MESSAGE_ENGINE_PHOTO_OCR"), TEXT("Photo Taken"), MB_USERICON);
    }
    break;
    default:
        break;
    }
    return DefWindowProc(hWnd, message, wParam, IParam);
}