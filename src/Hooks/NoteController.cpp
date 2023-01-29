#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/MirroredGameNoteController.hpp"
#include "GlobalNamespace/NoteFloorMovement.hpp"
#include "GlobalNamespace/NoteJump.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/NoteMovement.hpp"
#include "GlobalNamespace/BaseNoteVisuals.hpp"
#include "GlobalNamespace/CutoutAnimateEffect.hpp"
#include "GlobalNamespace/CutoutEffect.hpp"
#include "GlobalNamespace/DisappearingArrowControllerBase_1.hpp"
#include "GlobalNamespace/GameNoteController.hpp"
#include "GlobalNamespace/BombNoteController.hpp"
#include "GlobalNamespace/ConditionalMaterialSwitcher.hpp"
#include "GlobalNamespace/BoxCuttableBySaber.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/GameObject.hpp"

#include "NEConfig.h"
#include "NEUtils.hpp"
#include "Animation/AnimationHelper.h"
#include "Animation/ParentObject.h"
#include "tracks/shared/TimeSourceHelper.h"
#include "AssociatedData.h"
#include "NEHooks.h"
#include "NECaches.h"
#include "custom-json-data/shared/CustomBeatmapData.h"
#include "sombrero/shared/linq_functional.hpp"
#include "GlobalNamespace/BeatmapObjectManager.hpp"

using namespace GlobalNamespace;
using namespace UnityEngine;
using namespace TrackParenting;

BeatmapObjectAssociatedData *noteUpdateAD = nullptr;
TracksAD::TracksVector noteTracks;
std::unordered_map<std::string_view, std::unordered_set<NoteController*>> linkedNotes;
std::unordered_map<NoteController*, std::unordered_set<NoteController*>*> linkedLinkedNotes;

CutoutEffect* NECaches::GetCutout(GlobalNamespace::NoteControllerBase *nc, NECaches::NoteCache &noteCache) {
    CutoutEffect *&cutoutEffect = noteCache.cutoutEffect;
    if (!cutoutEffect) {
        noteCache.baseNoteVisuals = noteCache.baseNoteVisuals
                                    ?: nc->get_gameObject()->GetComponent<BaseNoteVisuals *>();
        CutoutAnimateEffect *cutoutAnimateEffect = noteCache.baseNoteVisuals->cutoutAnimateEffect;
        ArrayW<CutoutEffect *> cuttoutEffects = cutoutAnimateEffect->cuttoutEffects;
        for (CutoutEffect *effect: cuttoutEffects) {
            if (effect->get_name() != u"NoteArrow") {
                cutoutEffect = effect;
                break;
            }
        }
    }

    return cutoutEffect;
}

GlobalNamespace::DisappearingArrowControllerBase_1<GlobalNamespace::GameNoteController *> *
NECaches::GetDisappearingArrowController(GlobalNamespace::GameNoteController *nc, NECaches::NoteCache &noteCache) {
    auto& disappearingArrowController = noteCache.disappearingArrowController;
    if (!disappearingArrowController) {
        disappearingArrowController = nc->get_gameObject()->GetComponent<DisappearingArrowControllerBase_1<GameNoteController *> *>();
    }

    return disappearingArrowController;
}

GlobalNamespace::DisappearingArrowControllerBase_1<GlobalNamespace::MirroredGameNoteController *> *
NECaches::GetDisappearingArrowController(GlobalNamespace::MirroredGameNoteController *nc,
                                         NECaches::NoteCache &noteCache) {
    auto& disappearingArrowController = noteCache.mirroredDisappearingArrowController;
    if (!disappearingArrowController) {
        disappearingArrowController = nc->get_gameObject()->GetComponent<DisappearingArrowControllerBase_1<MirroredGameNoteController *> *>();
    }

    return disappearingArrowController;
}

float noteTimeAdjust(float original, float jumpDuration) {
    if (noteTracks.empty())
        return original;

    auto time = NoodleExtensions::getTimeProp(noteTracks);

    if (time) {
        return *time * jumpDuration;
    }

    return original;
}

void NECaches::ClearNoteCaches() {
    NECaches::noteCache.clear();
    noteUpdateAD = nullptr;
    noteTracks.clear();
    linkedNotes.clear();
    linkedLinkedNotes.clear();
}

MAKE_HOOK_MATCH(NoteController_Init, &NoteController::Init, void,
                NoteController *self, GlobalNamespace::NoteData* noteData,
                float worldRotation, ::UnityEngine::Vector3 moveStartPos,
                ::UnityEngine::Vector3 moveEndPos, ::UnityEngine::Vector3 jumpEndPos,
                float moveDuration, float jumpDuration, float jumpGravity,
                float endRotation, float uniformScale, bool rotateTowardsPlayer, bool useRandomRotation) {
    NoteController_Init(self, noteData, worldRotation, moveStartPos, moveEndPos, jumpEndPos, moveDuration, jumpDuration,
                        jumpGravity, endRotation, uniformScale, rotateTowardsPlayer, useRandomRotation);

    if (!Hooks::isNoodleHookEnabled())
        return;

    static auto CustomKlass = classof(CustomJSONData::CustomNoteData *);

    if (noteData->klass != CustomKlass) return;

    auto *customNoteData =
            reinterpret_cast<CustomJSONData::CustomNoteData *>(noteData);

    Transform *transform = self->get_transform();
    transform->set_localScale(
            NEVector::Vector3::one()); // This is a fix for animation due to notes being
    // recycled

    if (!customNoteData->customData)
        return;
    BeatmapObjectAssociatedData &ad = getAD(customNoteData->customData);


    CRASH_UNLESS(ad.parsed);

    auto link = ad.objectData.link;
    if (link) {
        auto& list = linkedNotes[*link];
        list.emplace(self);
        linkedLinkedNotes[self] = &list;
    }

    // TRANSPILERS SUCK!
    auto flipYSide = ad.flipY ? *ad.flipY : customNoteData->flipYSide;

    if (flipYSide > 0.0f)
    {
        self->noteMovement->jump->yAvoidance = flipYSide * self->noteMovement->jump->yAvoidanceUp;
    }
    else
    {
        self->noteMovement->jump->yAvoidance = flipYSide * self->noteMovement->jump->yAvoidanceDown;
    }

    auto& noteCache = NECaches::getNoteCache(self);

    ArrayW<ConditionalMaterialSwitcher *>& materialSwitchers = noteCache.conditionalMaterialSwitchers;
    if (!materialSwitchers) {
         materialSwitchers = self->GetComponentsInChildren<ConditionalMaterialSwitcher *>();
    }

    for (auto *materialSwitcher: materialSwitchers) {
        materialSwitcher->renderer->set_sharedMaterial(materialSwitcher->material0);
    }
    noteCache.dissolveEnabled = false;

    NoteJump *noteJump = self->noteMovement->jump;
    NoteFloorMovement *floorMovement = self->noteMovement->floorMovement;

    float zOffset = self->noteMovement->zOffset;
    moveStartPos.z += zOffset;
    moveEndPos.z += zOffset;
    jumpEndPos.z += zOffset;




    NEVector::Quaternion localRotation = NEVector::Quaternion::identity();
    if (ad.objectData.rotation || ad.objectData.localRotation) {
        if (ad.objectData.localRotation) {
            localRotation = *ad.objectData.localRotation;
        }

        if (ad.objectData.rotation) {
            NEVector::Quaternion worldRotationQuatnerion = *ad.objectData.rotation;

            NEVector::Quaternion inverseWorldRotation = NEVector::Quaternion::Inverse(worldRotationQuatnerion);
            noteJump->worldRotation = worldRotationQuatnerion;
            noteJump->inverseWorldRotation = inverseWorldRotation;
            floorMovement->worldRotation = worldRotationQuatnerion;
            floorMovement->inverseWorldRotation = inverseWorldRotation;

            worldRotationQuatnerion = worldRotationQuatnerion * localRotation;
            transform->set_localRotation(worldRotationQuatnerion);
        } else {
            transform->set_localRotation(NEVector::Quaternion(transform->get_localRotation()) * localRotation);
        }
    }
    auto const &tracks = TracksAD::getAD(customNoteData->customData).tracks;
    if (!tracks.empty()) {
        auto go = self->get_gameObject();
        for (auto &track: tracks) {
            track->AddGameObject(go);
        }
    }

    ad.endRotation = endRotation;
    ad.moveStartPos = moveStartPos;
    ad.moveEndPos = moveEndPos;
    ad.jumpEndPos = jumpEndPos;
    ad.worldRotation = self->get_worldRotation();
    ad.localRotation = localRotation;

    float num2 = jumpDuration * 0.5f;
    float startVerticalVelocity = jumpGravity * num2;
    float yOffset = (startVerticalVelocity * num2) - (jumpGravity * num2 * num2 * 0.5f);
    Vector3 noteOffset = jumpEndPos;
    noteOffset.z = 0;
    noteOffset.y += yOffset;
    ad.noteOffset = noteOffset;


    self->Update();
}

MAKE_HOOK_MATCH(NoteController_ManualUpdate, &NoteController::ManualUpdate, void,
                NoteController *self) {

    if (!Hooks::isNoodleHookEnabled())
        return NoteController_ManualUpdate(self);

    noteUpdateAD = nullptr;
    noteTracks.clear();

    static auto CustomKlass = classof(CustomJSONData::CustomNoteData *);

    if (self->noteData->klass != CustomKlass) return NoteController_ManualUpdate(self);

    auto *customNoteData =
        reinterpret_cast<CustomJSONData::CustomNoteData *>(self->noteData);
    if (!customNoteData->customData) {
        noteUpdateAD = nullptr;
        noteTracks.clear();
        return NoteController_ManualUpdate(self);
    }

    // TODO: Cache deserialized animation data
    // if (!customData.HasMember("_animation")) {
    //     NoteController_Update(self);
    //     return;
    // }

    BeatmapObjectAssociatedData &ad = getAD(customNoteData->customData);
    auto const& tracks = TracksAD::getAD(customNoteData->customData).tracks;

    noteUpdateAD = &ad;
    noteTracks = tracks;

    if (noteTracks.empty() && !ad.animationData.parsed) {
        return NoteController_ManualUpdate(self);
    }

    NoteJump *noteJump = self->noteMovement->jump;
    NoteFloorMovement *floorMovement = self->noteMovement->floorMovement;

    auto time = NoodleExtensions::getTimeProp(tracks);
    float normalTime;
    if (time)
    {
        normalTime = time.value();
    }
    else
    {
        float jumpDuration = noteJump->jumpDuration;
        float elapsedTime = TimeSourceHelper::getSongTime(noteJump->audioTimeSyncController) - (customNoteData->time - (jumpDuration * 0.5f));
        normalTime = elapsedTime / jumpDuration;
    }


    AnimationHelper::ObjectOffset offset = AnimationHelper::GetObjectOffset(
        ad.animationData, tracks, normalTime);

    if (offset.positionOffset.has_value()) {
        auto const& offsetPos = *offset.positionOffset;
        floorMovement->startPos = ad.moveStartPos + offsetPos;
        floorMovement->endPos = ad.moveEndPos + offsetPos;
        noteJump->startPos = ad.moveEndPos + offsetPos;
        noteJump->endPos = ad.jumpEndPos + offsetPos;
    }

    auto transform = self->get_transform();

    if (offset.scaleOffset.has_value()) {
        transform->set_localScale(*offset.scaleOffset);
    }

    if (offset.rotationOffset.has_value() ||
        offset.localRotationOffset.has_value()) {
        NEVector::Quaternion worldRotation = ad.worldRotation;
        NEVector::Quaternion localRotation = ad.localRotation;

        NEVector::Quaternion worldRotationQuaternion = worldRotation;
        if (offset.rotationOffset.has_value()) {
            worldRotationQuaternion =
                worldRotationQuaternion * *offset.rotationOffset;
            NEVector::Quaternion inverseWorldRotation =
                    NEVector::Quaternion::Inverse(worldRotationQuaternion);
            noteJump->worldRotation = worldRotationQuaternion;
            noteJump->inverseWorldRotation = inverseWorldRotation;
            floorMovement->worldRotation = worldRotationQuaternion;
            floorMovement->inverseWorldRotation = inverseWorldRotation;
        }

        worldRotationQuaternion = worldRotationQuaternion * localRotation;

        if (offset.localRotationOffset.has_value()) {
            worldRotationQuaternion =
                worldRotationQuaternion * *offset.localRotationOffset;
        }

        transform->set_localRotation(worldRotationQuaternion);
    }

    auto& noteCache = NECaches::getNoteCache(self);

    bool noteDissolveConfig = getNEConfig().enableNoteDissolve.GetValue();
    bool hasDissolveOffset = offset.dissolve.has_value() || offset.dissolveArrow.has_value();
    bool isDissolving = offset.dissolve.value_or(0) > 0 || offset.dissolveArrow.value_or(0) > 0;
    if (hasDissolveOffset && noteCache.dissolveEnabled != isDissolving && noteDissolveConfig) {
        ArrayW<ConditionalMaterialSwitcher *> materialSwitchers = noteCache.conditionalMaterialSwitchers;
        for (auto *materialSwitcher : materialSwitchers) {
            materialSwitcher->renderer->set_sharedMaterial(isDissolving ? materialSwitcher->material1 : materialSwitcher->material0);
        }
        noteCache.dissolveEnabled = isDissolving;
    }

    if (offset.dissolve.has_value()) {
        CutoutEffect * cutoutEffect = NECaches::GetCutout(self, noteCache);
        CRASH_UNLESS(cutoutEffect);

        if (noteDissolveConfig) {
            cutoutEffect->SetCutout(1 - *offset.dissolve);
        } else {
            cutoutEffect->SetCutout(*offset.dissolve >= 0 ? 0 : 1);
        }
    }

    static auto *gameNoteControllerClass = classof(GameNoteController *);
    static auto *bombNoteControllerClass = classof(BombNoteController *);

    if (il2cpp_functions::class_is_assignable_from(gameNoteControllerClass, self->klass)) {
        if (offset.dissolveArrow.has_value() && self->noteData->colorType != ColorType::None) {
            auto disappearingArrowController = NECaches::GetDisappearingArrowController((GameNoteController*) self, noteCache);
            if (noteDissolveConfig) {
                disappearingArrowController->SetArrowTransparency(*offset.dissolveArrow);
            } else {
                disappearingArrowController->SetArrowTransparency(*offset.dissolveArrow >= 0 ? 1 : 0);
            }
        }
    }

    if (il2cpp_functions::class_is_assignable_from(gameNoteControllerClass, self->klass) || il2cpp_functions::class_is_assignable_from(bombNoteControllerClass, self->klass)) {
        if (offset.cuttable.has_value()) {
            bool enabled = *offset.cuttable >= 1;

            if (self->klass == gameNoteControllerClass) {
                auto *gameNoteController = reinterpret_cast<GameNoteController *>(self);
                ArrayW<BoxCuttableBySaber *> bigCuttables = gameNoteController->bigCuttableBySaberList;
                for (auto bigCuttable: bigCuttables) {
                    if (bigCuttable->canBeCut != enabled) {
                        bigCuttable->set_canBeCut(enabled);
                    }
                }
                ArrayW<BoxCuttableBySaber *> smallCuttables = gameNoteController->smallCuttableBySaberList;
                for (auto smallCuttable: smallCuttables) {
                    if (smallCuttable->canBeCut != enabled) {
                        smallCuttable->set_canBeCut(enabled);
                    }
                }
            } else if (self->klass == bombNoteControllerClass) {
                auto *bombNoteController = reinterpret_cast<BombNoteController *>(self);
                CuttableBySaber *cuttable = bombNoteController->cuttableBySaber;
                if (cuttable->get_canBeCut() != enabled) {
                    cuttable->set_canBeCut(enabled);
                }
            }
        }
    }

    NoteController_ManualUpdate(self);

    // NoteJump.ManualUpdate will be the last place this is used after it was set in
    // NoteController.ManualUpdate. To make sure it doesn't interfere with future notes, it's set
    // back to null
    noteUpdateAD = nullptr;
    noteTracks.clear();
}

MAKE_HOOK_MATCH(NoteController_SendNoteWasCutEvent_LinkedNotes, &NoteController::SendNoteWasCutEvent, void,
                NoteController *self, ByRef<::GlobalNamespace::NoteCutInfo> noteCutInfo) {
    NoteController_SendNoteWasCutEvent_LinkedNotes(self, noteCutInfo);

    if (!Hooks::isNoodleHookEnabled())
        return;


    auto *customNoteData =
            reinterpret_cast<CustomJSONData::CustomNoteData *>(self->noteData);
    if (!customNoteData->customData) {
        return;
    }

    BeatmapObjectAssociatedData &ad = getAD(customNoteData->customData);

    auto link = ad.objectData.link;

    if (!link) return;

    auto& list = linkedNotes[*link];

    list.erase(self);
    linkedLinkedNotes.erase(self);

    auto cuts = list | Sombrero::Linq::Functional::Select([&](auto const& noteController) {
        return std::pair(noteController, NoteCutInfo(noteController->noteData, noteCutInfo->speedOK, noteCutInfo->directionOK, noteCutInfo->saberTypeOK,
                           noteCutInfo->wasCutTooSoon, noteCutInfo->saberSpeed, noteCutInfo->saberDir, noteCutInfo->saberType,
                           noteCutInfo->timeDeviation, noteCutInfo->cutDirDeviation, noteCutInfo->cutPoint, noteCutInfo->cutNormal, noteCutInfo->cutDistanceToCenter,
                           noteCutInfo->cutAngle, noteCutInfo->worldRotation, noteCutInfo->inverseWorldRotation, noteCutInfo->noteRotation, noteCutInfo->notePosition,
                           noteCutInfo->saberMovementData));
    }) | Sombrero::Linq::Functional::ToVector();

    for (auto const& note : list) {
        linkedLinkedNotes.erase(note);
    }
    list.clear();

    for (auto& [noteController, cutInfo] : cuts) {
        auto ref = ByRef<NoteCutInfo>(cutInfo);
        noteController->SendNoteWasCutEvent(ref);
    }
}
MAKE_HOOK_MATCH(BeatmapObjectManager_Despawn_LinkedNotes, static_cast<void (GlobalNamespace::BeatmapObjectManager::*)(::GlobalNamespace::NoteController*)>(&GlobalNamespace::BeatmapObjectManager::Despawn), void,
                BeatmapObjectManager* self, GlobalNamespace::NoteController* noteController) {
    BeatmapObjectManager_Despawn_LinkedNotes(self, noteController);

    if (!Hooks::isNoodleHookEnabled())
        return;


    auto linkedLinkedIt = linkedLinkedNotes.find(noteController);
    if (linkedLinkedIt != linkedLinkedNotes.end()) {
        linkedLinkedIt->second->erase(noteController);
        linkedLinkedNotes.erase(linkedLinkedIt);
    }
}

void InstallNoteControllerHooks(Logger &logger) {
    INSTALL_HOOK(logger, NoteController_Init);
    INSTALL_HOOK(logger, NoteController_ManualUpdate);

    INSTALL_HOOK(logger, NoteController_SendNoteWasCutEvent_LinkedNotes);
    INSTALL_HOOK(logger, BeatmapObjectManager_Despawn_LinkedNotes);
}

NEInstallHooks(InstallNoteControllerHooks);