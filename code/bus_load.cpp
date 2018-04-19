void
LoadInkFile(game_memory* Memory, const char* fileName)
{
    // WIP
    // debug_read_file_result Result = Memory->DEBUGPlatformReadEntireFile(nullptr, (char*)fileName);
    // if (Result.ContentsSize == 0)
    // {
    //     return;
    // }
    // struct json_value_s* root = json_parse(Result.Contents, Result.ContentsSize);
    // assert(root->type == json_type_object);
    // struct json_object_s* object = (struct json_object_s*)root->payload;
    
}

internal const char*
GetStringAndAdvanceJson(json_object_element_s** e)
{
    const char* Result = ((json_string_s*)(*e)->value->payload)->string;
    *e = (*e)->next;
    return Result;
}

internal dialogue_tree
LoadDialogue(game_memory* Memory, game_state* GameState, const char* fileName)
{
    dialogue_tree Return = {};

	debug_read_file_result Result = Memory->DEBUGPlatformReadEntireFile(nullptr, (char*)fileName);
	if (Result.ContentsSize == 0)
		return Return;

    struct json_value_s* root = json_parse(Result.Contents, Result.ContentsSize);
    struct json_object_s* object = (struct json_object_s*)root->payload;

    struct json_object_element_s* a = object->start;
    struct json_value_s* a_value = a->value;
    struct json_array_s* dialogue_array = (struct json_array_s*)a_value->payload;
    const int32 line_count = dialogue_array->length;

    Return.NumEntries = line_count;
    Return.Entries = PushArray(&GameState->WorldArena, line_count, dialogue_entry);

    struct json_array_element_s* elem = dialogue_array->start;

    int i=0;
    while (elem != nullptr)
    {
        json_object_s* object = (json_object_s*)elem->value->payload;
        json_object_element_s* e = (json_object_element_s*)object->start;

        const char* branch = GetStringAndAdvanceJson(&e);
        const char* line = GetStringAndAdvanceJson(&e);
        const char* newline_s = GetStringAndAdvanceJson(&e);
        bool32 newline = strcmp(newline_s,"0");
        const char* branchto = GetStringAndAdvanceJson(&e);
        const char* energyeffect = GetStringAndAdvanceJson(&e);
        const char* stateeffect = GetStringAndAdvanceJson(&e);

        Return.Entries[i].branch = branch;
        Return.Entries[i].line = line;
        Return.Entries[i].newline = newline;
        Return.Entries[i].branchto = branchto;
        Return.Entries[i].energyeffect = energyeffect;
        Return.Entries[i].stateeffect = stateeffect;
        
        elem = elem->next;
        ++i;
    }

    return Return;
}

internal void
LoadScriptData(game_memory* Memory, game_state* GameState)
{
    {
        // load days
        debug_read_file_result StopsResult = Memory->DEBUGPlatformReadEntireFile(nullptr, "../data/script/stops.json");
        struct json_value_s* root = json_parse(StopsResult.Contents, StopsResult.ContentsSize);
        struct json_object_s* object = (struct json_object_s*)root->payload;
        struct json_object_element_s* a = object->start;
        struct json_value_s* a_value = a->value;
        struct json_array_s* stop_template_array = (struct json_array_s*)a_value->payload;
        const int32 stop_template_count = stop_template_array->length;
        GameState->Script.NumStopTemplates = stop_template_count;
        GameState->Script.StopTemplates = PushArray(&GameState->WorldArena, stop_template_count, stop_template);

        struct json_array_element_s* elem = stop_template_array->start;
        if (elem == nullptr) return;

        int i=0;
        while (elem != nullptr)
        {
            json_object_s* o = (json_object_s*)elem->value->payload;
            json_object_element_s* e = (json_object_element_s*)o->start;
        	json_array_s* passengerarray = (json_array_s*)e->value->payload;
            const int32 passenger_count = passengerarray->length;
            GameState->Script.StopTemplates[i].NumPassengers = passenger_count;
            GameState->Script.StopTemplates[i].PassengerIds = PushArray(&GameState->WorldArena, passenger_count, string_id);

            GameState->Script.StopTemplates[i].id = e->name->string;

    		// parse out of passengerarray; this will give us our list of passengers for each stop
			struct json_array_element_s* passenger = (struct json_array_element_s*)passengerarray->start;
            int j=0;
            while (passenger != nullptr)
            {
                json_object_s* o = (json_object_s*)passenger->value->payload;
                GameState->Script.StopTemplates[i].PassengerIds[j].id = ((json_string_s*)o)->string;

                passenger = passenger->next;
                ++j;
            }

            elem = elem->next;
            ++i;
        }
    }

    {
        // load script
        debug_read_file_result ScriptResult = Memory->DEBUGPlatformReadEntireFile(nullptr, "../data/script/script.json");
        struct json_value_s* root = json_parse(ScriptResult.Contents, ScriptResult.ContentsSize);
        struct json_object_s* object = (struct json_object_s*)root->payload;
        struct json_object_element_s* a = object->start;
        struct json_value_s* a_value = a->value;
        struct json_array_s* day_array = (struct json_array_s*)a_value->payload;
        const int32 day_count = day_array->length;
        GameState->Script.NumDays = day_count;
        GameState->Script.Days = PushArray(&GameState->WorldArena, day_count, day);

        struct json_array_element_s* elem = day_array->start;
        if (elem == nullptr) return;

        int i=0;
        while (elem != nullptr)
        {
            json_object_s* o = (json_object_s*)elem->value->payload;
            json_object_element_s* e = (json_object_element_s*)o->start;
            json_array_s* daysarray = (json_array_s*)e->value->payload;
            const int32 stops_count = daysarray->length;
            GameState->Script.Days[i].NumStops = stops_count;
            GameState->Script.Days[i].StopIds = PushArray(&GameState->WorldArena, stops_count, string_id);

            GameState->Script.Days[i].id = e->name->string;

            // parse out of passengerarray; this will give us our list of passengers for each stop
            struct json_array_element_s* day = (struct json_array_element_s*)daysarray->start;
            int j=0;
            while (day != nullptr)
            {
                json_object_s* o = (json_object_s*)day->value->payload;
                GameState->Script.Days[i].StopIds[j].id = ((json_string_s*)o)->string;

                day = day->next;
                ++j;
            }

            elem = elem->next;
            ++i;
        }    
    }
}

internal void
LoadPassengerList(game_memory* Memory, game_state* GameState, const char* fileName, passenger_list* OutputList)
{
    debug_read_file_result Result = Memory->DEBUGPlatformReadEntireFile(nullptr, (char*)fileName);
	struct json_value_s* root = json_parse(Result.Contents, Result.ContentsSize);
	struct json_object_s* object = (struct json_object_s*)root->payload;

	struct json_object_element_s* a = object->start;
	struct json_value_s* a_value = a->value;
	struct json_array_s* passenger_array = (struct json_array_s*)a_value->payload;
    const int32 passenger_count = passenger_array->length;
    OutputList->PassengerDbCount = passenger_count;
    OutputList->PassengerDb = PushArray(&GameState->WorldArena, passenger_count, passenger_template);

	struct json_array_element_s* elem = passenger_array->start;
    if (elem == nullptr) return;

	int i=0;
    while (elem != nullptr)
	{
        json_object_s* object = (json_object_s*)elem->value->payload;
        json_object_element_s* e = (json_object_element_s*)object->start;


        // TODO - use GetAndAvanceJson
        const char* id = GetStringAndAdvanceJson(&e);
        const char* charid = GetStringAndAdvanceJson(&e);
        const char* dialog_identifier = GetStringAndAdvanceJson(&e);
        const char* avatar_body = GetStringAndAdvanceJson(&e);
        const char* avatar_head = GetStringAndAdvanceJson(&e);
        const char* start_mood_s = GetStringAndAdvanceJson(&e);
        real32 start_mood = atof(start_mood_s);
        const char* board_time_s = GetStringAndAdvanceJson(&e);
        real32 board_time = atof(board_time_s);

        OutputList->PassengerDb[i].Id = id;
        OutputList->PassengerDb[i].CharId = charid;
        OutputList->PassengerDb[i].AvatarBody = avatar_body;
        OutputList->PassengerDb[i].AvatarHead = avatar_head;
        OutputList->PassengerDb[i].DialogIdentifier = dialog_identifier;
        OutputList->PassengerDb[i].StartMood = start_mood;
        OutputList->PassengerDb[i].BoardTime = board_time;

        if (strlen(OutputList->PassengerDb[i].DialogIdentifier) > 0)
        {
            // this is all pretty gross..
            char filename[1024];
            strcpy_s(filename, 1024, "../data/dialogue/");
            strcat_s(filename, 1016, OutputList->PassengerDb[i].DialogIdentifier);
            strcat_s(filename, 1000, ".json");

            OutputList->PassengerDb[i].DialogueTree = LoadDialogue(Memory, GameState, filename);
        }
        ++i;
        elem = elem->next;
	}
}