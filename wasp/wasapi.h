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

#pragma once

#include "wave.h"

#include <mmdeviceapi.h>

typedef enum AudioState {
    AUDIOSTATE_IDLE = 0,
    AUDIOSTATE_PLAY = 1,
    AUDIOSTATE_PAUSE = 2,
    AUDIOSTATE_EXIT = 3,
    AUDIOSTATE_FORCE_DWORD = 0x7FFFFFFF  // TODO 
} AUDIOSTATE, * AUDIOSTATEPTR;

typedef struct Audio {
    HANDLE                  hThread;
    HANDLE                  hEvent; // TODO need to stop spinning when audio is complete

    WAVEPTR                 lpWave;
    AUDIOSTATE              asState;

    IMMDevice*              lpDevice;
    IAudioClient*           lpAudioClient;
    IAudioRenderClient*     lpAudioRenderer;
    UINT32                  nBufferSize;        // In Frames

    UINT32                  nCurrentFrame;
    UINT32                  nCurrentSample;
} AUDIO, * AUDIOPTR;

AUDIOPTR InitializeAudio();
BOOL PlayAudio(AUDIOPTR lpAudio, WAVEPTR lpWav);
VOID ResumeAudio(AUDIOPTR lpAudio);
VOID PauseAudio(AUDIOPTR lpAudio);
VOID StopAudio(AUDIOPTR lpAudio);
VOID ReleaseAudio(AUDIOPTR lpAudio);