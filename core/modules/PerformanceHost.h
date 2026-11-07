#ifndef MODULES_PERFORMANCEHOST_H_
#define MODULES_PERFORMANCEHOST_H_

#include <stdint.h>

// Snapshot of the 12 Performance FX slots. Owned by feature modules that
// store/recall scenes; filled and applied through PerformancePageHost.
static constexpr uint8_t kPerfFxSceneSlotCount = 12;
static constexpr uint8_t kPerfFxSceneNameLength = 16;

struct strPerfScene
{
    char name[kPerfFxSceneNameLength];
    uint8_t fxPlaces[kPerfFxSceneSlotCount];
    uint8_t selectedValues[kPerfFxSceneSlotCount];
    int16_t fxValues[kPerfFxSceneSlotCount][4];
    int16_t liveValues[kPerfFxSceneSlotCount];
};

static_assert(sizeof(strPerfScene) % 4 == 0, "strPerfScene must be 4-byte aligned for FatFs DMA");

// Button state values forwarded to PerformancePage::onScreenButton.
// Matches firmware buttonStates (interfaceDefs.h).
enum PerformancePageButtonState : uint8_t
{
    kPerformanceButtonRelease = 0,
    kPerformanceButtonPress = 1,
    kPerformanceButtonHold = 2,
    kPerformanceButtonDouble = 3,
};

// Host services available to a PerformancePage while it is active.
// Implemented by Performance Mode inside the firmware — modules depend only
// on this header, not on performanceMode.h / display internals.
class PerformancePageHost
{
  public:
    virtual ~PerformancePageHost() {}

    virtual void setPageTitle(const char* title) = 0;
    // index 0..7 screen buttons under the display
    virtual void setScreenLabel(uint8_t index, const char* text, bool filled) = 0;
    virtual void finishScreenLabels() = 0;

    virtual void restoreTrackLabels() = 0;
    virtual void refreshFxRack() = 0;

    virtual bool isShiftPressed() const = 0;

    virtual void captureFxScene(strPerfScene& scene) = 0;
    virtual void recallFxScene(const strPerfScene& scene) = 0;
};

// Firmware-provided host for Performance Mode pages.
PerformancePageHost& getPerformancePageHost();

#endif /* MODULES_PERFORMANCEHOST_H_ */
