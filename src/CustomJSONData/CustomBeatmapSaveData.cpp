#include "CustomJSONData/CustomBeatmapSaveData.h"

void CustomJSONData::CustomBeatmapSaveData::ctor(List_1<BeatmapSaveData::EventData*>* events, 
                                                  List_1<BeatmapSaveData::NoteData*>* notes, 
                                                  List_1<BeatmapSaveData::WaypointData*>* waypoints, 
                                                  List_1<BeatmapSaveData::ObstacleData*>* obstacles,
                                                  BeatmapSaveData::SpecialEventKeywordFiltersData *specialEventsKeywordFilters) {
    this->events = events;
    this->notes = notes;
    this->waypoints = waypoints;
    this->obstacles = obstacles;
    this->specialEventsKeywordFilters = specialEventsKeywordFilters;
}

DEFINE_CLASS(CustomJSONData::CustomBeatmapSaveData);

void CustomJSONData::CustomBeatmapSaveData_NoteData::ctor(float time, int lineIndex, NoteLineLayer lineLayer, BeatmapSaveData::NoteType type, NoteCutDirection cutDirection) {
    this->time = time;
    this->lineIndex = lineIndex;
    this->lineLayer = lineLayer;
    this->type = type;
    this->cutDirection = cutDirection;
}

DEFINE_CLASS(CustomJSONData::CustomBeatmapSaveData_NoteData);

void CustomJSONData::CustomBeatmapSaveData_ObstacleData::ctor(float time, int lineIndex, ObstacleType type, float duration, int width) {
    this->time = time;
    this->lineIndex = lineIndex;
    this->type = type;
    this->duration = duration;
    this->width = width;
}

DEFINE_CLASS(CustomJSONData::CustomBeatmapSaveData_ObstacleData);

void CustomJSONData::CustomBeatmapSaveData_EventData::ctor(float time, BeatmapSaveData::BeatmapEventType type, int value) {
    this->time = time;
    this->type = type;
    this->value = value;
}

DEFINE_CLASS(CustomJSONData::CustomBeatmapSaveData_EventData);