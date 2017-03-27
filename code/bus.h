#ifndef __BUSGAME_H_

#include "bus_platform.h"

#define Pi32 3.14159265359f

#if NETRUNNER_SLOW
	#define Assert(Expression) \
		if (!(Expression)) {*(int *)0 = 0;}
#else
	#define Assert(Expression)
#endif

// TODO - swap, min, max, macros?
#define KiloBytes(Value) ((Value) * 1024)
#define Megabytes(Value) (KiloBytes(Value) * 1024)
#define Gigabytes(Value) (Megabytes(Value) * 1024)
#define Terabytes(Value) (Gigabytes(Value) * 1024)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

inline uint32
SafeTruncateUInt64(uint64 Value)
{
	// TODO - MAX_INT
    Assert(Value <= 0xFFFFFFFF);
    return (uint32)Value;	
}

inline game_controller_input* GetController(game_input* Input, int ControllerIndex)
{
	Assert(ControllerIndex < ArrayCount(Input->Controllers));
	return &Input->Controllers[ControllerIndex];
}

#include "bus_intrinsics.h"

struct memory_arena
{
    memory_index Size;
    uint8* Base;
    memory_index Used;
};

struct dialogue_entry
{
    const char* branch;
    const char* line;
    bool32 newline;
    const char* branchto;
    const char* energyeffect;
    const char* stateeffect;
    sf::FloatRect worldLineBounds;    
};

struct dialogue_tree
{
    uint32 NumEntries;
    dialogue_entry* Entries;
};


struct passenger_template
{
    const char* Id;
    const char* CharId;
    const char* AvatarBody;
    const char* AvatarHead;
    const char* DialogIdentifier;

    real32 StartMood;
    real32 BoardTime;

    // TODO - these are being duplicated right now
    sf::Texture* AvatarBodyTexture;
    sf::Sprite* AvatarBodySprite;
    sf::Texture* AvatarHeadTexture;
    sf::Sprite* AvatarHeadSprite;

    // TODO - these are being duplicated right now
    dialogue_tree DialogueTree;
};

#define MAX_DIALOGUE_BRANCHES_TAKEN 10

struct active_dialogue_branches
{
    const char* DialogueBranchesTaken[MAX_DIALOGUE_BRANCHES_TAKEN];
    uint32 NextDialogueBranchIndex;
    bool32 DialogueIsDone;
    bool32 PassengerIsIn;
};

struct passenger
{
    active_dialogue_branches ActiveBranches;
    passenger_template* Template;
    real32 Mood;
};

#define PASSENGER_ENTRY_X 1000

#define DEFAULT_TIME_BETWEEN_STOPS 8.f
#define DEFAULT_COUNTDOWN_TO_BUS_MOVING 2.f
#define DEFAULT_COUNTDOWN_TO_OFFWORK 2.f
#define DEFAULT_COUNTDOWN_TO_DRIVING 5.f
#define DEFAULT_BUS_SPEED 8
#define MAX_ENERGY_METER_WIDTH 200
#define MAX_PASSENGERS 1000
#define MAX_TOTAL_STATE_EFFECTS 200
#define STATE_EFFECT_COUNTDOWN 3.f

struct sf_sprite
{
    sf::Texture* t;
    sf::Sprite* s;
};

enum game_mode
{
    game_mode_init,
    game_mode_title,
    game_mode_driving,
    game_mode_boarding,
    game_mode_transition,
    game_mode_transition_to_offwork,
    game_mode_offwork,
    game_mode_transition_to_driving,
};

// stupid
struct string_id
{
    const char* id;
};

struct stop_template
{
    const char* id;
    uint32 NumPassengers;
    string_id* PassengerIds;
};

struct day
{
    const char* id; // don't know if I need this.
    uint32 NumStops;
    string_id* StopIds;
};

struct script
{
    uint32 NumStopTemplates;
    stop_template* StopTemplates;

    uint32 NumDays;
    day* Days;
}; 

// stop and route are runtime versions of day and stop_template
struct stop
{
    stop_template* Template;

    real32 DistanceToStop;
    uint32 NumPassengersHandled;
};

// TODO - route should be created from a series of days
struct route
{
    uint32 NumStops;
    int32 NextStop;
    real32 TimeToNextStop;
    real32 ElapsedTime;
    stop* Stops;
};


enum boarding_state
{
    boarding_state_eval,
    boarding_state_in,
    boarding_state_dialogue,
    boarding_state_out
};

struct passenger_list
{
    uint32 PassengerDbCount;
    passenger_template* PassengerDb;
};

struct story_state
{
    sf_sprite CurrentStoryBg;
    active_dialogue_branches ActiveBranches;
    dialogue_tree DialogTree;
};

struct game_state
{
    memory_arena WorldArena;
    sf_sprite BusImage;
    sf_sprite DoorImage; 
    sf_sprite DialogueBackground;
    sf_sprite BackgroundImage;
    sf_sprite Schematic;
    sf_sprite Waiting;
    sf_sprite Title;
    int32 BackgroundImageXScroll;
    int32 BusStopXScroll;

    char* StateEffectText;
    real32 StateEffectTextCountdown;

    passenger_list GeneralPassengers;
    passenger_list NightPassengers;
    passenger_list CustomPassengers;

    uint32 CurrentPassengerCount;
    passenger CurrentPassengers[MAX_PASSENGERS];

    boarding_state BoardingState;
    real32 BoardingStateCountdown;
    passenger BoardingPassenger;

    game_mode GameMode;
    game_mode PendingGameMode;
    real32 TransitionCountdown;

    script Script;
    uint32 CurrentDay; // day, day, night, report, etc.

    route CurrentRoute;

    // mood is normalized to +/- 1
    real32 DriverMood;
    bool32 FlashMeterPositive;
    bool32 FlashMeterNegative;
    real32 FlashLength;

    char* PendingStateEffects[MAX_TOTAL_STATE_EFFECTS];
    char* StateEffects[MAX_TOTAL_STATE_EFFECTS];
    uint32 NumPendingStateEffects;
    uint32 NumStateEffects;

    story_state StoryState;

    // points to whatever dialog tree is active; for input/callback type handling
    active_dialogue_branches* CurrentActiveBranches;
    dialogue_tree* CurrentDialogTree;

    sf::Font* Font;

    sf::SoundBuffer* AmbientAudioBuffer;
    sf::Sound* AmbientAudio;
    sf::SoundBuffer* DriveStopBuffer;
    sf::Sound* DriveStopAudio;

    sf::SoundBuffer* DoorOpenAudioBuffer;
    sf::Sound* DoorOpenAudio;

    sf::SoundBuffer* PositiveBuffer;
    sf::Sound* PositiveAudio;
    sf::SoundBuffer* NegativeAudioBuffer;
    sf::Sound* NegativeAudio;
};

// services that the platform provides to the game


#define __BUSGAME_H_
#endif