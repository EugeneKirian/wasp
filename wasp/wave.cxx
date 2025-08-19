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

#include "mem.h"
#include "wave.h"

#include <aviriff.h>

#define MIN_WAVE_FILE_SIZE  38

BOOL IsWaveFile(RIFFLIST* lpHeader) {
    return lpHeader->fcc == FCC('RIFF') && lpHeader->fccListType == FCC('WAVE');
}

WAVEPTR OpenWave(LPCSTR lpszPath) {
    HANDLE hFile = CreateFileA(lpszPath, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    DWORD dwFileSize = GetFileSize(hFile, NULL);

    if (dwFileSize < MIN_WAVE_FILE_SIZE) {
        CloseHandle(hFile);
        return NULL;
    }

    DWORD read = 0;
    BYTE bytes[MIN_WAVE_FILE_SIZE];
    if (!ReadFile(hFile, bytes, MIN_WAVE_FILE_SIZE, &read, NULL)
        || read != MIN_WAVE_FILE_SIZE || !IsWaveFile((RIFFLIST*)bytes)) {
        CloseHandle(hFile);
        return NULL;
    }

    WAVEPTR wav = (WAVEPTR)AllocateMemory(sizeof(WAVE));

    if (wav == NULL) {
        CloseHandle(hFile);
        return NULL;
    }

    ZeroMemory(wav, sizeof(WAVE));

    strcpy(wav->szPath, lpszPath);

    LPVOID data = AllocateMemory(dwFileSize);

    if (data == NULL) {
        FreeMemory(wav);
        CloseHandle(hFile);
        return NULL;
    }

    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);

    if (!ReadFile(hFile, data, dwFileSize, &read, NULL) || read != dwFileSize) {
        FreeMemory(wav);
        FreeMemory(data);
        CloseHandle(hFile);
        return NULL;
    }

    CloseHandle(hFile);

    BOOL found = FALSE;
    LPVOID end = (LPVOID)((size_t)data + dwFileSize);

    for (RIFFCHUNK* chunk = (RIFFCHUNK*)((size_t)data + sizeof(RIFFLIST));
        chunk < end; chunk = RIFFNEXT(chunk)) {

        // Search for format chunk. It must be present in a valid WAV file.
        if (chunk->fcc == FCC('fmt ')) {
            LPWAVEFORMATEX fmt = (LPWAVEFORMATEX)((size_t)chunk + sizeof(RIFFCHUNK));

            if (fmt->wFormatTag != WAVE_FORMAT_PCM) { break; }

            wav->wfxFormat.wFormatTag = fmt->wFormatTag;
            wav->wfxFormat.nChannels = fmt->nChannels;
            wav->wfxFormat.nSamplesPerSec = fmt->nSamplesPerSec;
            wav->wfxFormat.nAvgBytesPerSec = fmt->nAvgBytesPerSec;
            wav->wfxFormat.nBlockAlign = fmt->nBlockAlign;
            wav->wfxFormat.wBitsPerSample = fmt->wBitsPerSample;

            found = TRUE;
        }
        // Search for data chunk. It must be present in a valid WAV file.
        else if (chunk->fcc == FCC('data')) {

            // Ensure that the format chunk preceeded the data chunk in the file.
            if (!found) { break; }

            // Ensure that the file contains at least the same amount of data
            // as specified in the data chunk size.
            if (((size_t)end - (size_t)chunk - sizeof(RIFFCHUNK)) < chunk->cb) { break; }

            wav->dwNumFrames = chunk->cb / wav->wfxFormat.nBlockAlign;
            wav->dwNumSamples = chunk->cb / (wav->wfxFormat.wBitsPerSample >> 3);

            wav->lpSamples = AllocateMemory(chunk->cb);

            if (wav->lpSamples != NULL) {
                CopyMemory(wav->lpSamples,
                    (LPVOID)((size_t)chunk + sizeof(RIFFCHUNK)), chunk->cb);

                FreeMemory(data);

                return wav;
            }
        }
    }

    FreeMemory(wav);
    FreeMemory(data);

    return NULL;
}

VOID ReleaseWave(WAVEPTR lpWav) {
    if (lpWav != NULL) {
        if (lpWav->lpSamples != NULL) {
            FreeMemory(lpWav->lpSamples);
        }

        FreeMemory(lpWav);
    }
}