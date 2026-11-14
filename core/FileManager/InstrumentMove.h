#ifndef FILEMANAGER_INSTRUMENTMOVE_H_
#define FILEMANAGER_INSTRUMENTMOVE_H_

#include "FileManagerOperation.h"

#include "mtStructs.h"

#include <stdint.h>

// Reorders the sample instrument slots in one operation. The caller passes the new order as
// order[newSlot] = oldSlot (a permutation of 0..kSampleInstrumentsCount-1).
//
// Steps:
//  1. every sample that changes slot gets a self-contained workspace .pti (header + sample
//     data). After a project open the workspace files are header-only and the audio lives in
//     the project folder under slot-numbered names, which would no longer match.
//  2. sample RAM, instrument structs and stream metadata are put in the new order, one slot
//     per update. Sample memory is packed in slot order, so each slot is brought into place
//     with a single in-place rotation of the range it moves across.
//  3. the workspace files are renamed, through temporary names so no rename overwrites a
//     file still waiting to be moved.
//  4. optionally, every pattern step is rewritten to follow its sample.
class InstrumentMove : public FileManagerOperation
{
  public:
    virtual void update() override;
    virtual float getProgress() override;

    bool reorderInstruments(const uint8_t* order, bool updatePatterns);

  private:
    enum InstrumentMoveStep
    {
        initStep,
        materializeStep,
        reorderStep,
        renameToTempStep,
        renameFromTempStep,
        rebindStreamsStep,
        remapPatternsStep,
        finishStep,
    };

    InstrumentMoveStep operationStep;

    uint8_t rangeFirst;
    uint8_t rangeLast;
    bool remapPatterns;

    uint8_t order[kSampleInstrumentsCount];   // order[newSlot] = oldSlot
    uint8_t remap[kSampleInstrumentsCount];   // remap[oldSlot] = newSlot
    uint8_t current[kSampleInstrumentsCount]; // current[slot] = oldSlot held there right now

    uint8_t stepSlot;
    uint8_t materializeState;
    uint16_t patternIdx;

    void moveInit();
    bool materializeWorkspaceInstrument(uint8_t idx, uint8_t& state);
    bool bringSlotIntoPlace(uint8_t targetSlot);
    void swapAdjacentStructs(uint8_t slotA);
    void renameWorkspaceFile(const char* format, uint8_t fromNumber, uint8_t toNumber);
    void moveFinish();
    void releaseInstruments();
    void abort(uint8_t errorId);
};

#endif /* FILEMANAGER_INSTRUMENTMOVE_H_ */
