#ifndef TEST_MMSYSTEM_H
#define TEST_MMSYSTEM_H
#include <windows.h>
typedef void *HWAVEOUT;
typedef UINT MMRESULT;
typedef struct { WORD wFormatTag, nChannels; DWORD nSamplesPerSec, nAvgBytesPerSec;
    WORD nBlockAlign, wBitsPerSample, cbSize; } WAVEFORMATEX;
typedef struct { LPSTR lpData; DWORD dwBufferLength, dwBytesRecorded; DWORD_PTR dwUser;
    DWORD dwFlags, dwLoops; void *lpNext; DWORD_PTR reserved; } WAVEHDR;
#define WAVE_FORMAT_PCM 1
#define WAVE_MAPPER ((UINT)-1)
#define CALLBACK_NULL 0
#define MMSYSERR_NOERROR 0
#define WHDR_DONE 1
MMRESULT waveOutOpen(HWAVEOUT *, UINT, const WAVEFORMATEX *, DWORD_PTR, DWORD_PTR, DWORD);
MMRESULT waveOutReset(HWAVEOUT);
MMRESULT waveOutClose(HWAVEOUT);
MMRESULT waveOutPrepareHeader(HWAVEOUT, WAVEHDR *, UINT);
MMRESULT waveOutUnprepareHeader(HWAVEOUT, WAVEHDR *, UINT);
MMRESULT waveOutWrite(HWAVEOUT, WAVEHDR *, UINT);
#endif
