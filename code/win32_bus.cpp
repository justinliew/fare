#define USING_SFML

#ifdef USING_SFML
#include <SFML/Graphics.hpp>
#endif

#include "bus.h"


#include <windows.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_bus.h"

// TODO(casey): This is a global for now.
#ifndef USING_SFML
global_variable bool32 GlobalRunning;
global_variable bool32 GlobalPause;

global_variable win32_offscreen_buffer GlobalBackbuffer;
#endif

global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

global_variable int64 GlobalPerfCountFrequency;

// NOTE(casey): XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(casey): XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void 
CatStrings( size_t SourceACount, char* SourceA,
            size_t SourceBCount, char* SourceB,
            size_t DestCount, char* Dest)
{
    for (int Index = 0; Index < SourceACount; ++Index)
    {
        *Dest++ = *SourceA++;
    }
    for (int Index = 0; Index < SourceBCount; ++Index)
    {
        *Dest++ = *SourceB++;
    }
    *Dest++ = 0;
}

internal void
Win32GetEXEFileName(win32_state* State)
{
    // NOTE: WIN32_STATE_FILE_NAME_COUNT is dangerous, don't use it in production code.
    DWORD SizeOfFileName = GetModuleFileNameA(0, State->ExeFileName, sizeof(State->ExeFileName));
    State->OnePastLastEXEFileNameSlash = State->ExeFileName;
    for (char *Scan = State->ExeFileName;
        *Scan; ++Scan)
    {
        if (*Scan == '\\')
        {
            State->OnePastLastEXEFileNameSlash = Scan+1;
        }
    }
}

internal int
StringLength(char* Str)
{
    int Count = 0;
    while(*Str++)
    {
        ++Count;
    }
    return Count;
}

internal void
Win32BuildEXEPathFileName(win32_state* State, char* FileName, 
                            int DestCount, char* Dest)
{
    CatStrings(State->OnePastLastEXEFileNameSlash - State->ExeFileName, State->ExeFileName, 
                StringLength(FileName), FileName,
                DestCount, Dest);
}

#if BUS_INTERNAL
DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    VirtualFree(Memory, 0, MEM_RELEASE);
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    debug_read_file_result Result = {};
    HANDLE FileHandle = CreateFileA(FileName, 
                                   GENERIC_READ,
                                   FILE_SHARE_READ,
                                   0,
                                   OPEN_EXISTING,
                                   0,
                                   0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if (GetFileSizeEx(FileHandle,&FileSize))
        {
            uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            if (Result.Contents)
            {
                DWORD BytesRead;
                if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) &&
                    (FileSize32 == BytesRead))
                {
                    Result.ContentsSize = BytesRead;
                    // success
                }
                else
                {
                    DEBUGPlatformFreeFileMemory(Thread, Result.Contents);
                    Result.Contents = nullptr;
                }
            }
            else
            {
                // TODO - logging
            }
        }
        CloseHandle(FileHandle);
    }
    return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    bool32 Result = false;
    HANDLE FileHandle = CreateFileA(FileName, 
                                   GENERIC_WRITE,
                                   FILE_SHARE_READ,
                                   0,
                                   CREATE_ALWAYS,
                                   0,
                                   0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
        {
            // file written successfully
            Result = (MemorySize == BytesWritten);
        }
        else
        {
            // TODO logging
        }
        CloseHandle(FileHandle);
    }
    return Result;
}

#endif

internal FILETIME 
Win32GetLastWriteTime(const char* FileName)
{
    FILETIME LastWriteTime = {};

    WIN32_FILE_ATTRIBUTE_DATA Data;
    if (GetFileAttributesEx(FileName, 
                            GetFileExInfoStandard,
                            &Data))
    {
        LastWriteTime = Data.ftLastWriteTime;
    }
    return LastWriteTime;
}

internal win32_game_code
Win32LoadGameCode(const char* SourceDLLName, const char* TempDLLName)
{
    win32_game_code Result = {};

    Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);

    // TODO - need to get proper path here
    // TODO - automatic determination of when updates are necessary

    CopyFile(SourceDLLName, TempDLLName, FALSE);
    Result.GameCodeDLL = LoadLibraryA("bus_temp.dll");
    if (Result.GameCodeDLL)
    {
        Result.UpdateAndRender = (game_update_and_render *)GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
        Result.GetSoundSamples = (game_get_sound_samples *)GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");

        Result.IsValid = Result.UpdateAndRender != nullptr && Result.GetSoundSamples != nullptr;
    }

    if (!Result.IsValid)
    {
        Result.UpdateAndRender = 0;
        Result.GetSoundSamples = 0;        
    }

    return Result;
}

internal void
Win32UnloadGameCode(win32_game_code* GameCode)
{
    if (GameCode->GameCodeDLL)
    {
        FreeLibrary(GameCode->GameCodeDLL);
        GameCode->GameCodeDLL = nullptr;
    }
    GameCode->IsValid = false;
    GameCode->UpdateAndRender = 0;
    GameCode->GetSoundSamples = 0;
}

internal void
Win32LoadXInput(void)
{
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if (!XInputLibrary)
    {
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    if (!XInputLibrary)
    {
        XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
    }
    if(XInputLibrary)
    {
        // TODO - diagnostic
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        if(!XInputGetState) {XInputGetState = XInputGetStateStub;}

        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
        if(!XInputSetState) {XInputSetState = XInputSetStateStub;}
        else
        {
            // TODO - diagnostic
        }
    }
    else
    {
        // TODO - diagnostic
    }
}

#ifndef USING_SFML
internal void
Win32InitDSound(HWND Window, int32 SamplesPerSec, int32 BufferSize)
{
    // Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

    if (DSoundLibrary)
    {
        // get a DirectSound object
        direct_sound_create* DirectSoundCreate = (direct_sound_create *)
            GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        // TODO - check this works on XP - Directsound 8 or 7?
        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSec;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {};

                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

                // Create a primary buffer
                // TODO - DSBCAPS_GLOBALFOCUS?
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    // this fails right now
                    if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
                    {
                        // we have finally set the format
                    }
                    else
                    {
                        // TODO - diagnostic
                        OutputDebugString("FAIL");
                    }
                }
            }
            else
            {
                // TODO - diagnostic
                OutputDebugString("FAIL");
            }

            // create a secondary buffer
            // TODO - DSBCAPS_GETCURRENTPOSITION2
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;
            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
            {
            }
        }
        else
        {
            // TODO - diagnostic
        }
    }
    else
    {
        // TODO - diagnostic
    }
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return(Result);
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    // TODO(casey): Bulletproof this.
    // Maybe dont free first, free after, then free first if that fails.

    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;

    int BytesPerPixel = 4;

    // NOTE(casey): When the biHeight field is negative, this is the clue to
    // Windows to treat this bitmap as top-down, not bottom-up, meaning that
    // the first three bytes of the image are the color for the top left pixel
    // in the bitmap, not the bottom left!
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    // NOTE(casey): Thank you to Chris Hecker of Spy Party fame
    // for clarifying the deal with StretchDIBits and BitBlt!
    // No more DC for us.
    int BitmapMemorySize = (Buffer->Width*Buffer->Height)*BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width*BytesPerPixel;
    Buffer->BytesPerPixel = BytesPerPixel;

    // TODO(casey): Probably clear this to black
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
                           HDC DeviceContext, int WindowWidth, int WindowHeight)
{
    int OffsetX = 10;
    int OffsetY = 10;

    PatBlt(DeviceContext, 0, 0, WindowWidth, OffsetY, BLACKNESS);
    PatBlt(DeviceContext, 0, OffsetY + Buffer->Height, WindowWidth, WindowHeight, BLACKNESS);
    PatBlt(DeviceContext, 0, 0, OffsetX, WindowHeight, BLACKNESS);
    PatBlt(DeviceContext, OffsetX + Buffer->Width, 0, WindowWidth, WindowHeight, BLACKNESS);


    // for prototyping purposes, we are blitting 1-1 to ensure we don't introduce stretching artifacts
    StretchDIBits(DeviceContext,
                  /*
                  X, Y, Width, Height,
                  X, Y, Width, Height,
                  */
                  OffsetX, OffsetY, Buffer->Width, Buffer->Height,
                  0, 0, Buffer->Width, Buffer->Height,
                  Buffer->Memory,
                  &Buffer->Info,
                  DIB_RGB_COLORS, SRCCOPY);
}

internal LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
                        UINT Message,
                        WPARAM WParam,
                        LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_CLOSE:
        {
            // TODO(casey): Handle this with a message to the user?
            GlobalRunning = false;
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;

        case WM_DESTROY:
        {
            // TODO(casey): Handle this as an error - recreate window?
            GlobalRunning = false;
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Assert("Keyboard input came in through a non-dispatch method.")
        } break;
        
        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext,
                                       Dimension.Width, Dimension.Height);
            EndPaint(Window, &Paint);
        } break;

        default:
        {
//            OutputDebugStringA("default\n");
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        } break;
    }
    
    return(Result);
}

internal void Win32ClearBuffer(win32_sound_output* SoundOutput)
{
    VOID* Region1 = nullptr;
    DWORD Region1Size = 0;
    VOID* Region2 = nullptr;
    DWORD Region2Size = 0;

    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
                                &Region1, &Region1Size,
                                &Region2, &Region2Size,
                                0)))
    {
        // TODO(casey): Collapse these two loops
        int8* DestSample = (int8 *)Region1;
        for(DWORD ByteIndex = 0;
            ByteIndex < Region1Size;
            ++ByteIndex)
        {
            *DestSample++ = 0;
        }

        DestSample = (int8 *)Region2;
        for(DWORD ByteIndex = 0;
            ByteIndex < Region2Size;
            ++ByteIndex)
        {
            *DestSample++ = 0;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void Win32FillSoundBuffer(win32_sound_output* SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer* SourceBuffer)
{
    VOID* Region1 = nullptr;
    DWORD Region1Size = 0;
    VOID* Region2 = nullptr;
    DWORD Region2Size = 0;

    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
                                &Region1, &Region1Size,
                                &Region2, &Region2Size,
                                0)))
    {
        // TODO(casey): Collapse these two loops
        DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
        int16* DestSample = (int16 *)Region1;
        int16* SourceSample = SourceBuffer->Samples;
        for(DWORD SampleIndex = 0;
            SampleIndex < Region1SampleCount;
            ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            // TODO(casey): Draw this out for people
            ++SoundOutput->RunningSampleIndex;
        }

        DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
        DestSample = (int16 *)Region2;
        for(DWORD SampleIndex = 0;
            SampleIndex < Region2SampleCount;
            ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            // TODO(casey): Draw this out for people
            ++SoundOutput->RunningSampleIndex;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}
#endif

internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state* OldState, DWORD ButtonBit, game_button_state* NewState)
{
    NewState->EndedDown = (XInputButtonState & ButtonBit) == ButtonBit;
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal void Win32ProcessKeyboardMessage(game_button_state* NewState, bool32 IsDown)
{
    if (NewState->EndedDown != IsDown)
    {
        NewState->EndedDown = IsDown;
        ++NewState->HalfTransitionCount;
    }
}

internal real32 Win32ProcessXInputStickValue(SHORT ThumbValue, SHORT DeadZoneThreshold)
{
    real32 Result = 0;
    if (ThumbValue < -DeadZoneThreshold )
    {
        Result = (real32)(ThumbValue + DeadZoneThreshold) / (32768.f - DeadZoneThreshold);
    }
    else if (ThumbValue > DeadZoneThreshold )
    {
        Result = (real32)(ThumbValue - DeadZoneThreshold) / (32767.f - DeadZoneThreshold);
    }
    return Result;
}

internal void
Win32GetInputFileLocation(win32_state* State, bool InputStream, int SlotIndex, int DestCount, char* Dest)
{
    char Temp[64];
    wsprintf(Temp, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");    
    Win32BuildEXEPathFileName(State, Temp, DestCount, Dest);
}

internal win32_replay_buffer*
Win32GetReplayBuffer(win32_state* State, unsigned int Index)
{
    Assert(Index < ArrayCount(State->ReplayBuffers));
    win32_replay_buffer* Result = &State->ReplayBuffers[Index];
    return Result;
}

internal void
Win32BeginRecordingInput(win32_state* State, int InputRecordingIndex)
{
#if 0
	win32_replay_buffer* ReplayBuffer = Win32GetReplayBuffer(State, InputRecordingIndex);
    if (ReplayBuffer->MemoryBlock)
    {
        State->InputRecordingIndex = InputRecordingIndex;

        char FileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(State, true, InputRecordingIndex, sizeof(FileName), FileName);
        State->RecordingHandle = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

#if 0
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = State->TotalSize;
        SetFilePointerEx(State->RecordingHandle, FilePosition, 0, FILE_BEGIN);
#endif
        CopyMemory(ReplayBuffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
    }
#endif
}

internal void
Win32EndRecordingInput(win32_state* State)
{
    CloseHandle(State->RecordingHandle);
    State->InputRecordingIndex = 0;
}

internal void
Win32BeginInputPlayback(win32_state* State, int InputPlayingIndex)
{
    win32_replay_buffer* ReplayBuffer = Win32GetReplayBuffer(State, InputPlayingIndex);
    if (ReplayBuffer->MemoryBlock)
    {
        State->InputPlayingIndex = InputPlayingIndex;

        char FileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(State, true, InputPlayingIndex, sizeof(FileName), FileName);

        State->PlaybackHandle = CreateFileA(FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

#if 0
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = State->TotalSize;
        SetFilePointerEx(State->PlaybackHandle, FilePosition, 0, FILE_BEGIN);
#endif
        CopyMemory(State->GameMemoryBlock, ReplayBuffer->MemoryBlock, State->TotalSize);
    }
}

internal void
Win32EndInputPlayback(win32_state* State)
{
    CloseHandle(State->PlaybackHandle);
    State->InputPlayingIndex = 0;
}

internal void
Win32RecordInput(win32_state* State, game_input* Input )
{
    DWORD BytesWritten;
    WriteFile(State->RecordingHandle, Input, sizeof(*Input), &BytesWritten, 0);
}

internal void
Win32PlaybackInput(win32_state* State, game_input* Input )
{
    DWORD BytesRead;
	if (ReadFile(State->PlaybackHandle, Input, sizeof(*Input), &BytesRead, 0))
    {
    	if (BytesRead == 0)
        {
            // loop
            int PlayingIndex = State->InputPlayingIndex;
            Win32EndInputPlayback(State);
            Win32BeginInputPlayback(State, PlayingIndex);
            ReadFile(State->PlaybackHandle, Input, sizeof(*Input), &BytesRead, 0);
        }
    }
}

#ifndef USING_SFML
internal void
Win32ProcessPendingMessages(game_controller_input *KeyboardController, win32_state* Win32State)
{
    MSG Message;
    while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch(Message.message)
        {
            case WM_QUIT:
            {
                GlobalRunning = false;
            } break;
            
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32 VKCode = (uint32)Message.wParam;
                bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
                bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);
                if(WasDown != IsDown)
                {
                    if(VKCode == 'W')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                    }
                    else if(VKCode == 'A')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                    }
                    else if(VKCode == 'S')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                    }
                    else if(VKCode == 'D')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                    }
                    else if(VKCode == 'Q')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                    }
                    else if(VKCode == 'E')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                    }
                    else if(VKCode == VK_UP)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                    }
                    else if(VKCode == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                    }
                    else if(VKCode == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                    }
                    else if(VKCode == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                    }
                    else if(VKCode == VK_ESCAPE)
                    {
                        GlobalRunning = false;
                        Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                    }
                    else if(VKCode == VK_SPACE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                    }
#if BUS_INTERNAL
                    else if(VKCode == 'P' && IsDown)
                    {
                        if (IsDown)
                        {
                            GlobalPause = !GlobalPause;
                        }
                    }
                    else if (VKCode == 'L')
                    {
                        // removing for now due to Dropbox storage
#if 0
                        if (IsDown)
                        {
                            if (Win32State->InputPlayingIndex == 0)
                            {
                                if (Win32State->InputRecordingIndex == 0)
                                {
                                    Win32BeginRecordingInput(Win32State, 1);
                                }
                                else
                                {
                                    Win32EndRecordingInput(Win32State);
                                    Win32BeginInputPlayback(Win32State, 1);
                                }
                            }
                            else
                            {
                                Win32EndInputPlayback(Win32State);
                            }
                       }
#endif
                    }

#endif
                }

                bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
                if((VKCode == VK_F4) && AltKeyWasDown)
                {
                    GlobalRunning = false;
                }
            } break;

            default:
            {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
            } break;
        }
    }
}
#endif

inline LARGE_INTEGER Win32GetWallClock()
{
    LARGE_INTEGER Counter;
    QueryPerformanceCounter(&Counter);
    return Counter;
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    real32 Result = ((real32)(End.QuadPart - Start.QuadPart) / 
                        (real32)GlobalPerfCountFrequency);

    return Result;
}

#if 0
#if BUS_INTERNAL
internal void Win32DebugDrawVertical(win32_offscreen_buffer* Backbuffer, int X, int Top, int Bottom, uint32 Color)
{
    if (Top <= 0)
    {
        Top = 0;
    }
    if (Bottom > Backbuffer->Height)
    {
        Bottom = Backbuffer->Height;
    }

    if ((X >= 0) && (X < Backbuffer->Width))
    {    
        uint8* Pixel = (uint8 *)Backbuffer->Memory + 
                        X * Backbuffer->BytesPerPixel + 
                        Top * Backbuffer->Pitch;

        for (int Y = Top; Y < Bottom; ++Y)
        {
            *(uint32 *)Pixel = Color;
            Pixel += Backbuffer->Pitch;
        }
    }
}

internal void Win32DebugDrawSoundBufferMarker(win32_offscreen_buffer* Backbuffer, 
                                                win32_sound_output* SoundOutput, 
                                                real32 C, int PadX, int Top, int Bottom,
                                                DWORD Value, 
                                                uint32 Color)
{
    int X = PadX + (int)(C * (real32)Value);
    Win32DebugDrawVertical(Backbuffer, X, Top, Bottom, Color);    
}

internal void Win32DebugSyncDisplay(win32_offscreen_buffer* Backbuffer, 
                                        int MarkerCount, 
                                        win32_debug_time_marker* Markers, 
                                        int CurrentMarkerIndex,
                                        win32_sound_output* SoundOutput, 
                                        real32 TargetSecondsPerFrame)
{
    // TODO - draw where we are writing out sound
    int PadX = 16;
    int PadY = 16;

    int LineHeight = 64;

    real32 C = (real32)(Backbuffer->Width - 2*PadX) / (real32)SoundOutput->SecondaryBufferSize;
    for (int MarkerIndex = 0; 
        MarkerIndex < MarkerCount;
        ++MarkerIndex)
    {
        DWORD PlayColor = 0xFFFFFFFF;
        DWORD WriteColor = 0xFFFF0000;
        DWORD ExpectedFlipColor = 0xFFFFFF00;
        DWORD PlayWindowColor = 0xFFFF00FF;

        int Top = PadY;
        int Bottom = PadY + LineHeight;
        win32_debug_time_marker* Marker = &Markers[MarkerIndex];
        Assert(Marker->OutputPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(Marker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
        Assert(Marker->OutputLocation < SoundOutput->SecondaryBufferSize);
        Assert(Marker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(Marker->FlipWriteCursor < SoundOutput->SecondaryBufferSize);

        if (MarkerIndex == CurrentMarkerIndex)
        {
            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            int FirstTop = Top;

            Win32DebugDrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, Marker->OutputPlayCursor, PlayColor);
            Win32DebugDrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, Marker->OutputWriteCursor, WriteColor);

            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            Win32DebugDrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, Marker->OutputLocation, PlayColor);
            Win32DebugDrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, Marker->OutputLocation + Marker->OutputByteCount, WriteColor);

            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            Win32DebugDrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, FirstTop, Bottom, Marker->ExpectedFlipPlayCursor, ExpectedFlipColor);
        }

        Win32DebugDrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, Marker->FlipPlayCursor, PlayColor);
        Win32DebugDrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, Marker->FlipPlayCursor + 480 * SoundOutput->BytesPerSample, PlayWindowColor);
        Win32DebugDrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, Marker->FlipWriteCursor, WriteColor);
    }
}
#endif
#endif

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
    srand (time(NULL));
#ifdef USING_SFML
    // SHIPPING FULLSCREEN
//    sf::RenderWindow window(sf::VideoMode(1920, 1080), "Fare", sf::Style::Fullscreen);
    sf::RenderWindow window(sf::VideoMode(1920, 1080), "Fare");

    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    win32_state Win32State = {};
    
    Win32GetEXEFileName(&Win32State);
    char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];

    Win32BuildEXEPathFileName(&Win32State, "bus.dll", WIN32_STATE_FILE_NAME_COUNT, SourceGameCodeDLLFullPath);
    Win32BuildEXEPathFileName(&Win32State, "bus_temp.dll", WIN32_STATE_FILE_NAME_COUNT, TempGameCodeDLLFullPath);

    UINT desiredSchedulerMS = 1;
    bool32 SleepIsGranular = (timeBeginPeriod(desiredSchedulerMS) == TIMERR_NOERROR);

    // TODO - might replace input
    Win32LoadXInput();

    int MonitorRefreshHz = 60;
    real32 GameUpdateHz = (MonitorRefreshHz / 2.f);
    real32 TargetSecondsPerFrame = 1.f / (real32)GameUpdateHz;

    win32_sound_output SoundOutput = {};
    SoundOutput.SamplesPerSecond = 48000;
    SoundOutput.RunningSampleIndex = 0;
    SoundOutput.BytesPerSample = sizeof(int16)*2;
    SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
    SoundOutput.SafetyBytes = (int)((SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / (real32)GameUpdateHz / 3.f); // 1/3rd variability in our game update TODO - compute this variance to see what the lowest reasonable value is

    window.setFramerateLimit(MonitorRefreshHz);

    // TODO: Pool with bitmap VirtualAlloc
    int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
                                           MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);


#if BUS_INTERNAL
    LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
    LPVOID BaseAddress = 0;
#endif

    game_memory GameMemory = {};

    GameMemory.PermanentStorageSize = Megabytes(64);
//            GameMemory.TransientStorageSize = Gigabytes(1);
    GameMemory.TransientStorageSize = Megabytes(100);

    Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;

    // TODO MEM_LARGE_PAGES requires a few things
    // will we have to not pass MEM_LARGE_PAGES on WinXP?
    Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)Win32State.TotalSize,
                                            MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

    GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
    GameMemory.TransientStorage = (uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize;
    GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
    GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile; 
    GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

    // this stuff still seems slow. see if we can figure out how we can help windows speed up input recording
#if 0
    for (int ReplayIndex = 0;
            ReplayIndex < ArrayCount(Win32State.ReplayBuffers);
            ++ReplayIndex)
    {
        win32_replay_buffer* ReplayBuffer = &Win32State.ReplayBuffers[ReplayIndex];

        Win32GetInputFileLocation(&Win32State, false, ReplayIndex, sizeof(ReplayBuffer->ReplayFileName), ReplayBuffer->ReplayFileName);

        ReplayBuffer->FileHandle = CreateFileA(ReplayBuffer->ReplayFileName, GENERIC_READ|GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

        DWORD MaxSizeHigh = Win32State.TotalSize >> 32;
        DWORD MaxSizeLow = Win32State.TotalSize & 0xFFFFFFFF;
        ReplayBuffer->MemoryMap = CreateFileMapping(ReplayBuffer->FileHandle, 0, PAGE_READWRITE, MaxSizeHigh, MaxSizeLow, 0);
        ReplayBuffer->MemoryBlock = MapViewOfFile(ReplayBuffer->MemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, Win32State.TotalSize);

        if (ReplayBuffer->MemoryBlock)
        {

        }
        else
        {
            //TODO - diagnostic
        }
    }
#endif

    if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
    {
        game_input Input[2] = {};
        game_input* NewInput = &Input[0];
        game_input* OldInput = &Input[1];

        LARGE_INTEGER LastCounter = Win32GetWallClock();
        LARGE_INTEGER FlipWallClock = Win32GetWallClock();
        uint64 LastCycleCount = __rdtsc();

        bool32 SoundIsValid =  false;
        DWORD AudioLatencyBytes = 0;
        real32 AudioLatencySeconds = 0.f;

#if BUS_INTERNAL
        int DebugTimeMarkerIndex = 0;
        win32_debug_time_marker DebugTimeMarker[30] = {0};
#endif

        win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);

        while (window.isOpen())
        {
            // check all the window's events that were triggered since the last iteration of the loop
            sf::Event event;
            while (window.pollEvent(event))
            {
                // "close requested" event: we close the window
                if (event.type == sf::Event::Closed)
                    window.close();
            }

            NewInput->dtForFrame = TargetSecondsPerFrame;
            FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
            if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0)
            {
                Win32UnloadGameCode(&Game);
                Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);
            }

            // TODO - zeroing macro
            // TODO - We can't zero everything because the up/down state will
            // be wrong!!!
            game_controller_input* OldKeyboardController = GetController(OldInput, 0);
            game_controller_input* NewKeyboardController = GetController(NewInput, 0);
            game_controller_input ZeroController = {};
            *NewKeyboardController = ZeroController;
            NewKeyboardController->IsConnected = true;
            for (int ButtonIndex = 0; ButtonIndex < ArrayCount(OldKeyboardController->Buttons); ++ButtonIndex)
            {
                NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
            }

            thread_context Thread = {};

            if (Win32State.InputRecordingIndex > 0)
            {
                Win32RecordInput(&Win32State, NewInput);
            }
            if (Win32State.InputPlayingIndex > 0)
            {
                Win32PlaybackInput(&Win32State, NewInput);
            }
            if (Game.UpdateAndRender)
            {
                window.clear(sf::Color::Black);

                Game.UpdateAndRender(&GameMemory, NewInput, window, &Thread, TargetSecondsPerFrame);
                window.display();
            }

            LARGE_INTEGER WorkCounter = Win32GetWallClock();
            real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

            // untested so possibly buggy
            real32 SecondsElapsedForFrame = WorkSecondsElapsed;
            if (SecondsElapsedForFrame < TargetSecondsPerFrame)
            {
                // this could also go inside the while loop although it _shouldn't_ make a difference.
                if (SleepIsGranular)
                {
                    DWORD SleepMS = (DWORD)(1000.f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
                    if (SleepMS > 0)
                    {
                        Sleep(SleepMS);
                    }
                }

                real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                // TODO - LOG THIS MISS HERE
                //Assert(TestSecondsElapsedForFrame < TargetSecondsPerFrame);

                while (SecondsElapsedForFrame < TargetSecondsPerFrame)
                {
                    SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                }
            }
            else
            {
                // TODO - log missed frame.
            }

            LARGE_INTEGER EndCounter = Win32GetWallClock();
            real64 MSPerFrame = 1000.f * Win32GetSecondsElapsed(LastCounter, EndCounter);
            LastCounter = EndCounter;                    

            FlipWallClock = Win32GetWallClock();

            uint64 EndCycleCount = __rdtsc();
            uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
            LastCycleCount = EndCycleCount;

            real64 FPS = 0.f;
            real64 MCPF = (real64)CyclesElapsed / (1000.f * 1000.f);

#if BUS_INTERNAL
//                char Buffer[256];
            // _snprintf_s(Buffer, sizeof(Buffer), "%d ms, Fps %d, MegaCycle Count %d\n", (int32)MSPerFrame, (int32)FPS, (int32)MCPF);
//                OutputDebugStringA(Buffer);
#endif

#if BUS_INTERNAL
            ++DebugTimeMarkerIndex;
            if (DebugTimeMarkerIndex == ArrayCount(DebugTimeMarker))
            {
                DebugTimeMarkerIndex = 0;
            }
#endif

            game_input* Temp = NewInput;
            NewInput = OldInput;
            OldInput = Temp;
        }
    }
#else
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    win32_state Win32State = {};
    
    Win32GetEXEFileName(&Win32State);
    char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];

    Win32BuildEXEPathFileName(&Win32State, "bus.dll", WIN32_STATE_FILE_NAME_COUNT, SourceGameCodeDLLFullPath);
    Win32BuildEXEPathFileName(&Win32State, "bus_temp.dll", WIN32_STATE_FILE_NAME_COUNT, TempGameCodeDLLFullPath);

    UINT desiredSchedulerMS = 1;
    bool32 SleepIsGranular = (timeBeginPeriod(desiredSchedulerMS) == TIMERR_NOERROR);

    Win32LoadXInput();
    
    WNDCLASSA WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackbuffer, 960, 540); // 960*540 is for our software renderer.

    WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
//    WindowClass.hIcon;
    WindowClass.lpszClassName = "BusWindowClass";

    if(RegisterClassA(&WindowClass))
    {
        HWND Window =
            CreateWindowExA(
                0,//WS_EX_TOPMOST,//|WS_EX_LAYERED
                WindowClass.lpszClassName,
                "BUS",
                WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                0,
                0,
                Instance,
                0);
        if(Window)
        {
            // NOTE(casey): Since we specified CS_OWNDC, we can just
            // get one device context and use it forever because we
            // are not sharing it with anyone.

            HDC RefreshDC = GetDC(Window);
            int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
            ReleaseDC(Window,RefreshDC);

            int MonitorRefreshHz = 60;
            if (Win32RefreshRate > 1)
            {
                MonitorRefreshHz = Win32RefreshRate;
            }
            real32 GameUpdateHz = (MonitorRefreshHz / 2.f);
            real32 TargetSecondsPerFrame = 1.f / (real32)GameUpdateHz;

            win32_sound_output SoundOutput = {};
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.BytesPerSample = sizeof(int16)*2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            SoundOutput.SafetyBytes = (int)((SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / (real32)GameUpdateHz / 3.f); // 1/3rd variability in our game update TODO - compute this variance to see what the lowest reasonable value is

            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;

#if 0
            // test PC/WC update frequency. 480 Samples when we ran it.
            while(GlobalRunning)
            {
                DWORD PlayCursor, WriteCursor;
                GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);

                char TextBuffer[256];
				_snprintf_s(TextBuffer, sizeof(TextBuffer), "PC: %u WC: %u\n", PlayCursor, WriteCursor);
                OutputDebugStringA(TextBuffer);
            }
#endif

            // TODO: Pool with bitmap VirtualAlloc
            int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
                                                   MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);


#if BUS_INTERNAL
			LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
			LPVOID BaseAddress = 0;
#endif

            game_memory GameMemory = {};

            GameMemory.PermanentStorageSize = Megabytes(64);
//            GameMemory.TransientStorageSize = Gigabytes(1);
            GameMemory.TransientStorageSize = Megabytes(100);

            Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;

            // TODO MEM_LARGE_PAGES requires a few things
            // will we have to not pass MEM_LARGE_PAGES on WinXP?
            Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)Win32State.TotalSize,
                                                    MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

            GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
            GameMemory.TransientStorage = (uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize;
            GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
            GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile; 
            GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

            // this stuff still seems slow. see if we can figure out how we can help windows speed up input recording
            for (int ReplayIndex = 0;
                    ReplayIndex < ArrayCount(Win32State.ReplayBuffers);
                    ++ReplayIndex)
            {
                win32_replay_buffer* ReplayBuffer = &Win32State.ReplayBuffers[ReplayIndex];

                Win32GetInputFileLocation(&Win32State, false, ReplayIndex, sizeof(ReplayBuffer->ReplayFileName), ReplayBuffer->ReplayFileName);

                ReplayBuffer->FileHandle = CreateFileA(ReplayBuffer->ReplayFileName, GENERIC_READ|GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

                DWORD MaxSizeHigh = Win32State.TotalSize >> 32;
                DWORD MaxSizeLow = Win32State.TotalSize & 0xFFFFFFFF;
                ReplayBuffer->MemoryMap = CreateFileMapping(ReplayBuffer->FileHandle, 0, PAGE_READWRITE, MaxSizeHigh, MaxSizeLow, 0);
                ReplayBuffer->MemoryBlock = MapViewOfFile(ReplayBuffer->MemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, Win32State.TotalSize);

                if (ReplayBuffer->MemoryBlock)
                {

                }
                else
                {
                    //TODO - diagnostic
                }
            }

            if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
            {
                game_input Input[2] = {};
                game_input* NewInput = &Input[0];
                game_input* OldInput = &Input[1];

                LARGE_INTEGER LastCounter = Win32GetWallClock();
                LARGE_INTEGER FlipWallClock = Win32GetWallClock();
                uint64 LastCycleCount = __rdtsc();

                bool32 SoundIsValid =  false;
                DWORD AudioLatencyBytes = 0;
                real32 AudioLatencySeconds = 0.f;

#if BUS_INTERNAL
                int DebugTimeMarkerIndex = 0;
                win32_debug_time_marker DebugTimeMarker[30] = {0};
#endif

                win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);

                while(GlobalRunning)
                {
                    NewInput->dtForFrame = TargetSecondsPerFrame;
                    FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
                    if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0)
                    {
                        Win32UnloadGameCode(&Game);
                        Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);
                    }

                    // TODO - zeroing macro
                    // TODO - We can't zero everything because the up/down state will
                    // be wrong!!!
                    game_controller_input* OldKeyboardController = GetController(OldInput, 0);
                    game_controller_input* NewKeyboardController = GetController(NewInput, 0);
                    game_controller_input ZeroController = {};
                    *NewKeyboardController = ZeroController;
                    NewKeyboardController->IsConnected = true;
                    for (int ButtonIndex = 0; ButtonIndex < ArrayCount(OldKeyboardController->Buttons); ++ButtonIndex)
                    {
                        NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                    }

                    Win32ProcessPendingMessages(NewKeyboardController, &Win32State);

                    if (!GlobalPause)
                    {
                        POINT MouseP;
                        GetCursorPos(&MouseP);
                        ScreenToClient(Window, &MouseP);
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0], GetKeyState(VK_LBUTTON) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1], GetKeyState(VK_RBUTTON) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2], GetKeyState(VK_MBUTTON) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));
                        NewInput->MouseX = MouseP.x;
                        NewInput->MouseY = MouseP.y;
                        NewInput->MouseZ = 0; // TODO - support mousewheel?

                        // TODO - don't poll disconnected controllers to avoid xinput frame rate hitch on older libraries
                        // TODO(casey): Should we poll this more frequently
                        DWORD MaxControllerCount = XUSER_MAX_COUNT;
                        if(MaxControllerCount > (ArrayCount(NewInput->Controllers)-1))
                        {
                            MaxControllerCount = (ArrayCount(NewInput->Controllers)-1);
                        }

                        for (DWORD ControllerIndex = 0;
                             ControllerIndex < MaxControllerCount;
                             ++ControllerIndex)
                        {
                            int OurControllerIndex = 1 + ControllerIndex;
                            game_controller_input* OldController = GetController(OldInput, OurControllerIndex);
                            game_controller_input* NewController = GetController(NewInput, OurControllerIndex);
                            XINPUT_STATE ControllerState;
                            if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                            {
                                NewController->IsConnected = true;
                                NewController->IsAnalog = OldController->IsAnalog;

                                // TODO(casey): See if ControllerState.dwPacketNumber increments too rapidly
                                XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                                // TODO - collapse to single function
                                NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

                                if (NewController->StickAverageX != 0.f ||
                                    NewController->StickAverageY != 0.f)
                                {
                                    NewController->IsAnalog = true;
                                }

                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                                {
                                    NewController->StickAverageY = 1.f;
                                    NewController->IsAnalog = false;
                                }   
                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                                {
                                    NewController->IsAnalog = false;
                                    NewController->StickAverageY = -1.f;
                                }   
                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                                {
                                    NewController->IsAnalog = false;
                                    NewController->StickAverageX = -1.f;
                                }   
                                if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                                {
                                    NewController->IsAnalog = false;
                                    NewController->StickAverageX = 1.f;
                                }   

                                real32 Threshold = .5f;
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageX < -Threshold) ? 1 : 0, &OldController->MoveLeft, 1, &NewController->MoveLeft);
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageX > Threshold) ? 1 : 0, &OldController->MoveRight, 1, &NewController->MoveRight);
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageY < -Threshold) ? 1 : 0, &OldController->MoveDown, 1, &NewController->MoveDown);
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageY > Threshold) ? 1 : 0, &OldController->MoveUp, 1, &NewController->MoveUp);

                                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionDown, XINPUT_GAMEPAD_A, &NewController->ActionDown);
                                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionRight, XINPUT_GAMEPAD_B, &NewController->ActionRight);
                                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionLeft, XINPUT_GAMEPAD_X, &NewController->ActionLeft);
                                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionUp, XINPUT_GAMEPAD_Y, &NewController->ActionUp);
                                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
                                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);

                                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Back, XINPUT_GAMEPAD_BACK, &NewController->Back);
                                Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Start, XINPUT_GAMEPAD_START, &NewController->Start);
                            }
                            else
                            {
                                NewController->IsConnected = false;
                            }
                        }

                        thread_context Thread = {};

                        game_offscreen_buffer GameBuffer = {};
                        GameBuffer.Memory = GlobalBackbuffer.Memory;
                        GameBuffer.Width = GlobalBackbuffer.Width;
                        GameBuffer.Height = GlobalBackbuffer.Height;
                        GameBuffer.Pitch = GlobalBackbuffer.Pitch;
                        GameBuffer.BytesPerPixel = GlobalBackbuffer.BytesPerPixel;

                        if (Win32State.InputRecordingIndex > 0)
                        {
                            Win32RecordInput(&Win32State, NewInput);
                        }
                        if (Win32State.InputPlayingIndex > 0)
                        {
                            Win32PlaybackInput(&Win32State, NewInput);
                        }
                        if (Game.UpdateAndRender)
                        {
                            Game.UpdateAndRender(&GameMemory, NewInput, &GameBuffer, &Thread, TargetSecondsPerFrame);
                        }

                        LARGE_INTEGER AudioWallClock = Win32GetWallClock();
                        real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);

                        DWORD PlayCursor, WriteCursor;

                        if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                        {
                            /* Audio Latency Functionality
                            We define a safety value that is the number of samples we think our game update loop may vary by.

                            When we wake up to write audio, we will look and see what the play cursor position is and we will forecast ahead 
                            where we think the PC will be on the next frame boundary.

                            We will then look to see if the write cursor is before that by at least our safety margin. If it is, the target fill position 
                            is that frame boundary plus one frame. This gives us perfect audio sync in the case of a card that has low enough audio latency.

                            if the write cursor is after that safety margin, then we assume we can never sync the audio perfectly
                            so we will write one frame's worth of audio plus the safety margin's worth of guard samples (1ms, or something determined to be safe,
                            whatever we think the variability of our frame computation is).

                            */
                            if (!SoundIsValid)
                            {
                                SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                                SoundIsValid = true;
                            }

                            DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;

                            DWORD ExpectedSoundBytesPerFrame = (int)((real32)(SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz);
                            real32 SecondsLeftUntilFlip = TargetSecondsPerFrame - FromBeginToAudioSeconds;
                            DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame) * (real32)ExpectedSoundBytesPerFrame); 
                            DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFlip;

                            DWORD SafeWriteCursor = WriteCursor;
                            if (SafeWriteCursor < PlayCursor)
                            {
                                SafeWriteCursor += SoundOutput.SecondaryBufferSize;
                            }
                            Assert(SafeWriteCursor >= PlayCursor);
                            SafeWriteCursor += SoundOutput.SafetyBytes;
                            bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);

                            DWORD TargetCursor = 0;
                            if (AudioCardIsLowLatency)
                            {
                                TargetCursor = ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame;
                            }
                            else
                            {
                                TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes);
                            }
                            TargetCursor = (TargetCursor % SoundOutput.SecondaryBufferSize);

                            DWORD BytesToWrite = 0;
                            if (ByteToLock > TargetCursor)
                            {
                                BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock) + TargetCursor;
                            }
                            else
                            {
                                BytesToWrite = TargetCursor - ByteToLock;
                            }

                            // TODO - it looks like he'll break this up and move the audio into GameGetSoundSamples
                            game_sound_output_buffer GameSoundBuffer = {};
                            GameSoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                            GameSoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                            GameSoundBuffer.Samples = Samples;
                            if (Game.GetSoundSamples)
                            {
                                Game.GetSoundSamples(&GameMemory, &GameSoundBuffer, &Thread);
                            }

    #if BUS_INTERNAL
                            win32_debug_time_marker* Marker = &DebugTimeMarker[DebugTimeMarkerIndex];
                            Marker->OutputPlayCursor = PlayCursor;
                            Marker->OutputWriteCursor = WriteCursor;
                            Marker->OutputLocation = ByteToLock;
                            Marker->OutputByteCount = BytesToWrite;
                            Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;

                            DWORD UnwrappedWriteCursor = WriteCursor;
                            if (UnwrappedWriteCursor < PlayCursor)
                            {
                                UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
                            }
                            AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
                            AudioLatencySeconds = ((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) / (real32)SoundOutput.SamplesPerSecond;

                            // char TextBuffer[256];
                            // _snprintf_s(TextBuffer, sizeof(TextBuffer), "PC: %u BTL: %u TC: %u BTW: %u - PC: %u WC: %u DELTA: %u %f\n", 
                            //                         PlayCursor, ByteToLock, TargetCursor, BytesToWrite, PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
                            // OutputDebugStringA(TextBuffer);                        
    #endif
                            Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &GameSoundBuffer);
                        }
                        else
                        {
                            SoundIsValid = false;
                        }

                        LARGE_INTEGER WorkCounter = Win32GetWallClock();
                        real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

                        // untested so possibly buggy
                        real32 SecondsElapsedForFrame = WorkSecondsElapsed;
                        if (SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            // this could also go inside the while loop although it _shouldn't_ make a difference.
                            if (SleepIsGranular)
                            {
                                DWORD SleepMS = (DWORD)(1000.f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
                                if (SleepMS > 0)
                                {
                                    Sleep(SleepMS);
                                }
                            }

                            real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                            // TODO - LOG THIS MISS HERE
                            //Assert(TestSecondsElapsedForFrame < TargetSecondsPerFrame);

                            while (SecondsElapsedForFrame < TargetSecondsPerFrame)
                            {
                                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                            }
                        }
                        else
                        {
                            // TODO - log missed frame.
                        }

                        LARGE_INTEGER EndCounter = Win32GetWallClock();
                        real64 MSPerFrame = 1000.f * Win32GetSecondsElapsed(LastCounter, EndCounter);
                        LastCounter = EndCounter;                    

                        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
    #if BUS_INTERNAL
                        // NOTE - this is wrong on the 0th index
//                        Win32DebugSyncDisplay(&GlobalBackbuffer, ArrayCount(DebugTimeMarker), DebugTimeMarker, DebugTimeMarkerIndex - 1, &SoundOutput, TargetSecondsPerFrame);
    #endif

                        HDC DeviceContext = GetDC(Window);
                        Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext,
                                                   Dimension.Width, Dimension.Height);
                        ReleaseDC(Window, DeviceContext);

                        FlipWallClock = Win32GetWallClock();

    #if BUS_INTERNAL
                        {
                            DWORD PlayCursor;
                            DWORD WriteCursor;
                            if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                            {
                                Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarker));
                                win32_debug_time_marker* Marker = &DebugTimeMarker[DebugTimeMarkerIndex];
                                Marker->FlipPlayCursor = PlayCursor;
                                Marker->FlipWriteCursor = WriteCursor;
                            }
                        }
    #endif

                        uint64 EndCycleCount = __rdtsc();
                        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
                        LastCycleCount = EndCycleCount;

                        real64 FPS = 0.f;
                        real64 MCPF = (real64)CyclesElapsed / (1000.f * 1000.f);

    #if BUS_INTERNAL
         //                char Buffer[256];
    					// _snprintf_s(Buffer, sizeof(Buffer), "%d ms, Fps %d, MegaCycle Count %d\n", (int32)MSPerFrame, (int32)FPS, (int32)MCPF);
         //                OutputDebugStringA(Buffer);
    #endif

    #if BUS_INTERNAL
                        ++DebugTimeMarkerIndex;
                        if (DebugTimeMarkerIndex == ArrayCount(DebugTimeMarker))
                        {
                            DebugTimeMarkerIndex = 0;
                        }
    #endif

                        game_input* Temp = NewInput;
                        NewInput = OldInput;
                        OldInput = Temp;
                    }
                }//  GlobalPause
            }
            else
            {
                // TODO - we can't allocate memory
            }
        }
        else
        {
            // TODO(casey): Logging
        }
    }
    else
    {
        // TODO(casey): Logging
    }
#endif // USING_SFML
    
    return(0);
}
