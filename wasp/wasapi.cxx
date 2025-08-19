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

#include "mem.hxx"
#include "wasapi.hxx"
#include "wave.hxx"

#define TARGET_BUFFER_PADDING_IN_SECONDS  (1.0f / 60.0f)

#define SAFERELEASE(x) { if (x) { x->Release(); x = NULL; } }

DWORD WINAPI AudioMain(LPVOID lpThreadParameter) {
    AUDIOPTR audio = (AUDIOPTR)lpThreadParameter;

    CONST UINT32 target =
        (UINT32)(audio->nBufferSize * TARGET_BUFFER_PADDING_IN_SECONDS);

    while (audio->dwState != AUDIOSTATE_EXIT) {
        if (audio->dwState == AUDIOSTATE_PLAY) {
            UINT32 padding = 0;

            if (SUCCEEDED(audio->lpAudioClient->GetCurrentPadding(&padding))) {
                WAVEPTR wav = audio->lpWave;

                CONST UINT32 frames =
                    min(target - padding, wav->dwNumFrames - audio->nCurrentFrame);

                if (frames != 0) {
                    BYTE* lock;
                    if (SUCCEEDED(audio->lpAudioRenderer->GetBuffer(frames, &lock))) {
                        LPWAVEFORMATEX format = &wav->wfxFormat;
                        CONST size_t offset = audio->nCurrentFrame * format->nBlockAlign;
                        CONST LPVOID source = (LPVOID)((size_t)wav->lpSamples + offset);

                        CopyMemory(lock, source, frames * format->nBlockAlign);

                        audio->nCurrentFrame += frames;
                        audio->nCurrentSample += frames * format->nChannels;

                        audio->lpAudioRenderer->ReleaseBuffer(frames, 0);

                        if (wav->dwNumFrames <= audio->nCurrentFrame) {
                            audio->dwState = AUDIOSTATE_IDLE;
                        }
                    }
                }
            }
        }

        Sleep(1);

        if (audio->dwState == AUDIOSTATE_IDLE || audio->dwState == AUDIOSTATE_PAUSE) {
            if (audio->dwState == AUDIOSTATE_IDLE) {
                audio->nCurrentFrame = 0;
                audio->nCurrentSample = 0;
            }

            WaitForSingleObject(audio->hSignal, INFINITE);
        }
    }

    audio->lpAudioClient->Stop();

    SAFERELEASE(audio->lpAudioClient);
    SAFERELEASE(audio->lpAudioRenderer);

    CloseHandle(audio->hSignal);

    return EXIT_SUCCESS;
}

AUDIOPTR InitializeAudio() {
    AUDIOPTR audio = (AUDIOPTR)AllocateMemory(sizeof(AUDIO));

    if (audio == NULL) { return NULL; }

    ZeroMemory(audio, sizeof(AUDIO));

    IMMDeviceEnumerator* enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),
        NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (LPVOID*)&enumerator))) {
        FreeMemory(audio);
        return NULL;
    }

    if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &audio->lpDevice))) {
        SAFERELEASE(enumerator);
        FreeMemory(audio);
        return NULL;
    }

    enumerator->Release();

    return audio;
}

BOOL PlayAudio(AUDIOPTR lpAudio, WAVEPTR lpWav) {
    if (lpAudio == NULL || lpWav == NULL) { return FALSE; }

    // Stop current playback, if any, and release audio resources,
    // so that they can be recreated to match new audio format.
    {
        StopAudio(lpAudio);

        if (lpAudio->lpAudioClient != NULL) {
            lpAudio->lpAudioClient->Stop();
            lpAudio->lpAudioClient->Reset();

            SAFERELEASE(lpAudio->lpAudioRenderer);
            SAFERELEASE(lpAudio->lpAudioClient);
        }

        if (lpAudio->lpWave != NULL) {
            ReleaseWave(lpAudio->lpWave);
            lpAudio->lpWave = NULL;
        }
    }

    // Activate new audio client.
    if (FAILED(lpAudio->lpDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (LPVOID*)&lpAudio->lpAudioClient))) {
        return FALSE;
    }

    if (FAILED(lpAudio->lpAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_RATEADJUST | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
        20000000, 0, &lpWav->wfxFormat, &GUID_NULL))) {
        SAFERELEASE(lpAudio->lpAudioClient);
        return FALSE;
    }

    if (FAILED(lpAudio->lpAudioClient->GetService(__uuidof(IAudioRenderClient), (LPVOID*)&lpAudio->lpAudioRenderer))) {
        SAFERELEASE(lpAudio->lpAudioClient);
        return FALSE;
    }

    if (FAILED(lpAudio->lpAudioClient->GetBufferSize(&lpAudio->nBufferSize))) {
        SAFERELEASE(lpAudio->lpAudioRenderer);
        SAFERELEASE(lpAudio->lpAudioClient);
        return FALSE;
    }

    if (FAILED(lpAudio->lpAudioClient->Start())) {
        SAFERELEASE(lpAudio->lpAudioRenderer);
        SAFERELEASE(lpAudio->lpAudioClient);
        return FALSE;
    }

    lpAudio->hSignal = CreateEventA(NULL, TRUE, FALSE, NULL);

    if (lpAudio->hSignal == NULL) {
        SAFERELEASE(lpAudio->lpAudioRenderer);
        SAFERELEASE(lpAudio->lpAudioClient);
        return FALSE;
    }

    lpAudio->dwState = AUDIOSTATE_PLAY;
    lpAudio->hThread = CreateThread(NULL, 0, AudioMain, lpAudio, 0, NULL);

    if (lpAudio->hThread == NULL) {
        CloseHandle(lpAudio->hSignal);
        SAFERELEASE(lpAudio->lpAudioRenderer);
        SAFERELEASE(lpAudio->lpAudioClient);
        return FALSE;
    }

    lpAudio->lpWave = lpWav;

    return TRUE;
}

VOID ResumeAudio(AUDIOPTR lpAudio) {
    if (lpAudio == NULL) { return; }
    if (lpAudio->dwState == AUDIOSTATE_EXIT) { return; }

    if (lpAudio->dwState != AUDIOSTATE_PLAY) {
        lpAudio->dwState = AUDIOSTATE_PLAY;
        SetEvent(lpAudio->hSignal);
    }
}

VOID PauseAudio(AUDIOPTR lpAudio) {
    if (lpAudio == NULL) { return; }
    if (lpAudio->dwState == AUDIOSTATE_EXIT) { return; }

    if (lpAudio->dwState != AUDIOSTATE_PAUSE) {
        lpAudio->dwState = AUDIOSTATE_PAUSE;
    }
}

VOID StopAudio(AUDIOPTR lpAudio) {
    if (lpAudio == NULL) { return; }
    if (lpAudio->dwState == AUDIOSTATE_EXIT) { return; }

    // Set state to Idle and rewind playback position to 0.
    if (lpAudio->dwState != AUDIOSTATE_IDLE) {
        lpAudio->dwState = AUDIOSTATE_IDLE;
        lpAudio->nCurrentFrame = 0;
        lpAudio->nCurrentSample = 0;
    }
}

VOID ReleaseAudio(AUDIOPTR lpAudio) {
    if (lpAudio == NULL) { return; }
    if (lpAudio->dwState == AUDIOSTATE_EXIT) { return; }

    StopAudio(lpAudio);

    lpAudio->dwState = AUDIOSTATE_EXIT;
    SetEvent(lpAudio->hSignal);

    DWORD exit = EXIT_SUCCESS;
    while (GetExitCodeThread(lpAudio->hThread, &exit)) {
        if (exit != STILL_ACTIVE) { break; }

        Sleep(1);
    }

    SAFERELEASE(lpAudio->lpDevice);
    ReleaseWave(lpAudio->lpWave);
    FreeMemory(lpAudio);
}

BOOL IsAudioIdle(AUDIOPTR lpAudio) {
    if (lpAudio == NULL) { return FALSE; }

    return IsAudioPresent(lpAudio) && lpAudio->dwState == AUDIOSTATE_IDLE;
}

BOOL IsAudioPlaying(AUDIOPTR lpAudio) {
    if (lpAudio == NULL) { return FALSE; }

    return IsAudioPresent(lpAudio) && lpAudio->dwState == AUDIOSTATE_PLAY;
}

BOOL IsAudioPaused(AUDIOPTR lpAudio) {
    if (lpAudio == NULL) { return FALSE; }

    return IsAudioPresent(lpAudio) && lpAudio->dwState == AUDIOSTATE_PAUSE;
}

BOOL IsAudioPresent(AUDIOPTR lpAudio) {
    if (lpAudio == NULL) { return FALSE; }
    if (lpAudio->dwState == AUDIOSTATE_EXIT) { return FALSE; }

    return lpAudio->lpWave != NULL;
}