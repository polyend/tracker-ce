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

    void setScreenLabel(uint8_t index, const char* text, bool filled) override
    {
        if (index >= 8)
        {
            return;
        }

        const uint32_t* colors = filled ? display.getTheme().performActiveTrackLabelsColors
                                        : display.getTheme().inactiveLabelsColors;

        display.setControlColors(performanceMode.label[index], colors);
        display.setControlText(performanceMode.label[index], text != nullptr ? text : "");
        display.setControlText2(performanceMode.label[index], (char*)"");
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

    bool isShiftPressed() const override
    {
        return performanceMode.buttons->isButtonPressed(interfaceButtonShift);
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
};

static PerformancePageHostImpl s_performancePageHost;

PerformancePageHost& getPerformancePageHost()
{
    return s_performancePageHost;
}
