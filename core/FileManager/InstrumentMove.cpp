#include "InstrumentMove.h"

#include "VoiceStreamManager.h"
#include "fileManager.h"
#include "fileManagerDefs.h"
#include "mtAudioEngine.h"

#include <algorithm>
#include <cstring>

namespace
{
// workspace file numbers are slot + 1 (1..48); temporary names use oldSlot + kTempNumberBase
constexpr uint8_t kTempNumberBase = 51;
static_assert(kSampleInstrumentsCount + kTempNumberBase <= 99, "temp file numbers need 2 digits");

const char* const kWorkspaceFormats[] = {
    cWorkspaceInstrumentFileFormatNew,
    cWorkspaceInstrumentFileFormatOld,
    cWorkspaceSamplesFilesFormat,
};

uint32_t instrumentRamFrames(uint8_t idx)
{
    const strSampleMemory& mem = mtProject.sampleMemory[idx];
    if (mem.isStreamed)
    {
        return mem.ramFrames;
    }
    return mem.ramFrames > 0 ? mem.ramFrames : mtProject.instrument[idx].sample.length;
}

// int16 words the slot occupies in the sample bank
uint32_t instrumentFootprint(uint8_t idx, bool stereo)
{
    if (!mtProject.instrument[idx].isActive)
    {
        return 0;
    }
    return instrumentRamFrames(idx) * (stereo ? 2 : 1);
}

void setInstrumentBaseAddress(uint8_t idx, int16_t* base, bool stereo)
{
    mtProject.instrument[idx].sample.address = base;
    mtProject.sampleMemory[idx].address[strSampleMemory::Channel::kLeft] = base;
    mtProject.sampleMemory[idx].address[strSampleMemory::Channel::kRight] =
      stereo ? base + instrumentRamFrames(idx) : base;
}

void workspaceInstrumentPath(char* out, size_t len, uint8_t idx)
{
    snprintf(out, len, cWorkspaceInstrumentFileFormatNew, idx + 1);
}
} // namespace

void InstrumentMove::update()
{
    InstrumentMoveStep nextStep = operationStep;

    switch (operationStep)
    {
        case InstrumentMoveStep::initStep:
        {
            moveInit();
            if (newFileManager.getCurrentOperation() != fmMoveInstrumentsInWorkspace)
            {
                return; // aborted
            }
            stepSlot = rangeFirst;
            materializeState = 0;
            nextStep = InstrumentMoveStep::materializeStep;
            break;
        }
        case InstrumentMoveStep::materializeStep:
        {
            if (stepSlot > rangeLast)
            {
                stepSlot = rangeFirst;
                nextStep = InstrumentMoveStep::reorderStep;
                break;
            }

            // a sample that ends up back in its own slot keeps its files as they are
            bool done = (remap[stepSlot] == stepSlot) ||
                        materializeWorkspaceInstrument(stepSlot, materializeState);

            if (newFileManager.getCurrentOperation() != fmMoveInstrumentsInWorkspace)
            {
                // a save error stopped the operation - nothing has been moved yet
                releaseInstruments();
                return;
            }

            if (done)
            {
                stepSlot++;
                materializeState = 0;
            }
            break;
        }
        case InstrumentMoveStep::reorderStep:
        {
            if (stepSlot > rangeLast)
            {
                stepSlot = rangeFirst;
                nextStep = InstrumentMoveStep::renameToTempStep;
                break;
            }

            if (!bringSlotIntoPlace(stepSlot))
            {
                abort(fmMoveInstrumentsError);
                return;
            }
            stepSlot++;
            break;
        }
        case InstrumentMoveStep::renameToTempStep:
        {
            if (stepSlot > rangeLast)
            {
                stepSlot = rangeFirst;
                nextStep = InstrumentMoveStep::renameFromTempStep;
                break;
            }

            // stepSlot is an old slot number here
            if (remap[stepSlot] != stepSlot)
            {
                for (const char* format : kWorkspaceFormats)
                {
                    renameWorkspaceFile(format, stepSlot + 1, stepSlot + kTempNumberBase);
                }
            }
            stepSlot++;
            break;
        }
        case InstrumentMoveStep::renameFromTempStep:
        {
            if (stepSlot > rangeLast)
            {
                nextStep = InstrumentMoveStep::rebindStreamsStep;
                break;
            }

            if (remap[stepSlot] != stepSlot)
            {
                for (const char* format : kWorkspaceFormats)
                {
                    renameWorkspaceFile(format, stepSlot + kTempNumberBase, remap[stepSlot] + 1);
                }
            }
            stepSlot++;
            break;
        }
        case InstrumentMoveStep::rebindStreamsStep:
        {
            // streamed samples that changed slot read from their renamed workspace file
            for (uint8_t idx = rangeFirst; idx <= rangeLast; idx++)
            {
                if (order[idx] != idx && mtProject.instrument[idx].isActive &&
                    mtProject.sampleMemory[idx].isStreamed)
                {
                    char wsPath[PATCH_SIZE];
                    workspaceInstrumentPath(wsPath, sizeof(wsPath), idx);
                    voiceStreamManager.rebindInstrumentStreamFile(
                      idx, wsPath, mtProject.sampleMemory[idx].fileDataOffset);
                }
            }
            patternIdx = PATTERN_INDEX_MIN;
            nextStep = remapPatterns ? InstrumentMoveStep::remapPatternsStep
                                     : InstrumentMoveStep::finishStep;
            break;
        }
        case InstrumentMoveStep::remapPatternsStep:
        {
            if (patternIdx == PATTERN_INDEX_MIN)
            {
                newFileManager.remapLoadedPatternsInstruments(remap);
            }

            // a failing pattern file is left alone - it would not load anyway
            newFileManager.remapWorkspacePatternInstruments(patternIdx, remap);

            if (patternIdx >= PATTERN_INDEX_MAX)
            {
                nextStep = InstrumentMoveStep::finishStep;
            }
            else
            {
                patternIdx++;
            }
            break;
        }
        case InstrumentMoveStep::finishStep:
        {
            moveFinish();
            break;
        }
        default: abort(fmMoveInstrumentsError); break;
    }

    operationStep = nextStep;
}

float InstrumentMove::getProgress()
{
    const float rangeSize = (float)(rangeLast - rangeFirst + 1);
    const float slotPart = (float)(stepSlot - rangeFirst) / rangeSize;

    switch (operationStep)
    {
        case initStep: return 0.0f;
        case materializeStep: return 0.5f * slotPart;
        case reorderStep: return 0.5f + 0.1f * slotPart;
        case renameToTempStep: return 0.6f + 0.05f * slotPart;
        case renameFromTempStep: return 0.65f + 0.05f * slotPart;
        case rebindStreamsStep: return 0.7f;
        case remapPatternsStep:
            return 0.7f + 0.3f * (float)(patternIdx - PATTERN_INDEX_MIN) /
                            (float)(PATTERN_INDEX_MAX - PATTERN_INDEX_MIN + 1);
        default: return 1.0f;
    }
}

//=================================================================================================
//=================================================================================================

bool InstrumentMove::reorderInstruments(const uint8_t* newOrder, bool updatePatterns)
{
    if (newFileManager.getStatus() != fmIdle)
        return false;
    if (newFileManager.getCurrentOperation() != fmNoOperation)
        return false;
    if (newOrder == nullptr)
        return false;

    // must be a permutation
    bool seen[kSampleInstrumentsCount] = {};
    for (uint8_t i = 0; i < kSampleInstrumentsCount; i++)
    {
        uint8_t src = newOrder[i];
        if (src >= kSampleInstrumentsCount || seen[src])
            return false;
        seen[src] = true;
    }

    int16_t first = -1;
    int16_t last = -1;
    for (uint8_t i = 0; i < kSampleInstrumentsCount; i++)
    {
        order[i] = newOrder[i];
        remap[newOrder[i]] = i;
        current[i] = i;
        if (newOrder[i] != i)
        {
            if (first < 0)
                first = i;
            last = i;
        }
    }

    if (first < 0)
        return false; // nothing to do

    rangeFirst = first;
    rangeLast = last;
    remapPatterns = updatePatterns;
    stepSlot = rangeFirst;
    materializeState = 0;
    patternIdx = PATTERN_INDEX_MIN;

    newFileManager.setStatus(fmMovingInstruments);
    newFileManager.setCurrentOperation(fmMoveInstrumentsInWorkspace);
    operationStep = InstrumentMoveStep::initStep;
    return true;
}

void InstrumentMove::moveInit()
{
    // the reorder rotates sample RAM in place, which relies on the slots in the range being
    // packed back to back in slot order - check before touching anything
    int16_t* expectedStart = nullptr;
    for (uint8_t idx = rangeFirst; idx <= rangeLast; idx++)
    {
        if (!mtProject.instrument[idx].isActive)
        {
            continue;
        }
        int16_t* start = mtProject.instrument[idx].sample.address;
        if (expectedStart != nullptr && start != expectedStart)
        {
            stopOperationWithError(fmMoveInstrumentsError);
            return;
        }
        expectedStart = start + instrumentFootprint(idx, newFileManager.isInstrumentStereo(idx));
    }

    for (uint8_t idx = rangeFirst; idx <= rangeLast; idx++)
    {
        engine.setBlockedInstrument(idx);
        voiceStreamManager.invalidateInstrumentVoices(idx);
    }
}

// returns true when the slot's workspace .pti holds the header and the full sample data
bool InstrumentMove::materializeWorkspaceInstrument(uint8_t idx, uint8_t& state)
{
    if (!mtProject.instrument[idx].isActive)
    {
        return true;
    }

    char wsPath[PATCH_SIZE];
    workspaceInstrumentPath(wsPath, sizeof(wsPath), idx);

    const bool streamed = mtProject.sampleMemory[idx].isStreamed;

    if (state == 0)
    {
        const char* streamPath =
          streamed ? voiceStreamManager.getInstrumentStreamPath(idx) : nullptr;
        if (streamPath != nullptr && strcmp(streamPath, wsPath) == 0)
        {
            // the backing file already is the workspace file, it moves with the rename
            return true;
        }

        bool stereo = false;
        if (!newFileManager.isInstrumentSampleChanged(idx) &&
            newFileManager.isPtiFileValid(
              wsPath, stereo, sizeof(strInstrumentFile), mtProject.instrument[idx].sample.length))
        {
            // already complete - a streamed sample is rebound to it after the renames
            return true;
        }

        if (streamed)
        {
            // saveSampleDataToWorkspace copies the stream source in and rebinds, but writes
            // over the old file without truncating it
            if (fileSystem->exists(wsPath))
            {
                fileSystem->remove(wsPath);
            }
        }
        else if (newFileManager.saveInstrument(idx) == 0)
        {
            stopOperationWithError(fmMoveInstrumentsError);
            return false;
        }

        state = 1;
    }

    if (newFileManager.saveSampleDataToWorkspace(idx))
    {
        newFileManager.clearSampleChanged(idx);
        return true;
    }

    return false;
}

void InstrumentMove::swapAdjacentStructs(uint8_t slotA)
{
    const uint8_t slotB = slotA + 1;

    std::swap(mtProject.instrument[slotA], mtProject.instrument[slotB]);
    std::swap(mtProject.sampleMemory[slotA], mtProject.sampleMemory[slotB]);
    voiceStreamManager.swapInstrumentMetadata(slotA, slotB);
    std::swap(current[slotA], current[slotB]);
}

// Moves the sample that belongs in targetSlot there from wherever it currently is - always at
// or after targetSlot, since the slots before it are already final. The slots in between
// shift one place towards the end.
bool InstrumentMove::bringSlotIntoPlace(uint8_t targetSlot)
{
    uint8_t from = targetSlot;
    while (from <= rangeLast && current[from] != order[targetSlot])
    {
        from++;
    }
    if (from > rangeLast)
    {
        return false;
    }
    if (from == targetSlot)
    {
        return true;
    }

    // stereo is derived from the L/R address spacing, which the struct swaps keep intact,
    // but read it before any base address changes
    bool stereo[kSampleInstrumentsCount] = {};
    int16_t* blockStart = nullptr;
    for (uint8_t idx = targetSlot; idx <= from; idx++)
    {
        if (mtProject.instrument[idx].isActive)
        {
            stereo[idx] = newFileManager.isInstrumentStereo(idx);
            if (blockStart == nullptr)
            {
                blockStart = mtProject.instrument[idx].sample.address;
            }
        }
    }

    const bool movingActive = mtProject.instrument[from].isActive;
    if (movingActive)
    {
        // [targetSlot .. from-1][from] -> [from][targetSlot .. from-1]
        int16_t* movingStart = mtProject.instrument[from].sample.address;
        int16_t* movingEnd = movingStart + instrumentFootprint(from, stereo[from]);
        std::rotate(blockStart, movingStart, movingEnd);
    }
    // an empty slot takes no RAM - moving it leaves the packing of the active samples as is

    for (uint8_t slot = from; slot > targetSlot; slot--)
    {
        swapAdjacentStructs(slot - 1);
        std::swap(stereo[slot - 1], stereo[slot]);
    }

    if (movingActive)
    {
        int16_t* base = blockStart;
        for (uint8_t idx = targetSlot; idx <= from; idx++)
        {
            if (mtProject.instrument[idx].isActive)
            {
                setInstrumentBaseAddress(idx, base, stereo[idx]);
                base += instrumentFootprint(idx, stereo[idx]);
            }
        }
    }

    return true;
}

void InstrumentMove::renameWorkspaceFile(const char* format, uint8_t fromNumber, uint8_t toNumber)
{
    char pathFrom[PATCH_SIZE];
    char pathTo[PATCH_SIZE];

    snprintf(pathFrom, sizeof(pathFrom), format, fromNumber);
    snprintf(pathTo, sizeof(pathTo), format, toNumber);

    if (!fileSystem->exists(pathFrom))
    {
        return;
    }
    if (fileSystem->exists(pathTo))
    {
        // a leftover from an interrupted earlier move
        fileSystem->remove(pathTo);
    }
    fileSystem->rename(pathFrom, pathTo);
}

void InstrumentMove::moveFinish()
{
    for (uint8_t idx = rangeFirst; idx <= rangeLast; idx++)
    {
        if (order[idx] != idx)
        {
            // headers on disk follow the files, but unsaved edits stayed with the old flag
            // slot - rewrite them
            newFileManager.setInstrumentStructChanged(idx);
        }
    }

    newFileManager.setProjectStructChanged();
    mtProject.values.projectNotSavedFlag |= 1;

    releaseInstruments();

    newFileManager.setStatus(fmMoveInstrumentsEnd);
    newFileManager.setCurrentOperation(fmNoOperation);
}

void InstrumentMove::releaseInstruments()
{
    for (uint8_t idx = rangeFirst; idx <= rangeLast; idx++)
    {
        engine.clearBlockedInstrument(idx);
    }
}

void InstrumentMove::abort(uint8_t errorId)
{
    releaseInstruments();
    stopOperationWithError(errorId);
}
