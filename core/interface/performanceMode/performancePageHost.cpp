#include "PerformanceHost.h"

#include "core/interface.h"
#include "core/interfaceDefs.h"
#include "display.h"
#include "fileManager.h"
#include "mtAudioEngine.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "performanceEngine.h"
#include "performanceMode.h"
#include "titleBar.h"

#include <cstring>

class PerformancePageHostImpl : public PerformancePageHost
{
  public:
    void setPageTitle(const char* title) override
    {
        performanceMode.titleBar->show();
        performanceMode.titleBar->setTitle(title != nullptr ? title : "");
    }

    void setScreenLabel(uint8_t index,
                        const char* text,
                        PerformanceLabelState state,
                        const char* text2) override
    {
        if (index >= 8)
        {
            return;
        }

        const uint32_t* colors = display.getTheme().inactiveLabelsColors;
        if (state == PerformanceLabelState::Active)
        {
            colors = display.getTheme().performActiveTrackLabelsColors;
        }
        else if (state == PerformanceLabelState::Muted)
        {
            colors = display.getTheme().performMutedTrackLabelsColors;
        }
        else if (state == PerformanceLabelState::Filled)
        {
            colors = display.getTheme().performActiveTrackLabelsColors;
        }

        const char* secondary = text2;
        if (secondary == nullptr && state == PerformanceLabelState::Active)
        {
            secondary = "Active";
        }

        display.setControlColors(performanceMode.label[index], colors);
        display.setControlText(performanceMode.label[index], text != nullptr ? text : "");
        display.setControlText2(performanceMode.label[index],
                                secondary != nullptr ? (char*)secondary : (char*)"");
        display.setControlStyle(performanceMode.label[index],
                                controlStyleCenterX | controlStyleCenterY | controlStyleFont3);
        display.setControlShow(performanceMode.label[index]);
        display.refreshControl(performanceMode.label[index]);
    }

    void finishScreenLabels() override { display.synchronizeRefresh(); }

    void restoreTrackLabels() override
    {
        performanceMode.titleBar->setTitle("Performance FX");
        performanceMode.showTracksPatterns();
        performanceMode.showTracksState(
          performanceMode.buttons->isButtonPressed(interfaceButtonShift));
    }

    void refreshFxRack() override
    {
        for (uint8_t place = 0; place < 12; place++)
        {
            performanceMode.showFxNames(place);
            performanceMode.showPerformaceValue(place);
        }
        performanceMode.lightUpPadBoard();
        display.synchronizeRefresh();
    }

    bool isShowingFxRack() const override
    {
        return mtInterface.isPerformanceModeScreenActive() &&
               !performanceMode.isPerformancePageActive();
    }

    bool isActivePerformancePage(const PerformancePage* page) const override
    {
        return page != nullptr && performanceMode.getActivePerformancePage() == page;
    }

    bool isShiftPressed() const override
    {
        return performanceMode.buttons->isButtonPressed(interfaceButtonShift);
    }

    uint8_t tracksPage() const override { return performanceMode.tracksPage ? 1 : 0; }

    bool isSampleTrackMuted(uint8_t index) const override
    {
        if (index >= kSampleTracks || index >= kPerfHostSampleTracks)
        {
            return false;
        }
        return mtProject.values.trackMute[index] != 0;
    }

    bool isMidiTrackMuted(uint8_t index) const override
    {
        if (index >= kMaxMidiTracksInTrackers || index >= kPerfHostMidiTracks)
        {
            return false;
        }
        return mtProject.values.midiTrackMute[index] != 0;
    }

    void captureFxScene(strPerfScene& scene) override { performance.captureScene(scene); }

    void recallFxScene(const strPerfScene& scene) override { performance.recallScene(scene); }

    void capturePatternScene(strPatternScene& scene) override
    {
        memset(scene.name, 0, sizeof(scene.name));
        for (uint8_t track = 0; track < kPerfHostPerformanceTracks; track++)
        {
            if (track < kPerformanceTracks)
            {
                scene.trackPatterns[track] = mtProject.perfTracksPatterns[track];
            }
            else
            {
                scene.trackPatterns[track] = 1;
            }
        }
    }

    void recallPatternScene(const strPatternScene& scene) override
    {
        for (uint8_t track = 0; track < kPerformanceTracks && track < kPerfHostPerformanceTracks;
             track++)
        {
            uint16_t pattern = scene.trackPatterns[track];
            if (pattern < 1)
            {
                pattern = 1;
            }
            mtProject.perfTracksPatterns[track] = pattern;
            sequencer.setTrackToLoadNow(track, pattern);
        }
        newFileManager.setProjectStructChanged();
        performanceMode.refreshTrackPattern = 1;
    }

    void captureMuteScene(strMuteScene& scene) override
    {
        memset(scene.name, 0, sizeof(scene.name));
        for (uint8_t i = 0; i < kPerfHostSampleTracks && i < kSampleTracks; i++)
        {
            scene.sampleMute[i] = mtProject.values.trackMute[i] ? 1 : 0;
        }
        for (uint8_t i = 0; i < kPerfHostMidiTracks && i < kMaxMidiTracksInTrackers; i++)
        {
            scene.midiMute[i] = mtProject.values.midiTrackMute[i] ? 1 : 0;
        }
    }

    void recallMuteScene(const strMuteScene& scene) override
    {
        for (uint8_t i = 0; i < kSampleTracks && i < kPerfHostSampleTracks; i++)
        {
            uint8_t muted = scene.sampleMute[i] ? 1 : 0;
            mtProject.values.trackMute[i] = muted;
            engine.muteTrack(i, muted);
        }
        for (uint8_t i = 0; i < kMaxMidiTracksInTrackers && i < kPerfHostMidiTracks; i++)
        {
            mtProject.values.midiTrackMute[i] = scene.midiMute[i] ? 1 : 0;
        }
        newFileManager.setProjectStructChanged();
        performanceMode.refreshTrackState = 1;
    }

    uint8_t performanceFxCount() const override { return performance.getFxCount(); }

    const char* performanceFxName(uint8_t fxId) const override
    {
        return performance.getFxName(fxId);
    }

    int16_t performanceFxMin(uint8_t fxId) const override { return performance.getFxMin(fxId); }

    int16_t performanceFxMax(uint8_t fxId) const override { return performance.getFxMax(fxId); }

    void applyPerformanceFx(uint8_t fxId, int16_t value) override
    {
        performance.applyFx(fxId, value);
    }

    void clearPerformanceFx(uint8_t fxId) override { performance.clearFx(fxId); }
};

static PerformancePageHostImpl s_performancePageHost;

PerformancePageHost& getPerformancePageHost()
{
    return s_performancePageHost;
}
