#include "bus.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb-master/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb-master/stb_image_resize.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb-master/stb_truetype.h"

#include "json.cpp"
#include "json.h"

internal void
InitializeArena(memory_arena* Arena, memory_index Size, uint8* Base)
{
    Arena->Size = Size;
    Arena->Base = Base;
    Arena->Used = 0;
}

#define PushSize(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count)*sizeof(type))

void*
PushSize_(memory_arena* Arena, memory_index Size)
{
    Assert((Arena->Used + Size) <= Arena->Size);
    void* Result = Arena->Base + Arena->Used;
    Arena->Used += Size;
    return Result;
}

#include "bus_load.cpp"

internal void
LoadSfSprite(sf_sprite* Image, const char* fileName)
{
    Image->t = new sf::Texture;
    Image->t->loadFromFile(fileName);
    Image->s = new sf::Sprite;
    Image->s->setTexture(*Image->t);
}

internal void
DrawSprite(sf::RenderWindow& Window, sf::Sprite& Sprite, int32 TopLeftX, int32 TopLeftY, real32 Scale=1.0, real32 Alpha=1.f, bool32 Night=false)
{
    int32 Colour = Night? 40 : 255;
    Sprite.setColor(sf::Color(Colour, Colour, Colour, Alpha*255));
	Sprite.setPosition(sf::Vector2f(TopLeftX, TopLeftY));
	Sprite.setScale(sf::Vector2f(Scale,Scale));
	Window.draw(Sprite);
}

// branches are only valid in the current branch
internal bool32
IsDialogueLineBranchTo(active_dialogue_branches* Branches, dialogue_entry Entry)
{
    bool32 IsBranchToValid = strcmp(Entry.branchto,"");
    bool32 IsNotLastBranch = strcmp(Entry.branchto,"<endin>") && strcmp(Entry.branchto,"<endout>");

    if (Branches->NextDialogueBranchIndex == 0)
        return false;

    bool32 IsActiveBranch = !strcmp(Entry.branch, Branches->DialogueBranchesTaken[Branches->NextDialogueBranchIndex-1]); 

    return IsBranchToValid && IsNotLastBranch && IsActiveBranch;
}

internal bool32
IsDialogueLineActive(active_dialogue_branches* Branches, dialogue_entry Entry)
{
    bool32 FoundInLatestBranch = !strcmp(Branches->DialogueBranchesTaken[Branches->NextDialogueBranchIndex-1], Entry.branch);
    bool32 FoundBranch = false;
    bool32 FoundBranchTo = !strcmp(Entry.branchto, ""); // becomes true if branchto is empty
    for (uint32 i=0;i<Branches->NextDialogueBranchIndex;++i)
    {
        if (!strcmp(Branches->DialogueBranchesTaken[i], Entry.branch))
            FoundBranch = true;

        if (!strcmp(Branches->DialogueBranchesTaken[i], Entry.branchto))
            FoundBranchTo = true;
    }
    return FoundInLatestBranch || (FoundBranch && FoundBranchTo);
}

internal void
PushDialogueBranch(active_dialogue_branches* Branches, const char* BranchName)
{
    Branches->DialogueBranchesTaken[Branches->NextDialogueBranchIndex++] = BranchName;

    // we should never hit this but it will crash in weirder ways later
    assert(Branches->NextDialogueBranchIndex < MAX_DIALOGUE_BRANCHES_TAKEN);
}

internal void
HandleBranchToEnd(game_state* GameState, dialogue_tree* Tree, active_dialogue_branches* ActiveBranches)
{
    if (Tree->NumEntries > 0)
    {
        for (uint32 i=0;i<Tree->NumEntries;++i)
        {
            if (!IsDialogueLineActive(ActiveBranches, Tree->Entries[i]))
                continue;

            bool32 IsBranchToEnd = !strcmp(Tree->Entries[i].branchto,"<endin>") || !strcmp(Tree->Entries[i].branchto,"<endout>");

            if (IsBranchToEnd)
            {
                ActiveBranches->DialogueIsDone = true;
                ActiveBranches->PassengerIsIn = !strcmp(Tree->Entries[i].branchto,"<endin>");
                return;
            }
        }
    }
}

internal void
DrawDialogue(sf::RenderWindow& Window, sf::Font* Font, dialogue_tree* CurrentTree, active_dialogue_branches* Branches, uint32 StartTextX, uint32 StartTextY, uint32 FontSize, uint32 Spacing, real32 Alpha)
{
    if (CurrentTree->NumEntries > 0)
    {
        int32 TextX=StartTextX, TextY=StartTextY;
        for (uint32 i=0;i<CurrentTree->NumEntries;++i)
        {
            if (!IsDialogueLineActive(Branches, CurrentTree->Entries[i]))
                continue;

            sf::Text text;
            text.setFont(*Font);
            text.setString(CurrentTree->Entries[i].line);
            text.setCharacterSize(FontSize);
            text.setPosition(TextX, TextY);

            text.setFillColor(sf::Color(255,255,255,255*Alpha));
			if (IsDialogueLineBranchTo(Branches, CurrentTree->Entries[i]))
            {
                //text.setStyle(sf::Text::Underlined);
                text.setFillColor(sf::Color::Cyan);
                CurrentTree->Entries[i].worldLineBounds = text.getGlobalBounds();
            }

            Window.draw(text);

            if (CurrentTree->Entries[i].newline)
            {
                TextX = StartTextX;
                TextY += Spacing; 
            }
            else
            {
                TextX += text.getLocalBounds().width;
            }
        }
    }
}

internal void
DrawAvatar(sf::RenderWindow& Window, game_state* GameState, passenger_template* Passenger, int32 X, int32 Y, real32 Scale, real32 Alpha)
{
    // calculate head offset
    // Y offset is -height of head
    // X offset is +half body
    sf::FloatRect BodyBounds = Passenger->AvatarBodySprite->getLocalBounds();
    sf::FloatRect HeadBounds = Passenger->AvatarHeadSprite->getLocalBounds();
    int32 XOffset = Scale * (BodyBounds.width / 2 - HeadBounds.width / 2);
    int32 YOffset = Scale * -HeadBounds.height + 10; //10 is a fudge factor

    DrawSprite(Window, *Passenger->AvatarHeadSprite, X + XOffset, Y + YOffset, Scale, Alpha);    
    DrawSprite(Window, *Passenger->AvatarBodySprite, X, Y, Scale, Alpha);
}

// hard coded every 2nd day is night
internal bool32 IsNight(game_state* GameState)
{
    return ((GameState->CurrentDay+1) % 2 == 0);
}

internal void
DrawWrappedBackground(sf::RenderWindow& Window, game_state* GameState, sf::Sprite& Sprite, int32 XScrollValue, int32 Y, real32 Alpha)
{
    bool32 Night = IsNight(GameState);
    sf::FloatRect W = Sprite.getLocalBounds();
    DrawSprite(Window, Sprite, XScrollValue - W.width, Y, 1.f, Alpha, Night);
    DrawSprite(Window, Sprite, XScrollValue, Y, 1.f, Alpha, Night);

    if (GameState->GameMode == game_mode_driving)
    {
        GameState->BackgroundImageXScroll += DEFAULT_BUS_SPEED;
    }
    if (GameState->BackgroundImageXScroll > W.width)
    {
        GameState->BackgroundImageXScroll -= W.width;
    }
}

// we want them to end up at PASSENGER_ENTRY_X
// the bus is moving at DEFAULT_BUS_SPEED 
// 30fps
// PASSENGER_ENTRY_X / (60*DEFAULT_BUS_SPEED) is how many seconds in advance
internal void DrawBusStop(sf::RenderWindow& Window, game_state* GameState, real32 Alpha)
{
    real32 TimeInAdvance = PASSENGER_ENTRY_X / (30 * DEFAULT_BUS_SPEED);
    if (GameState->CurrentRoute.TimeToNextStop < TimeInAdvance && GameState->GameMode == game_mode_driving)
    {
        GameState->BusStopXScroll += DEFAULT_BUS_SPEED;

        for (uint32 i=0;i<GameState->CurrentRoute.Stops[GameState->CurrentRoute.NextStop].Template->NumPassengers; ++i)
        {
            DrawSprite(Window, *GameState->Waiting.s, GameState->BusStopXScroll + i * 30, 800,0.5f, Alpha, IsNight(GameState));
        }
    }
}

internal void
DrawMeters(sf::RenderWindow& Window, game_state* GameState, real32 DeltaTime, real32 ScreenAlpha)
{
    sf::Text text;
    text.setFont(*GameState->Font);
    text.setString("Energy");
    text.setCharacterSize(30);
    text.setPosition(60, 50);
    text.setFillColor(sf::Color(255,255,255,255*ScreenAlpha));
    Window.draw(text);

    uint32 MeterLength = MAX_ENERGY_METER_WIDTH * GameState->DriverMood;
    sf::RectangleShape EnergyMeter(sf::Vector2f(MeterLength,50));
    EnergyMeter.setPosition(60,100);

    if (GameState->FlashMeterPositive && GameState->FlashLength > 0.f)
    {
        EnergyMeter.setFillColor(sf::Color(10,200,10,255*ScreenAlpha));
    }
    else if (GameState->FlashMeterNegative && GameState->FlashLength > 0.f)
    {
        EnergyMeter.setFillColor(sf::Color(200,10,10, 255*ScreenAlpha));
    }
    else
    {
        EnergyMeter.setFillColor(sf::Color(255,255,255,255*ScreenAlpha));
    }

    if (GameState->FlashLength > 0.f)
    {
        GameState->FlashLength -= DeltaTime;
    }

    if (GameState->FlashLength < 0.f)
    {
        GameState->FlashLength = 0.f;
        GameState->FlashMeterPositive = false;
        GameState->FlashMeterNegative = false;
    }

    Window.draw(EnergyMeter);

    text.setString("Time");
    text.setCharacterSize(30);
    text.setPosition(60, 150);
    text.setFillColor(sf::Color(255,255,255,255*ScreenAlpha));
    Window.draw(text);

    char TimeBuffer[256];
    sprintf_s(TimeBuffer,256, "%.0f",GameState->CurrentRoute.ElapsedTime);
    text.setString(TimeBuffer);
    text.setCharacterSize(30);
    text.setPosition(60, 200);
    text.setFillColor(sf::Color(255,255,255,255*ScreenAlpha));
    Window.draw(text);
}


internal void
DrawDebugText(sf::RenderWindow& Window, game_state* GameState)
{
    // sf::Text text;
    // text.setFont(*Font);
    // text.setString(CurrentTree->Entries[i].line);
    // text.setCharacterSize(20);
    // text.setPosition(TextX, TextY);

    // if (IsDialogueLineBranchTo(Branches, CurrentTree->Entries[i]))
    // {
    //     //text.setStyle(sf::Text::Underlined);
    //     text.setFillColor(sf::Color::Cyan);
    //     CurrentTree->Entries[i].worldLineBounds = text.getGlobalBounds();
    // }

    // Window.draw(text);
}

struct transition_values
{
    real32 Scale;
	uint32 X;
    uint32 Y;
    real32 Alpha;
};

internal transition_values
CalculateTransitionValues(game_state* GameState)
{
    transition_values Result = {};
	Result.X = PASSENGER_ENTRY_X;
	Result.Y = 600;
	Result.Scale = 0.6f;
    Result.Alpha = 1.f;


    real32 BoardTime = GameState->BoardingPassenger.Template->BoardTime;
    real32 Percentage = (BoardTime - GameState->BoardingStateCountdown) / BoardTime;
    real32 P3 = Percentage * 3;

    switch (GameState->BoardingState)
    {
        case boarding_state_dialogue:
            Result.Scale = 0.6f;
            Result.Y = 600;
            break;

        case boarding_state_eval:
            Result.Scale = 0.f;
            Result.Y = 600;
            break;

        case boarding_state_in:
			Result.Scale = 0.6 * ((uint32)P3 / 3.f);
			Result.Y = 600;
            Result.Alpha = 1.f * ((uint32)P3 / 3.f);
            break;

        case boarding_state_out:
        {
            bool32 GoingIntoBus = GameState->CurrentActiveBranches->PassengerIsIn;

            if (GoingIntoBus)
            {
                Result.X = PASSENGER_ENTRY_X + (1000 * (uint32)P3 / 3.f);
				Result.Scale = 0.6f;
            }
            else
            {
    			Result.Scale = 0.6 - 0.6 * ((uint32)P3 / 3.f);
                Result.Y = 600;
                Result.Alpha = 1.f - 1.f * ((uint32)P3 / 3.f);
            }
            break;
        }

        default:
            break;
    }

    return Result;
}

struct StateEffectPushNotifications
{
    char* effectName;
    char* dialogue;
};

static StateEffectPushNotifications Notifications[MAX_TOTAL_STATE_EFFECTS] = {
    {"uni_negative", "She will remember that"},
    {"uni_positive", "She will remember that"}
};

internal char* 
GetStateEffectDialogue(game_state* GameState, const char* effectname)
{
    for (uint32 i=0;i<GameState->NumPendingStateEffects;++i)
    {
		if (!strcmp(Notifications[i].effectName, effectname))
		{
			return Notifications[i].dialogue;
		}
    }
    return "";
}

internal void
DrawStateEffects(sf::RenderWindow& Window, game_state* GameState, real32 DeltaTime)
{
    // this assumes sonly 1 state effect per frame added!
    if (GameState->NumPendingStateEffects != GameState->NumStateEffects)
    {
        GameState->StateEffectText = GetStateEffectDialogue(GameState, GameState->PendingStateEffects[GameState->NumPendingStateEffects-1]);
        GameState->StateEffectTextCountdown = STATE_EFFECT_COUNTDOWN;
        GameState->StateEffects[GameState->NumStateEffects++] = GameState->PendingStateEffects[GameState->NumPendingStateEffects-1];
    }

    if (GameState->StateEffectTextCountdown > 0.f)
    {
        GameState->StateEffectTextCountdown -= DeltaTime;

        sf::Text text;
        text.setFont(*GameState->Font);
        text.setString(GameState->StateEffectText);
        text.setCharacterSize(40);
        text.setPosition(1450,900);

        text.setFillColor(sf::Color(160,50,50));
        Window.draw(text);
          
    }

}

internal void
Render(sf::RenderWindow& Window, game_state* GameState, real32 DeltaTime)
{
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape))
    {
        Window.close();
    }
    if (GameState->GameMode == game_mode_title)
    {
        Window.clear();
        DrawSprite(Window, *GameState->Title.s, 0,0);
    }
    else if (GameState->GameMode == game_mode_offwork || GameState->GameMode == game_mode_transition_to_driving)
    {
        real32 ScreenAlpha = 1.f;
        if (GameState->GameMode == game_mode_transition_to_driving)
        {
            ScreenAlpha = GameState->TransitionCountdown / DEFAULT_COUNTDOWN_TO_DRIVING;
            if (ScreenAlpha < 0.f)
                ScreenAlpha = 0.f;
        }

        if (GameState->StoryState.CurrentStoryBg.s != nullptr)
        {
            Window.clear(sf::Color(51,58,58,255));
            DrawSprite(Window, *GameState->StoryState.CurrentStoryBg.s, 800, 100, 1.f, ScreenAlpha);
            DrawDialogue(Window, GameState->Font, &GameState->StoryState.DialogTree, &GameState->StoryState.ActiveBranches, 100, 500, 40, 45, ScreenAlpha);    
       }
    }
    else
    {
        real32 ScreenAlpha = 1.f;
        if (GameState->GameMode == game_mode_transition_to_offwork)
        {
            ScreenAlpha = GameState->TransitionCountdown / DEFAULT_COUNTDOWN_TO_OFFWORK;
            if (ScreenAlpha < 0.f)
                ScreenAlpha = 0.f;
        }

        DrawWrappedBackground(Window, GameState, *GameState->BackgroundImage.s, GameState->BackgroundImageXScroll, 0, ScreenAlpha);
        DrawBusStop(Window, GameState, ScreenAlpha);    

        DrawSprite(Window, *GameState->BusImage.s, 0, 0, 1.f, ScreenAlpha);

        DrawSprite(Window, *GameState->DialogueBackground.s, 0, 0, 0.6f, ScreenAlpha);
        DrawMeters(Window, GameState, DeltaTime, ScreenAlpha);

        DrawSprite(Window, *GameState->Schematic.s, 800,0, 1.f, ScreenAlpha);

        DrawSprite(Window, *GameState->DialogueBackground.s, 1400, 500, 1.f, ScreenAlpha);

        if (GameState->GameMode == game_mode_boarding)
        {            
            passenger_template* Passenger = GameState->BoardingPassenger.Template;
            if (Passenger != nullptr && (GameState->BoardingState == boarding_state_dialogue || GameState->BoardingState == boarding_state_out))
            {
                dialogue_tree* CurrentTree = &Passenger->DialogueTree;
                active_dialogue_branches* Branches = &GameState->BoardingPassenger.ActiveBranches;
				if (Branches->NextDialogueBranchIndex > 0)
				{
					DrawDialogue(Window, GameState->Font, CurrentTree, Branches, 1450, 550, 25, 30, 1.f);
				}
            }
		    DrawStateEffects(Window, GameState, DeltaTime);

			transition_values Values = CalculateTransitionValues(GameState);
            DrawAvatar(Window, GameState, GameState->BoardingPassenger.Template, Values.X, Values.Y, Values.Scale, Values.Alpha);
        }
        else
        {
            DrawSprite(Window, *GameState->DoorImage.s, 900,300, 1.f, ScreenAlpha);
        }
    }
}

internal void
StartDialog(game_state* GameState, dialogue_tree* Tree, active_dialogue_branches* Branches)
{
    memset(Branches, 0, sizeof(active_dialogue_branches));
    PushDialogueBranch(Branches, "<start>");
    GameState->CurrentActiveBranches = Branches;
    GameState->CurrentDialogTree = Tree;    
}

internal void
LoadPassengerAvatars(passenger_list* PassengerList)
{
    for (uint32 i=0;i<PassengerList->PassengerDbCount;++i)
    {
        // this is all pretty gross..
        char filename[1024];
        strcpy_s(filename, 1024, "../data/chars/");
        strcat_s(filename, 1016, PassengerList->PassengerDb[i].AvatarBody);
        strcat_s(filename, 1000, ".png");
        PassengerList->PassengerDb[i].AvatarBodyTexture = new sf::Texture;
        PassengerList->PassengerDb[i].AvatarBodyTexture->loadFromFile(filename);

        PassengerList->PassengerDb[i].AvatarBodySprite = new sf::Sprite;
        PassengerList->PassengerDb[i].AvatarBodySprite->setTexture(*PassengerList->PassengerDb[i].AvatarBodyTexture);
    

        strcpy_s(filename, 1024, "../data/chars/");
        strcat_s(filename, 1016, PassengerList->PassengerDb[i].AvatarHead);
        strcat_s(filename, 1000, ".png");
        PassengerList->PassengerDb[i].AvatarHeadTexture = new sf::Texture;
        PassengerList->PassengerDb[i].AvatarHeadTexture->loadFromFile(filename);

        PassengerList->PassengerDb[i].AvatarHeadSprite = new sf::Sprite;
        PassengerList->PassengerDb[i].AvatarHeadSprite->setTexture(*PassengerList->PassengerDb[i].AvatarHeadTexture);
    }
}

internal passenger_template*
SpawnRandomPassenger(game_state* GameState, bool32 Partier)
{
    if (Partier)
    {
        int32 num_passengers = GameState->NightPassengers.PassengerDbCount;
        return &GameState->NightPassengers.PassengerDb[rand() % num_passengers];
    }
    else
    {
        int32 num_passengers = GameState->GeneralPassengers.PassengerDbCount;
		int32 r = rand() % num_passengers;
        return &GameState->GeneralPassengers.PassengerDb[r];
    }
}

internal passenger_template*
TryFindCustomPassenger(game_state* GameState, const char* id)
{
    // try id/stateeffect for every state effect
    for (uint32 s=0;s<GameState->NumStateEffects;++s)
    {
		for (uint32 i=0;i<GameState->CustomPassengers.PassengerDbCount;++i)
		{
			char full_id[256];
			sprintf_s(full_id, 256, "%s/%s", id, GameState->StateEffects[s]);
			if (!strcmp(GameState->CustomPassengers.PassengerDb[i].Id , full_id))
				return &GameState->CustomPassengers.PassengerDb[i];
		}
    }

    for (uint32 i=0;i<GameState->CustomPassengers.PassengerDbCount;++i)
    {
        if (!strcmp(GameState->CustomPassengers.PassengerDb[i].Id , id))
            return &GameState->CustomPassengers.PassengerDb[i];
    }

	// SHIPPING THIS CANNOT BE HERE
    // for (uint32 i=0;i<GameState->GeneralPassengers.PassengerDbCount;++i)
    // {
    //     if (!strcmp(GameState->GeneralPassengers.PassengerDb[i].Id , id))
    //         return &GameState->GeneralPassengers.PassengerDb[i];
    // }
    // for (uint32 i=0;i<GameState->NightPassengers.PassengerDbCount;++i)
    // {
    //     if (!strcmp(GameState->NightPassengers.PassengerDb[i].Id , id))
    //         return &GameState->NightPassengers.PassengerDb[i];
    // }
return nullptr;
}

internal passenger_template*
SpawnPassenger(game_state* GameState, stop Stop)
{
    passenger_template* Result = nullptr;

    uint32 PassengerIndex = Stop.NumPassengersHandled;
    const char* id = Stop.Template->PassengerIds[PassengerIndex].id;
    Result = TryFindCustomPassenger(GameState, id);

    bool32 Partier = !strcmp(id, "randompartier");

    if (Result == nullptr)
        Result = SpawnRandomPassenger(GameState, Partier);

    return Result;
}

internal void
StartPassengerBoard(game_state* GameState, passenger_template* P)
{
    GameState->BoardingPassenger.Template = P;
}

internal void
PushStateEffects(game_state* GameState, dialogue_entry Entry)
{
	if (strlen(Entry.stateeffect) > 0)
	{
		GameState->PendingStateEffects[GameState->NumPendingStateEffects++] = (char*)Entry.stateeffect;
	}
}

internal void
ApplyEnergyEffects(game_state* GameState, dialogue_entry Entry)
{
    bool32 minorDecrease = !strcmp(Entry.energyeffect, "-");
    bool32 minorIncrease = !strcmp(Entry.energyeffect, "+");
    bool32 majorDecrease = !strcmp(Entry.energyeffect, "--");
    bool32 majorIncrease = !strcmp(Entry.energyeffect, "++");

    if (minorDecrease)
    {
        GameState->NegativeAudio->play();
        GameState->DriverMood -= 0.1f;
        GameState->BoardingPassenger.Mood -= 0.1f;
        GameState->FlashMeterNegative = true;
        GameState->FlashLength = 0.2f;
    }
    if (minorIncrease)
    {
        GameState->PositiveAudio->play();
        GameState->DriverMood += 0.1f;
        GameState->BoardingPassenger.Mood += 0.1f;
        GameState->FlashMeterPositive = true;
        GameState->FlashLength = 0.2f;
    }
    if (majorDecrease)
    {
        GameState->NegativeAudio->play();
        GameState->DriverMood -= 0.3f;
        GameState->BoardingPassenger.Mood -= 0.3f;
        GameState->FlashMeterNegative = true;
        GameState->FlashLength = 0.5f;
    }
    if (majorIncrease)
    {
        GameState->PositiveAudio->play();
        GameState->DriverMood += 0.3f;
        GameState->BoardingPassenger.Mood += 0.3f;
        GameState->FlashMeterPositive = true;
        GameState->FlashLength = 0.5f;
    }

    if (GameState->DriverMood < 0.f)
        GameState->DriverMood = 0.f;
    if (GameState->DriverMood > 1.f)
        GameState->DriverMood = 1.f;

    if (GameState->BoardingPassenger.Mood < 0.f)
        GameState->BoardingPassenger.Mood = 0.f;
    if (GameState->BoardingPassenger.Mood > 1.f)
        GameState->BoardingPassenger.Mood = 1.f;
}

internal void
DialogueResponse(game_state* GameState, active_dialogue_branches* Branches, dialogue_tree* Tree, dialogue_entry Entry)
{
    PushDialogueBranch(Branches, Entry.branchto);
    HandleBranchToEnd(GameState, Tree, Branches);
    ApplyEnergyEffects(GameState, Entry);
    PushStateEffects(GameState, Entry);
}

internal void
HandleInput(sf::RenderWindow& Window, game_state* GameState)
{
    bool IsPressed = sf::Mouse::isButtonPressed(sf::Mouse::Left);
    sf::Vector2i Position = sf::Mouse::getPosition(Window);

	if (GameState->CurrentDialogTree != nullptr)
	{
		for (uint32 i=0;i<GameState->CurrentDialogTree->NumEntries;++i)
		{
			if (IsDialogueLineBranchTo(GameState->CurrentActiveBranches, GameState->CurrentDialogTree->Entries[i]) && GameState->CurrentDialogTree->Entries[i].worldLineBounds.contains(Position.x, Position.y) && IsPressed)
			{
				DialogueResponse(GameState, GameState->CurrentActiveBranches, GameState->CurrentDialogTree, GameState->CurrentDialogTree->Entries[i]);
			}
		}
	}
}

internal void SwitchGameMode(game_state* GameState, game_mode NewMode)
{
    GameState->PendingGameMode = NewMode;
}

internal void
StartBoardingMode(game_state* GameState)
{
    GameState->DoorOpenAudio->play();
    GameState->CurrentRoute.Stops[GameState->CurrentRoute.NextStop].NumPassengersHandled = 0;
    GameState->BoardingState = boarding_state_eval;
}

internal void
RunBoardingLoop(game_state* GameState, real32 DeltaTime)
{
    switch (GameState->BoardingState)
    {
        case boarding_state_eval:
            if (GameState->CurrentRoute.Stops[GameState->CurrentRoute.NextStop].NumPassengersHandled < 
                    GameState->CurrentRoute.Stops[GameState->CurrentRoute.NextStop].Template->NumPassengers)
            {
                passenger_template* P = SpawnPassenger(GameState,GameState->CurrentRoute.Stops[GameState->CurrentRoute.NextStop]);

                StartPassengerBoard(GameState, P);
                GameState->BoardingState = boarding_state_in;
                GameState->BoardingStateCountdown = P->BoardTime;
            }
            else
            {
                GameState->DoorOpenAudio->play();
                if ((uint32)GameState->CurrentRoute.NextStop + 1 >= GameState->CurrentRoute.NumStops)
                {
                    SwitchGameMode(GameState, game_mode_transition_to_offwork);
                }
                else
                {
                    SwitchGameMode(GameState, game_mode_transition);
                }
            }
            break;

        case boarding_state_in:
            if (GameState->BoardingStateCountdown < 0.f)
            {
                StartDialog(GameState, &GameState->BoardingPassenger.Template->DialogueTree, &GameState->BoardingPassenger.ActiveBranches);
                GameState->BoardingState = boarding_state_dialogue;
            }
            GameState->BoardingStateCountdown -= DeltaTime;
            break;

        case boarding_state_dialogue:
            if (GameState->CurrentActiveBranches->DialogueIsDone)
            {
                GameState->BoardingState = boarding_state_out;
                GameState->BoardingStateCountdown = GameState->BoardingPassenger.Template->BoardTime;
            }
            break;

        case boarding_state_out:
            if (GameState->BoardingStateCountdown < 0.f)
            {
                ++GameState->CurrentRoute.Stops[GameState->CurrentRoute.NextStop].NumPassengersHandled;
                GameState->BoardingState = boarding_state_eval;
            }
            GameState->BoardingStateCountdown -= DeltaTime;
            break;

        default:
            break;
    }
}

internal void
StartDrivingMode(game_state* GameState)
{
    GameState->AmbientAudio->play();
    GameState->DriveStopAudio->play();
    GameState->CurrentRoute.NextStop++;
    GameState->CurrentRoute.TimeToNextStop = GameState->CurrentRoute.Stops[GameState->CurrentRoute.NextStop].DistanceToStop;
    GameState->BusStopXScroll = 0;
}

internal void
RunDrivingLoop(game_state* GameState, real32 DeltaTime)
{
    if (GameState->CurrentRoute.TimeToNextStop < 0.f)
    {
        SwitchGameMode(GameState, game_mode_boarding);
    }
    GameState->CurrentRoute.TimeToNextStop -= DeltaTime;
}

internal void
StartTransitionMode(game_state* GameState)
{
    GameState->TransitionCountdown = DEFAULT_COUNTDOWN_TO_BUS_MOVING;
}

internal void
RunTransitionLoop(game_state* GameState, real32 DeltaTime)
{
    GameState->TransitionCountdown -= DeltaTime;
    if (GameState->TransitionCountdown < 0.f)
    {
        SwitchGameMode(GameState, game_mode_driving);
    }
}

#define DISTRIBUTION_COUNT 8
internal uint32 PassengerDistribution[DISTRIBUTION_COUNT] = {1,2,4,8,8,4,2,1};

// internal uint32
// GenerateNumPassengers(uint32 CurrentStop, uint32 TotalStops)
// {
//     assert(TotalStops != 0);

//     real32 Factor = DISTRIBUTION_COUNT / TotalStops;
//     uint32 B = PassengerDistribution[(uint32)(Factor * CurrentStop)];
//     uint32 R = rand() % 3;
//     return (B + R > 0) ? B + R : 1;
// }

internal stop_template*
GetStopTemplateFromId(game_state* GameState, const char* id)
{
    for (uint32 i=0;i<GameState->Script.NumStopTemplates;++i)
    {
        if (!strcmp(GameState->Script.StopTemplates[i].id, id))
            return &GameState->Script.StopTemplates[i];
    }
    return nullptr;
}

internal route
GenerateRouteFromCurrentDay(game_state* GameState)
{
    route Result = {};
    // CurrentDay

    day* CurrentDay = &GameState->Script.Days[GameState->CurrentDay];

    Result.NumStops = CurrentDay->NumStops;
    Result.Stops = PushArray(&GameState->WorldArena, Result.NumStops, stop);

    for (uint32 i=0;i<Result.NumStops;++i)
    {
        stop_template* T = GetStopTemplateFromId(GameState, CurrentDay->StopIds[i].id);
        assert(T != nullptr);
        Result.Stops[i].Template = T;

        int32 R = rand() % 4 - 2;
        Result.Stops[i].DistanceToStop = 8.f + R;
        Result.Stops[i].NumPassengersHandled = 0;
    }

    Result.ElapsedTime = 0.f;
    Result.NextStop = -1;
    return Result;
}

internal route
GenerateRoute(game_state* GameState, uint32 DayLength)
{
    route Result = {};
    assert(0);
    return Result;

    // DEPRECATED IN FAVOUR OF DAYS
    // // my general idea is that the average time between stops is 8 +/- 2, so we take DayLength / 8
    // uint32 NumStops = DayLength / 8;

    // route Result = {};
    // Result.NumStops = NumStops;
    // Result.Stops = PushArray(&GameState->WorldArena, NumStops, stop);
    
    // for (uint32 i=0;i<NumStops;++i)
    // {
    //     stop* Stop = &Result.Stops[i];

    //     int32 R = rand() % 4 - 2;
    //     Stop->DistanceToStop = 8.f + R;
    //     Stop->NumGeneralPassengers = GenerateNumPassengers(i, NumStops);
    // }  
    // Result.NextStop = -1;

    // return Result;
}

internal void
StartTransitionToOffwork(game_state* GameState)
{
    GameState->TransitionCountdown = DEFAULT_COUNTDOWN_TO_OFFWORK;
}

internal void
RunTransitionToOffwork(game_state* GameState, real32 DeltaTime)
{
    if (GameState->TransitionCountdown < 0.f)
    {
        SwitchGameMode(GameState, game_mode_offwork);
    }
    GameState->TransitionCountdown -= DeltaTime;
}

internal void
StartTransitionToDriving(game_state* GameState)
{
    GameState->TransitionCountdown = DEFAULT_COUNTDOWN_TO_DRIVING;
}

internal void
RunTransitionToDriving(game_state* GameState, real32 DeltaTime)
{
    if (GameState->TransitionCountdown < 0.f)
    {
        GameState->CurrentPassengerCount = 0;
        GameState->CurrentDay++;
		if (GameState->CurrentDay >= GameState->Script.NumDays)
		{
			GameState->CurrentDay = 0;
		}
        GameState->CurrentRoute = GenerateRouteFromCurrentDay(GameState);
        SwitchGameMode(GameState, game_mode_driving);
    }
    GameState->TransitionCountdown -= DeltaTime;
}

internal void
StartMetaMode(game_memory* Memory, game_state* GameState)
{
    int32 Vignette = rand() % 2;
    char fileName[512];
    char* VignetteString = Vignette == 0 ? "day_kids" : "day_spouse";
    char* MoodString = GameState->DriverMood < .33f ? "sad" : (GameState->DriverMood < .66f ? "neutral" : "happy");

    sprintf_s(fileName,512,"../data/meta/%s_%s.png", VignetteString, MoodString);

    LoadSfSprite(&GameState->StoryState.CurrentStoryBg, fileName);

    sprintf_s(fileName, 512, "../data/meta/dialogue_%s_%s.json", VignetteString, MoodString);

    GameState->StoryState.DialogTree = LoadDialogue(Memory, GameState, fileName);
    StartDialog(GameState, &GameState->StoryState.DialogTree, &GameState->StoryState.ActiveBranches);
}

internal void 
RunMetaLoop(game_memory* Memory, game_state* GameState, real32 DeltaTime)
{
    if (GameState->CurrentActiveBranches->DialogueIsDone)
    {
        SwitchGameMode(GameState, game_mode_transition_to_driving);
        GameState->TransitionCountdown = DEFAULT_COUNTDOWN_TO_OFFWORK;
    }
}

internal void
RunTitleScreen(game_state* GameState)
{
    if (sf::Mouse::isButtonPressed(sf::Mouse::Left))
    {
        SwitchGameMode(GameState, game_mode_driving);
    }
}

internal void
RunRideLoop(game_memory* Memory, game_state* GameState, real32 DeltaTime)
{
    if (GameState->PendingGameMode != GameState->GameMode)
    {
        GameState->GameMode = GameState->PendingGameMode;
        switch (GameState->GameMode)
        {
            case game_mode_driving:
				StartDrivingMode(GameState);
                break;

            case game_mode_boarding:
                StartBoardingMode(GameState);
                break;

            case game_mode_transition:
                StartTransitionMode(GameState);
                break;

            case game_mode_transition_to_offwork:
                StartTransitionToOffwork(GameState);
                break;

            case game_mode_offwork:
                StartMetaMode(Memory, GameState);
                break;

            case game_mode_transition_to_driving:
                StartTransitionToDriving(GameState);
                break;

            default:
                break;            
        }
    }

    switch (GameState->GameMode)
    {
        case game_mode_title:
            RunTitleScreen(GameState);
            break;
        case game_mode_driving:
            RunDrivingLoop(GameState, DeltaTime);
            break;

        case game_mode_boarding:
            RunBoardingLoop(GameState, DeltaTime);
            break;

		case game_mode_transition:
			RunTransitionLoop(GameState, DeltaTime);
			break;

        case game_mode_transition_to_offwork:
            RunTransitionToOffwork(GameState, DeltaTime);
            break;

        case game_mode_offwork:
            RunMetaLoop(Memory, GameState, DeltaTime);
            break;

        case game_mode_transition_to_driving:
            RunTransitionToDriving(GameState, DeltaTime);
            break;

        default:
            break;
    }

    if (GameState->GameMode != game_mode_transition_to_offwork &&
        GameState->GameMode != game_mode_offwork && 
        GameState->GameMode != game_mode_transition_to_driving)
    {
        GameState->CurrentRoute.ElapsedTime += DeltaTime;
    }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    game_state* GameState = (game_state*)Memory->PermanentStorage;

    if (!Memory->IsInitialized)
    {
        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state), (uint8 *)Memory->PermanentStorage + sizeof(game_state));

        LoadScriptData(Memory, GameState);

        LoadInkFile(Memory, "../data/dialogue/test2.ink.json");

        LoadPassengerList(Memory, GameState, "../data/passengers.json", &GameState->GeneralPassengers);
        LoadPassengerAvatars(&GameState->GeneralPassengers);

        LoadPassengerList(Memory, GameState, "../data/nightpassengers.json", &GameState->NightPassengers);
        LoadPassengerAvatars(&GameState->NightPassengers);

        LoadPassengerList(Memory, GameState, "../data/scripted_passengers.json", &GameState->CustomPassengers);
        LoadPassengerAvatars(&GameState->CustomPassengers);

        GameState->CurrentPassengerCount = 0;
        GameState->CurrentDay = 0;
        GameState->CurrentRoute = GenerateRouteFromCurrentDay(GameState);
        GameState->DriverMood = 0.4f;

        // SHIPPING title screen
        SwitchGameMode(GameState, game_mode_title);
        //SwitchGameMode(GameState, game_mode_driving);

        LoadSfSprite(&GameState->BusImage, "../data/bus.png");
        LoadSfSprite(&GameState->DoorImage, "../data/door.png");
        LoadSfSprite(&GameState->BackgroundImage, "../data/scroll.png");
        LoadSfSprite(&GameState->DialogueBackground, "../data/dialogue.png");
        LoadSfSprite(&GameState->Schematic, "../data/layout.png");
        LoadSfSprite(&GameState->Waiting, "../data/silhouette.png");
        LoadSfSprite(&GameState->Title, "../data/title.png");

        GameState->Font = new sf::Font;
        GameState->Font->loadFromFile("../data/GoodbyeMyLover.ttf");

        GameState->AmbientAudioBuffer = new sf::SoundBuffer;
        GameState->AmbientAudioBuffer->loadFromFile("../data/audio/ambience.wav");

        GameState->DriveStopBuffer = new sf::SoundBuffer;
        GameState->DriveStopBuffer->loadFromFile("../data/audio/drivestop1.wav");

        GameState->DoorOpenAudioBuffer = new sf::SoundBuffer;
        GameState->DoorOpenAudioBuffer->loadFromFile("../data/audio/dooropen.wav");

        GameState->PositiveBuffer = new sf::SoundBuffer;
        GameState->PositiveBuffer->loadFromFile("../data/audio/positive.wav");

        GameState->NegativeAudioBuffer = new sf::SoundBuffer;
        GameState->NegativeAudioBuffer->loadFromFile("../data/audio/negative.wav");

        GameState->PositiveAudio = new sf::Sound;
        GameState->PositiveAudio->setBuffer(*GameState->PositiveBuffer);
        GameState->PositiveAudio->setVolume(40);

        GameState->NegativeAudio = new sf::Sound;
        GameState->NegativeAudio->setBuffer(*GameState->NegativeAudioBuffer);
        GameState->NegativeAudio->setVolume(40);

        GameState->AmbientAudio = new sf::Sound;
        GameState->AmbientAudio->setBuffer(*GameState->AmbientAudioBuffer);
        GameState->AmbientAudio->setVolume(20);
        GameState->AmbientAudio->setLoop(true);

        GameState->DriveStopAudio = new sf::Sound;
        GameState->DriveStopAudio->setBuffer(*GameState->DriveStopBuffer);
        GameState->DriveStopAudio->setVolume(40);

        GameState->DoorOpenAudio = new sf::Sound;
        GameState->DoorOpenAudio->setBuffer(*GameState->DoorOpenAudioBuffer);

        Memory->IsInitialized = true;

		srand(time(NULL));
    }

    RunRideLoop(Memory, GameState, DeltaTime);

    Render(Window, GameState, DeltaTime);
    HandleInput(Window, GameState);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    // game_state* GameState = (game_state*)Memory->PermanentStorage;
    // GameOutputSound(GameState, SoundBuffer, 400);
}

