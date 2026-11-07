#ifndef SOURCE_SCRIPTRUNNER_H_
#define SOURCE_SCRIPTRUNNER_H_

#include "PlatformImpl.h"
#include "Tracker.h"
#include "WavWriter.h"
#include <string>
#include <vector>

class ScriptRunner
{
  public:
    bool load(const char* path);
    // Returns false on hard error. Advances virtual time and drives tracker.
    bool run(PlatformImpl& platform, Tracker* tracker, WavWriter* wav);

  private:
    struct Step
    {
        enum Type
        {
            kWait,
            kTap,
            kPress,
            kRelease,
            kPad,
            kEnc,
            kScreenshot,
            kQuit
        } type;
        int a = 0;
        int b = 0;
        std::string name;
        std::string path;
    };

    std::vector<Step> steps;
    bool mapButtonName(const std::string& name, uint8_t& out) const;
};

#endif
