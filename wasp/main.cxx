/*
Copyright (c) 2025 Eugene Kirian

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <windows.h>
#include <commctrl.h>
#include <strsafe.h>

#include "mem.hxx"
#include "wasapi.hxx"
#include "wasp.hxx"

#define WINDOW_NAME                 "WASP"
#define STATUS_BAR_ID               0

#define MAX_STATUS_BAR_TEXT_LENGTH  128
#define DEFAULT_STATUS_BAR_TEXT     "00:00:00 / 00:00:00"

HWND WND;
HWND Button;

HWND TrackBar;
DWORD TrackBarCurrent;
DWORD TrackBarMax;

HWND StatusBar;
CHAR StatusBarText[MAX_STATUS_BAR_TEXT_LENGTH] = DEFAULT_STATUS_BAR_TEXT;

AUDIOPTR Audio;

VOID UpdateStatusBar() {
    CHAR text[MAX_STATUS_BAR_TEXT_LENGTH];

    CONST UINT32 elapsed =
        Audio->nCurrentSample / (Audio->lpWave->wfxFormat.nChannels * Audio->lpWave->wfxFormat.nSamplesPerSec);
    CONST UINT32 total =
        Audio->lpWave->dwNumSamples / (Audio->lpWave->wfxFormat.nChannels * Audio->lpWave->wfxFormat.nSamplesPerSec);

    StringCchPrintfA(text, 128, "%02d:%02d:%02d / %02d:%02d:%02d",
        elapsed / (60 * 60), (elapsed / 60) % 60, elapsed % 60,
        total / (60 * 60), (total / 60) % 60, total % 60);

    if (strcmp(StatusBarText, text) != 0) {
        strcpy(StatusBarText, text);
        SendMessageA(StatusBar, SB_SETTEXT, (WPARAM)0, (LPARAM)StatusBarText);
    }
}

VOID UpdateTrackBar() {
    UINT32 elapsed = 0;
    UINT32 total = 0;

    if (IsAudioPresent(Audio)) {
        elapsed =
            Audio->nCurrentSample / (Audio->lpWave->wfxFormat.nChannels * Audio->lpWave->wfxFormat.nSamplesPerSec);
        total =
            Audio->lpWave->dwNumSamples / (Audio->lpWave->wfxFormat.nChannels * Audio->lpWave->wfxFormat.nSamplesPerSec);
    }

    if (TrackBarMax != total) {
        TrackBarMax = total;
        SendMessageA(TrackBar, TBM_SETRANGEMAX, TRUE, TrackBarMax);
    }

    if (TrackBarCurrent != elapsed) {
        TrackBarCurrent = elapsed;
        SendMessageA(TrackBar, TBM_SETPOS, TRUE, TrackBarCurrent);
    }
}

BOOL ActivatePlayback(WAVEPTR lpWav) {
    if (PlayAudio(Audio, lpWav)) {
        EnableWindow(TrackBar, TRUE);
        UpdateTrackBar();
        return TRUE;
    }

    EnableWindow(TrackBar, FALSE);
    ReleaseWave(lpWav);

    return FALSE;
}

VOID ResumePlayback() {
    ResumeAudio(Audio);

    strcpy(StatusBarText, DEFAULT_STATUS_BAR_TEXT);
    SendMessageA(StatusBar, SB_SETTEXT, (WPARAM)0, (LPARAM)StatusBarText);

    EnableWindow(TrackBar, TRUE);
    UpdateTrackBar();
}

VOID DisablePlayback() {
    StopAudio(Audio);

    strcpy(StatusBarText, DEFAULT_STATUS_BAR_TEXT);
    SendMessageA(StatusBar, SB_SETTEXT, (WPARAM)0, (LPARAM)StatusBarText);

    EnableWindow(TrackBar, FALSE);
    UpdateTrackBar();
}

VOID OpenFile(LPCSTR lpszPath) {
    if (lpszPath == NULL) { return; }

    // If file was selected, check if it is different from the current file, if any.
    if (IsAudioPresent(Audio)) {
        // Check if the selected file is the same as currently open file.
        if (strcmp(Audio->lpWave->szPath, lpszPath) == 0) {
            ResumePlayback();
            return;
        }

        // Stop playback in case the new file is different from the current file.
        DisablePlayback();
    }

    // Attempt to open a wav file.
    WAVEPTR wav = OpenWave(lpszPath);
    if (wav != NULL) {
        // If the selected file is a valid wav file - play it immediately.
        ActivatePlayback(wav);
    }
}

VOID OpenFileDialog() {
    CHAR szFile[MAX_PATH];
    ZeroMemory(szFile, MAX_PATH);

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAMEA));

    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = WND;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "Wave\0*.WAV\0All\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        OpenFile(ofn.lpstrFile);
    }
}

VOID HandleButtonClick() {
    // If audio is already present, then switch between play/pause.
    // In case audio ran to the end - resume audio from the start.
    if (IsAudioPresent(Audio)) {
        if (IsAudioPlaying(Audio)) {
            PauseAudio(Audio);
        }
        else if (IsAudioPaused(Audio) || IsAudioIdle(Audio)) {
            ResumeAudio(Audio);
        }

        return;
    }
    
    // In case when audio is not present - offer user to open a wav file.
    OpenFileDialog();
}

LRESULT WINAPI WaspWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_FILE_OPEN:
            OpenFileDialog();
            break;
        case ID_FILE_EXIT:
            DestroyWindow(hWnd);
            break;
        case ID_HELP_ABOUT:
            // TODO
            break;
        default:
            if (Button == (HWND)lParam) {
                HandleButtonClick();
            }
        }
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

HWND CreateWaspWindow(HINSTANCE hInstance) {
    WNDCLASSA wcls;
    ZeroMemory(&wcls, sizeof(WNDCLASSA));

    wcls.style = CS_SAVEBITS | CS_DBLCLKS;
    wcls.lpfnWndProc = WaspWndProc;
    wcls.hInstance = hInstance;
    wcls.hIcon = LoadIconA(hInstance, (LPCSTR)IDI_ICON1);
    wcls.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wcls.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wcls.lpszMenuName = (LPCSTR)MAKEINTRESOURCE(IDR_MENU1);
    wcls.lpszClassName = WINDOW_NAME;

    const ATOM atom = RegisterClassA(&wcls);

    if (!atom) { return NULL; }

    return CreateWindowExA(WS_EX_ACCEPTFILES,
        WINDOW_NAME, WINDOW_NAME,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 480, 160, NULL, NULL, hInstance, NULL);
}

HWND CreateWaspTrackBar(HINSTANCE hInstance, HWND hWnd, int x, int y, int width, int height) {
    // https://learn.microsoft.com/en-us/windows/win32/controls/trackbar-control-styles

    return CreateWindowExA(0, TRACKBAR_CLASS, "",
        WS_DISABLED | WS_TABSTOP | WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
        x, y, width, height,
        hWnd, NULL, hInstance, NULL);
}

HWND CreateWaspButton(HINSTANCE hInstance, HWND hWnd, LPCSTR text, int x, int y, int width, int height) {
    HWND button = CreateWindowExA(0, "BUTTON", text,
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_ICON | BS_DEFPUSHBUTTON,
        x, y, width, height, hWnd, NULL, hInstance, NULL);

    SendMessageA(button, BM_SETIMAGE, (WPARAM)IMAGE_ICON,
        (LPARAM)LoadIconA(hInstance, (LPCSTR)IDI_ICON2));

    return button;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    // Initialize.
    if (FAILED(CoInitializeEx(NULL, COINIT_SPEED_OVER_MEMORY))) {
        MessageBoxA(NULL, "Can't initialize COM!", WINDOW_NAME, MB_ICONERROR | MB_OK);
        return EXIT_FAILURE;
    }

    InitializeMemory();
    Audio = InitializeAudio();

    if (Audio == NULL) {
        MessageBoxA(NULL, "Can't initialize WASAPI!", WINDOW_NAME, MB_ICONERROR | MB_OK);
        CoUninitialize();
        return EXIT_FAILURE;
    }

    // Initialize main window and controls.
    InitCommonControls();

    WND = CreateWaspWindow(hInstance);

    if (WND == NULL) {
        MessageBoxA(NULL, "Can't create WASP window!", WINDOW_NAME, MB_ICONERROR | MB_OK);
        CoUninitialize();
        return EXIT_FAILURE;
    }

    Button = CreateWaspButton(hInstance, WND, "", 0, 0, 75, 75);
    TrackBar = CreateWaspTrackBar(hInstance, WND, 75, 25, 380, 40);
    StatusBar = CreateStatusWindowA(WS_CHILD | WS_VISIBLE, StatusBarText, WND, STATUS_BAR_ID);

    UpdateWindow(WND);
    ShowWindow(WND, nShowCmd);

    // Handle command line arguments as if it is a file path to a wave file.
    if (*lpCmdLine != NULL) {
        OpenFile(lpCmdLine);
    }

    // Main window event loop.
    BOOL active = TRUE;
    while (active) {
        MSG msg;
        // Process all incoming window messages.
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { active = FALSE; }

            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        // Update user interface to match current playback state.
        if (active) {
            if (IsAudioPresent(Audio)) {
                UpdateStatusBar();
                UpdateTrackBar();
            }

            Sleep(1);

            continue;
        }

        // Release audio resources properly before shutting down the app.
        ReleaseAudio(Audio);
    }

    CoUninitialize();

    return EXIT_SUCCESS;
}
