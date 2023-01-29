#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "GlobalNamespace/BeatmapData.hpp"
#include "GlobalNamespace/BeatmapDataLoader.hpp"

#include "custom-json-data/shared/CustomBeatmapData.h"
#include "custom-json-data/shared/CustomBeatmapSaveDatav3.h"
#include "custom-json-data/shared/VList.h"
#include "pinkcore/shared/API.hpp"

#include "FakeNoteHelper.h"
#include "SceneTransitionHelper.hpp"
#include "NEHooks.h"
#include "NEJSON.h"

#include "songloader/shared/API.hpp"

#include "sombrero/shared/linq_functional.hpp"

#include "Constants.hpp"
using namespace System;
using namespace System::Collections::Generic;
using namespace GlobalNamespace;
using namespace CustomJSONData;
using namespace Sombrero::Linq;


// return true if fake
// subtracts from object count if fake
template<typename T>
static bool IsFake(T* o, bool v2) {
    auto const optData = o->customData;

    if (!optData || !optData->value) return false;

    rapidjson::Value const& customData = *optData->value;

    auto fake = NEJSON::ReadOptionalBool(customData, v2 ? NoodleExtensions::Constants::V2_FAKE_NOTE
                                                        : NoodleExtensions::Constants::INTERNAL_FAKE_NOTE);
    return fake.value_or(false);
}

template<typename U, typename T>
auto FakeCount(VList<T> list, bool v2) {
    int i = list.size();
    for (auto o : list) {
        auto note = il2cpp_utils::try_cast<U>(o);
        if (!note) continue;

        if(IsFake(*note, v2)) i--;
    }

    return i;
}

void HandleBasicInfo(CustomJSONData::CustomLevelInfoSaveData*, std::string const&, BeatmapSaveDataVersion3::BeatmapSaveData* beatmap, GlobalNamespace::BeatmapDataBasicInfo* ret) {

    auto customBeatmap = il2cpp_utils::try_cast<CustomBeatmapData>(beatmap);

    if (!customBeatmap) return;

    bool v2 = customBeatmap.value()->v2orEarlier;

    ret->cuttableNotesCount = FakeCount<v3::CustomBeatmapSaveData_ColorNoteData>(VList(beatmap->colorNotes), v2);
    ret->bombsCount = FakeCount<v3::CustomBeatmapSaveData_BombNoteData>(VList(beatmap->bombNotes), v2);
    ret->obstaclesCount = FakeCount<v3::CustomBeatmapSaveData_ObstacleData>(VList(beatmap->obstacles), v2);
}

MAKE_HOOK_MATCH(BeatmapDataLoader_GetBeatmapDataBasicInfoFromSaveData, &BeatmapDataLoader::GetBeatmapDataBasicInfoFromSaveData, GlobalNamespace::BeatmapDataBasicInfo*,
                BeatmapSaveDataVersion3::BeatmapSaveData* beatmapSaveData) {
    auto ret = BeatmapDataLoader_GetBeatmapDataBasicInfoFromSaveData(beatmapSaveData);

    auto customBeatmap = il2cpp_utils::try_cast<CustomBeatmapData>(beatmapSaveData);

    if (!customBeatmap) return ret;

    bool v2 = customBeatmap.value()->v2orEarlier;

    // Not necessary in v3
    if (!v2) return ret;

    ret->cuttableNotesCount = FakeCount<v3::CustomBeatmapSaveData_ColorNoteData>(VList(beatmapSaveData->colorNotes), v2);
    ret->bombsCount = FakeCount<v3::CustomBeatmapSaveData_BombNoteData>(VList(beatmapSaveData->bombNotes), v2);
    ret->obstaclesCount = FakeCount<v3::CustomBeatmapSaveData_ObstacleData>(VList(beatmapSaveData->obstacles), v2);

    return ret;
}

void InstallBeatmapDataHooks(Logger &logger) {
    // force CJD to be first
    Modloader::requireMod("CustomJSONData");
    RuntimeSongLoader::API::AddBeatmapDataBasicInfoLoadedEvent(HandleBasicInfo);
    INSTALL_HOOK(logger, BeatmapDataLoader_GetBeatmapDataBasicInfoFromSaveData)

}
NEInstallHooks(InstallBeatmapDataHooks);