#ifndef __NETRUNNER_PLATFORM_H_

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#ifdef __cplusplus
extern "C" {
#endif

// TODO - NETRUNNER_INTERNAL: 0 means build for public release. 1 means dev only. 

#include <stdint.h>

#define internal static 
#define local_persist static 
#define global_variable static

typedef int32_t bool32;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef size_t memory_index;

typedef float real32;
typedef double real64;

// services that the platform provides to the game

typedef struct thread_context
{

} thread_context;

#if BUS_INTERNAL
// These are not for doing anything in the shipping game. They are blocking and the write doesn't protect against lost data
typedef struct debug_read_file_result
{
	int32 ContentsSize;
	void* Contents;
} debug_read_file_result;

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context* Thread, void* Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context* Thread, char* FileName)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(thread_context* Thread, char* FileName, uint32 MemorySize, void* Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif

// services that the game provides to the platform

// rendering specifically will have a 3-tiered abstraction. Display list abstraction.
typedef struct game_offscreen_buffer
{
	void* Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
} game_offscreen_buffer;

typedef struct game_sound_output_buffer
{
	int SamplesPerSecond;
	int SampleCount;
	int16* Samples;

} game_sound_output_buffer;

// general
//    if (Input.IsAnalog) {}

// buttons
//    if (Input.AButtonEndedDown) {}

// sticks
    // Input.StartX;
    // Input.MinX;
    // Input.MaxX;
    // Input.EndX;
typedef struct game_button_state
{
	int HalfTransitionCount;
	bool32 EndedDown;
} game_button_state;

typedef struct game_controller_input
{
	bool32 IsConnected;
	bool32 IsAnalog;
	real32 StickAverageX;
	real32 StickAverageY;

	union
	{
		game_button_state Buttons[12];
		struct 
		{
			game_button_state MoveUp;
			game_button_state MoveDown;
			game_button_state MoveLeft;
			game_button_state MoveRight;	

			game_button_state ActionUp;
			game_button_state ActionDown;
			game_button_state ActionLeft;
			game_button_state ActionRight;

			game_button_state LeftShoulder;
			game_button_state RightShoulder;	

			game_button_state Back;
			game_button_state Start;
		};
	};
} game_controller_input;

typedef struct game_input
{
	game_button_state MouseButtons[5];
	int32 MouseX, MouseY, MouseZ;

	real32 dtForFrame;

	game_controller_input Controllers[5];
} game_input;

typedef struct game_memory
{
	bool32 IsInitialized;
	uint64 PermanentStorageSize;
	void*  PermanentStorage; // required to be cleared to 0 at startup

	uint64 TransientStorageSize;
	void*  TransientStorage; // required to be cleared to 0 at startup

	debug_platform_free_file_memory* DEBUGPlatformFreeFileMemory;
	debug_platform_read_entire_file* DEBUGPlatformReadEntireFile; 
	debug_platform_write_entire_file* DEBUGPlatformWriteEntireFile;
} game_memory;

// 3 things:
// timing
// controller/keyboard input
// bitmap buffer to use
// sound buffer to use
// this may expand in the future (sound on a separate thread, for example)
#define GAME_UPDATE_AND_RENDER(name) void name(game_memory* Memory, game_input* Input, sf::RenderWindow& Window, thread_context* Thread, real32 DeltaTime)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);


// NOTE - at this moment, this has to be a very fast function (<1ms)
// TODO - reduce the pressure on this function's performance by measuring it or asking about it, etc.
#define GAME_GET_SOUND_SAMPLES(name) void name( game_memory* Memory, game_sound_output_buffer* SoundBuffer, thread_context* Thread)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

#ifdef __cplusplus
}
#endif

#define __NETRUNNER_PLATFORM_H_
#endif __NETRUNNER_PLATFORM_H_