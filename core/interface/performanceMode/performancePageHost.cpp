#include "PerformanceHost.h"

#include "core/interfaceDefs.h"
#include "display.h"
#include "performanceEngine.h"
#include "performanceMode.h"
#include "titleBar.h"

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

    bool isShiftPressed() const override
    {
        return performanceMode.buttons->isButtonPressed(interfaceButtonShift);
    }

    void captureFxScene(strPerfScene& scene) override { performance.captureScene(scene); }

    void recallFxScene(const strPerfScene& scene) override { performance.recallScene(scene); }
};

static PerformancePageHostImpl s_performancePageHost;

PerformancePageHost& getPerformancePageHost()
{
    return s_performancePageHost;
}
