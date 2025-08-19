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

#include "mem.h"
#include "wasapi.h"
#include "wasp.h"

#define WASP_WINDOW_NAME    "WASP"

HWND WND;
HWND Button;
HWND TrackBar;
HWND StatusBar;

AUDIOPTR Audio;

BOOL OpenWavFile(HWND hWnd) {
    OPENFILENAMEA ofn;
    CHAR szFile[MAX_PATH];
    ZeroMemory(szFile, MAX_PATH);
    ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "Wave\0*.WAV\0All\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn))
    {
        if (Audio->lpWave != NULL) {
            if (strcmp(Audio->lpWave->szPath, ofn.lpstrFile) != 0) {
                StopAudio(Audio);
            }
        }

        WAVEPTR lpWav = OpenWave(ofn.lpstrFile);

        if (lpWav != NULL) {
            PlayAudio(Audio, lpWav);
            return TRUE;
        }
    }

    return FALSE;
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
            OpenWavFile(hWnd);
            break;
        case ID_FILE_EXIT:
            DestroyWindow(hWnd);
            break;
        case ID_HELP_ABOUT:
            break;
        default:
            if (Button == (HWND)lParam) {
                if (Audio->lpWave == NULL) {
                    OpenWavFile(hWnd);
                }
                else if (Audio->asState == AUDIOSTATE_IDLE
                    || Audio->asState == AUDIOSTATE_PAUSE) { /* TODO */
                    ResumeAudio(Audio);
                }
                else { PauseAudio(Audio); }

                //Audio->asState =
                //    (Audio->asState == AUDIOSTATE_IDLE || Audio->asState == AUDIOSTATE_PAUSE)
                //    ? AUDIOSTATE_PLAY : AUDIOSTATE_PAUSE;
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
    wcls.lpszClassName = WASP_WINDOW_NAME;

    const ATOM atom = RegisterClassA(&wcls);

    if (!atom) { return NULL; }

    return CreateWindowExA(WS_EX_ACCEPTFILES,
        WASP_WINDOW_NAME, WASP_WINDOW_NAME,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 480, 160, NULL, NULL, hInstance, NULL);
}

HWND CreateWaspTrackBar(HINSTANCE hInstance, HWND hWnd, int x, int y, int width, int height) {
    // https://learn.microsoft.com/en-us/windows/win32/controls/trackbar-control-styles

    return CreateWindowExA(0, TRACKBAR_CLASS, "",
        WS_DISABLED | WS_TABSTOP | WS_CHILD | WS_VISIBLE,
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
    InitializeMemory();

    Audio = InitializeAudio();

    if (Audio == NULL) {
        MessageBoxA(NULL, "Can't initialize WASAPI!", "WASP", MB_ICONERROR | MB_OK);
        return EXIT_FAILURE;
    }

    InitCommonControls();

    WND = CreateWaspWindow(hInstance);

    if (WND == NULL) {
        MessageBoxA(NULL, "Can't create WASP window!", "WASP", MB_ICONERROR | MB_OK);
        return EXIT_FAILURE;
    }

    Button = CreateWaspButton(hInstance, WND, "", 0, 0, 75, 75);
    TrackBar = CreateWaspTrackBar(hInstance, WND, 75, 25, 380, 40);

    //SendMessageA(TrackBar, TBM_SETRANGE,
    //    (WPARAM)TRUE,                   // redraw flag 
    //    (LPARAM)MAKELONG(7, 170));  // min. & max. positions

    //SendMessageA(TrackBar, TBM_SETPAGESIZE,
    //    0, (LPARAM)4);                  // new page size 

    //SendMessageA(TrackBar, TBM_SETSEL,
    //    (WPARAM)FALSE,                  // redraw flag 
    //    (LPARAM)MAKELONG(7, 170));

    //SendMessageA(TrackBar, TBM_SETPOS,
    //    (WPARAM)TRUE,                   // redraw flag 
    //    (LPARAM)7);

    StatusBar = CreateStatusWindowA(WS_CHILD | WS_VISIBLE, "00:00:00 / 00:00:00", WND, 1 /* TODO*/);

    UpdateWindow(WND);
    ShowWindow(WND, nShowCmd);

    BOOL active = TRUE;
    MSG msg;

    while (active) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) { active = FALSE; }

            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        if (!active) {
            ReleaseAudio(Audio);
            break;
        }

        if (Audio != NULL && Audio->lpWave != NULL) {
            //TODO
            //TODO MAke pretty
            // TODO make so it doesn't flicker
            CHAR message[128];

            CONST UINT32 elapsed =
                Audio->nCurrentSample / (Audio->lpWave->wfxFormat.nChannels * Audio->lpWave->wfxFormat.nSamplesPerSec);
            CONST UINT32 total =
                Audio->lpWave->dwNumSamples / (Audio->lpWave->wfxFormat.nChannels * Audio->lpWave->wfxFormat.nSamplesPerSec);

            StringCchPrintfA(message, 128, "%02d:%02d:%02d / %02d:%02d:%02d",
                elapsed / (60 * 60), (elapsed / 60) % 60, elapsed % 60,
                total / (60 * 60), (total / 60) % 60, total % 60);

            SendMessageA(StatusBar, SB_SETTEXT, (WPARAM)0, (LPARAM)message);
        }

        Sleep(1);
    }

    CoUninitialize();

    return EXIT_SUCCESS;
}
