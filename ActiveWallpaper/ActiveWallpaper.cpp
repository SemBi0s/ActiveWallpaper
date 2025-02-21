// ActiveWallpaper.cpp : Définit le point d'entrée de l'application.
//

#define WINVER _WIN32_WINNT_WIN7
#include "framework.h"
#include "ActiveWallpaper.h"
#include "player.h"
#include "PlayerSeeking.h"



#include <windows.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <vector>
#include <dshow.h>
#include <cstdlib>
#include <tchar.h>
#include <Gdiplus.h>
#include <shobjidl.h>
#include <strsafe.h>

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    HWND p = FindWindowEx(hwnd, NULL, L"SHELLDLL_DefView", NULL);
    HWND* ret = (HWND*)lParam;

    if (p)
    {
        // Gets the WorkerW Window after the current one.
        *ret = FindWindowEx(NULL, hwnd, L"WorkerW", NULL);
    }
    return true;
}
HWND get_wallpaper_window()
{

    // Fetch the Progman window
    HWND progman = FindWindow(L"ProgMan", NULL);
    // Send 0x052C to Progman. This message directs Progman to spawn a 
    // WorkerW behind the desktop icons. If it is already there, nothing 
    // happens.
    SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);
    // We enumerate all Windows, until we find one, that has the SHELLDLL_DefView 
    // as a child. 
    // If we found that window, we take its next sibling and assign it to workerw.

    HWND wallpaper_hwnd = nullptr;
    EnumWindows(EnumWindowsProc, (LPARAM)&wallpaper_hwnd);
    // Return the handle you're looking for.
    return wallpaper_hwnd;
}


// Include the v6 common controls in the manifest
#pragma comment(linker, \
    "\"/manifestdependency:type='Win32' "\
    "name='Microsoft.Windows.Common-Controls' "\
    "version='6.0.0.0' "\
    "processorArchitecture='*' "\
    "publicKeyToken='6595b64144ccf1df' "\
    "language='*'\"")


BOOL    InitializeWindow(HWND* pHwnd);
HRESULT PlayMediaFile(HWND hwnd, const WCHAR* sURL);
void    ShowErrorMessage(PCWSTR format, HRESULT hr);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Window message handlers
void    OnClose(HWND hwnd);
void    OnPaint(HWND hwnd);
void    OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
void    OnSize(HWND hwnd, UINT state, int cx, int cy);
void    OnKeyDown(HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags);

// Menu handlers
void    OnFileOpen(HWND hwnd);

// MFPlay event handler functions.
void OnMediaItemCreated(MFP_MEDIAITEM_CREATED_EVENT* pEvent);
void OnMediaItemSet(MFP_MEDIAITEM_SET_EVENT* pEvent);

// Constants 
const WCHAR CLASS_NAME[] = L"MFPlay Window Class";
const WCHAR WINDOW_NAME[] = L"MFPlay Sample Application";

//-------------------------------------------------------------------
//
// MediaPlayerCallback class
// 
// Implements the callback interface for MFPlay events.
//
//-------------------------------------------------------------------


PlayerSeeking player_seeking = PlayerSeeking();


#include <Shlwapi.h>

class MediaPlayerCallback : public IMFPMediaPlayerCallback
{
    long m_cRef; // Reference count

public:

    MediaPlayerCallback() : m_cRef(1)
    {
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
    {
        static const QITAB qit[] =
        {
            QITABENT(MediaPlayerCallback, IMFPMediaPlayerCallback),
            { 0 },
        };
        return QISearch(this, qit, riid, ppv);
    }
    STDMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&m_cRef);
    }
    STDMETHODIMP_(ULONG) Release()
    {
        ULONG count = InterlockedDecrement(&m_cRef);
        if (count == 0)
        {
            delete this;
            return 0;
        }
        return count;
    }

    // IMFPMediaPlayerCallback methods
    void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER* pEventHeader);
};


// Global variables

IMFPMediaPlayer* g_pPlayer = NULL;      // The MFPlay player object.
MediaPlayerCallback* g_pPlayerCB = NULL;    // Application callback object.

BOOL g_bHasVideo = FALSE;
PROPVARIANT* duration = NULL;
PWSTR pwszFilePath = NULL;
/////////////////////////////////////////////////////////////////////

INT WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, INT)
{
    (void)HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    HWND hwnd = 0;
    MSG msg = { 0 };

    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
    {
        return 0;
    }

    if (!InitializeWindow(&hwnd))
    {
        return 0;
    }

    // Message loop
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DestroyWindow(hwnd);
    CoUninitialize();

    return 0;
}


//-------------------------------------------------------------------
// WindowProc
//
// Main window procedure.
//-------------------------------------------------------------------

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_CLOSE, OnClose);
       
        HANDLE_MSG(hwnd, WM_PAINT, OnPaint);
       

    case WM_ERASEBKGND:
        return 1;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}


//-------------------------------------------------------------------
// InitializeWindow
//
// Creates the main application window.
//-------------------------------------------------------------------

BOOL InitializeWindow(HWND* pHwnd)
{
    const wchar_t CLASS_NAME[] = L"WallpaperDisplayer";
    const wchar_t WINDOW_NAME[] = L"";

    WNDCLASS wc = { 0 };

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = CLASS_NAME;
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
    

    if (!RegisterClass(&wc))
    {
        MessageBox(NULL,
            _T("Call to RegisterClassEx failed!"),
            _T("Windows Desktop Guided Tour"),
            NULL);

        return 1;
    }


    // TODO: CODE HERE

    SetProcessDPIAware();

    int x = GetSystemMetrics(SM_CXSCREEN);
    int y = GetSystemMetrics(SM_CYSCREEN);
    // The parameters to CreateWindow explained:
    // szWindowClass: the name of the application
    // szTitle: the text that appears in the title bar
    // WS_OVERLAPPEDWINDOW: the type of window to create
    // CW_USEDEFAULT, CW_USEDEFAULT: initial position (x, y)
    // 500, 100: initial size (width, length)
    // NULL: the parent of this window
    // NULL: this application does not have a menu bar
    // hInstance: the first parameter from WinMain
    // NULL: not used in this application

   

    HWND hWnd = CreateWindow(
        CLASS_NAME,
        WINDOW_NAME,
        WS_CHILD,
        CW_USEDEFAULT, CW_USEDEFAULT,
        x, y,
        get_wallpaper_window(),
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!hWnd)
    {
        MessageBox(NULL,
            _T("Call to CreateWindow failed!"),
            _T("Windows Desktop Guided Tour"),
            NULL);

        return FALSE;
    }





    // The parameters to ShowWindow explained:
    // hWnd: the value returned from CreateWindow
    // nCmdShow: the fourth parameter from WinMain
    ShowWindow(hWnd,
        SW_SHOWDEFAULT);
    UpdateWindow(hWnd);
    *pHwnd = hWnd;


    return TRUE;
}


//-------------------------------------------------------------------
// OnClose
//
// Handles the WM_CLOSE message.
//-------------------------------------------------------------------

void OnClose(HWND /*hwnd*/)
{
    if (g_pPlayer)
    {
        g_pPlayer->Shutdown();
        g_pPlayer->Release();
        g_pPlayer = NULL;
    }

    if (g_pPlayerCB)
    {
        g_pPlayerCB->Release();
        g_pPlayerCB = NULL;
    }

    PostQuitMessage(0);
}


//-------------------------------------------------------------------
// OnPaint
//
// Handles the WM_PAINT message.
//-------------------------------------------------------------------

int m = 0;

void OnPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = 0;

    MFP_MEDIAPLAYER_STATE state = MFP_MEDIAPLAYER_STATE_EMPTY;
   
	
    hdc = BeginPaint(hwnd, &ps);
   
    
    
    
    if (g_pPlayer && g_bHasVideo)
    {
        if (m == 1)
        {
            PlayMediaFile(hwnd, pwszFilePath);
            m = -1;
        }
        // frame fills the entire client area.
        m++;
    	
        g_pPlayer->UpdateVideo();
        
    }
    else
    {
        // There is no video stream, or playback has not started.
        // Paint the entire client area.
        
        OnFileOpen(hwnd);
       
        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
    }
   

   
	
    EndPaint(hwnd, &ps);
}


//-------------------------------------------------------------------
// OnSize
//
// Handles the WM_SIZE message.
//-------------------------------------------------------------------

void OnSize(HWND /*hwnd*/, UINT state, int /*cx*/, int /*cy*/)
{
    if (state == SIZE_RESTORED)
    {
        if (g_pPlayer)
        {
            // Resize the video.
            g_pPlayer->UpdateVideo();
        }
    }
}


//-------------------------------------------------------------------
// OnKeyDown
//
// Handles the WM_KEYDOWN message.
//-------------------------------------------------------------------

void OnKeyDown(HWND /*hwnd*/, UINT vk, BOOL /*fDown*/, int /*cRepeat*/, UINT /*flags*/)
{
    HRESULT hr = S_OK;

    switch (vk)
    {
    case VK_SPACE:

        // Toggle between playback and paused/stopped.
        if (g_pPlayer)
        {
            MFP_MEDIAPLAYER_STATE state = MFP_MEDIAPLAYER_STATE_EMPTY;

            hr = g_pPlayer->GetState(&state);

            if (SUCCEEDED(hr))
            {
                if (state == MFP_MEDIAPLAYER_STATE_PAUSED || state == MFP_MEDIAPLAYER_STATE_STOPPED)
                {
                    hr = g_pPlayer->Play();
                }
                else if (state == MFP_MEDIAPLAYER_STATE_PLAYING)
                {
                    hr = g_pPlayer->Pause();
                }
            }
        }
        break;
    }

    if (FAILED(hr))
    {
        ShowErrorMessage(TEXT("Playback Error"), hr);
    }
}


//-------------------------------------------------------------------
// OnCommand
// 
// Handles the WM_COMMAND message.
//-------------------------------------------------------------------

void OnCommand(HWND hwnd, int id, HWND /*hwndCtl*/, UINT /*codeNotify*/)
{
	
    switch (id)
    {
    case ID_FILE_OPEN:
        OnFileOpen(hwnd);
        break;

    case ID_FILE_EXIT:
        OnClose(hwnd);
        break;
    }
}


//-------------------------------------------------------------------
// OnFileOpen
//
// Handles the "File Open" command.
//-------------------------------------------------------------------

void OnFileOpen(HWND hwnd)
{
    HRESULT hr = S_OK;

    IFileOpenDialog* pFileOpen = NULL;
    IShellItem* pItem = NULL;
    
    

    // Create the FileOpenDialog object.
    hr = CoCreateInstance(
        __uuidof(FileOpenDialog),
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pFileOpen)
    );

    if (FAILED(hr)) { goto done; }


    hr = pFileOpen->SetTitle(L"Select a File to Play");

    if (FAILED(hr)) { goto done; }


    // Show the file-open dialog.
    hr = pFileOpen->Show(hwnd);

    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
    {
        // User cancelled.
        hr = S_OK;
        goto done;
    }
    if (FAILED(hr)) { goto done; }


    // Get the file name from the dialog.
    hr = pFileOpen->GetResult(&pItem);

    if (FAILED(hr)) { goto done; }


    hr = pItem->GetDisplayName(SIGDN_URL, &pwszFilePath);

    if (FAILED(hr)) { goto done; }
    

    // Open the media file.
    hr = PlayMediaFile(hwnd, pwszFilePath);

    if (FAILED(hr)) { goto done; }
    

done:
    if (FAILED(hr))
    {
        ShowErrorMessage(L"Could not open file.", hr);
    }

   

    if (pItem)
    {
        pItem->Release();
    }
    if (pFileOpen)
    {
        pFileOpen->Release();
    }
   
   
}


//-------------------------------------------------------------------
// PlayMediaFile
//
// Plays a media file, using the IMFPMediaPlayer interface.
//-------------------------------------------------------------------

HRESULT PlayMediaFile(HWND hwnd, const WCHAR* sURL)
{
    HRESULT hr = S_OK;

    // Create the MFPlayer object.
    if (g_pPlayer == NULL)
    {
        g_pPlayerCB = new (std::nothrow) MediaPlayerCallback();

        if (g_pPlayerCB == NULL)
        {
            hr = E_OUTOFMEMORY;
            goto done;
        }

        hr = MFPCreateMediaPlayer(
            NULL,
            FALSE,          // Start playback automatically?
            0,              // Flags
            g_pPlayerCB,    // Callback pointer
            hwnd,           // Video window
            &g_pPlayer
        );
        

        if (FAILED(hr)) { goto done; }
    }

    // Create a new media item for this URL.
    hr = g_pPlayer->CreateMediaItemFromURL(sURL, FALSE, 0, NULL);

    // The CreateMediaItemFromURL method completes asynchronously. 
    // The application will receive an MFP_EVENT_TYPE_MEDIAITEM_CREATED 
    // event. See MediaPlayerCallback::OnMediaPlayerEvent().


done:
    return hr;
}


//-------------------------------------------------------------------
// OnMediaPlayerEvent
// 
// Implements IMFPMediaPlayerCallback::OnMediaPlayerEvent.
// This callback method handles events from the MFPlay object.
//-------------------------------------------------------------------

void MediaPlayerCallback::OnMediaPlayerEvent(MFP_EVENT_HEADER* pEventHeader)
{
    if (FAILED(pEventHeader->hrEvent))
    {
        ShowErrorMessage(L"Playback error", pEventHeader->hrEvent);
        return;
    }

    switch (pEventHeader->eEventType)
    {
    case MFP_EVENT_TYPE_MEDIAITEM_CREATED:
        OnMediaItemCreated(MFP_GET_MEDIAITEM_CREATED_EVENT(pEventHeader));
        break;

    case MFP_EVENT_TYPE_MEDIAITEM_SET:
        OnMediaItemSet(MFP_GET_MEDIAITEM_SET_EVENT(pEventHeader));
        break;
    }
}


//-------------------------------------------------------------------
// OnMediaItemCreated
//
// Called when the IMFPMediaPlayer::CreateMediaItemFromURL method
// completes.
//-------------------------------------------------------------------

void OnMediaItemCreated(MFP_MEDIAITEM_CREATED_EVENT* pEvent)
{
    HRESULT hr = S_OK;

    // The media item was created successfully.

    if (g_pPlayer)
    {
        BOOL bHasVideo = FALSE, bIsSelected = FALSE;

        // Check if the media item contains video.
        hr = pEvent->pMediaItem->HasVideo(&bHasVideo, &bIsSelected);

        if (FAILED(hr)) { goto done; }

        g_bHasVideo = bHasVideo && bIsSelected;

        // Set the media item on the player. This method completes asynchronously.
        hr = g_pPlayer->SetMediaItem(pEvent->pMediaItem);
    }

done:
    if (FAILED(hr))
    {
        ShowErrorMessage(L"Error playing this file.", hr);
    }
}


//-------------------------------------------------------------------
// OnMediaItemSet
//
// Called when the IMFPMediaPlayer::SetMediaItem method completes.
//-------------------------------------------------------------------

void OnMediaItemSet(MFP_MEDIAITEM_SET_EVENT* /*pEvent*/)
{
    HRESULT hr = S_OK;

    hr = g_pPlayer->Play();

    if (FAILED(hr))
    {
        ShowErrorMessage(L"IMFPMediaPlayer::Play failed.", hr);
    }
}


void ShowErrorMessage(PCWSTR format, HRESULT hrErr)
{
    HRESULT hr = S_OK;
    WCHAR msg[MAX_PATH];

    hr = StringCbPrintf(msg, sizeof(msg), L"%s (hr=0x%X)", format, hrErr);

    if (SUCCEEDED(hr))
    {
        MessageBox(NULL, msg, L"Error", MB_ICONERROR);
    }
}


