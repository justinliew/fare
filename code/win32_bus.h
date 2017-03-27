#ifndef __WIN32_NETRUNNER_H
#define __WIN32_NETRUNNER_H

struct win32_offscreen_buffer
{
    // NOTE(casey): Pixels are alwasy 32-bits wide, Memory Order BB GG RR XX
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

struct win32_sound_output
{
    int SamplesPerSecond;
    uint32 RunningSampleIndex;
    int WavePeriod;
    DWORD BytesPerSample;
    DWORD SecondaryBufferSize;
    real32 tSine;
    DWORD SafetyBytes;
    // TODO - should RunningSampleIndex be in bytes?
    // TODO - math gets easier if we add a "bytes per second" field?
};

struct win32_debug_time_marker
{
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputLocation;
    DWORD OutputByteCount;

    DWORD ExpectedFlipPlayCursor;
    DWORD FlipPlayCursor;
    DWORD FlipWriteCursor;
};

struct win32_game_code
{
    HMODULE GameCodeDLL;
    FILETIME DLLLastWriteTime;
    game_update_and_render* UpdateAndRender;
    game_get_sound_samples* GetSoundSamples;
    bool32 IsValid;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_replay_buffer
{
    HANDLE FileHandle;
    HANDLE MemoryMap;
    char ReplayFileName[WIN32_STATE_FILE_NAME_COUNT];
    void* MemoryBlock;
};

struct win32_state
{
    uint64 TotalSize;
    void* GameMemoryBlock;
    win32_replay_buffer ReplayBuffers[4];
    
    HANDLE RecordingHandle;
    int InputRecordingIndex;

    HANDLE PlaybackHandle;
    int InputPlayingIndex;

    char ExeFileName[WIN32_STATE_FILE_NAME_COUNT];
    char* OnePastLastEXEFileNameSlash;

};

#endif