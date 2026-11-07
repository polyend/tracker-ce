#include "ScriptRunner.h"
#include "interfaceDefs.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

bool ScriptRunner::mapButtonName(const std::string& name, uint8_t& out) const
{
    auto eq = [&](const char* s) {
        if (name.size() != std::strlen(s))
            return false;
        for (size_t i = 0; i < name.size(); ++i)
        {
            char a = name[i];
            char b = s[i];
            if (a >= 'a' && a <= 'z')
                a = static_cast<char>(a - 'a' + 'A');
            if (b >= 'a' && b <= 'z')
                b = static_cast<char>(b - 'a' + 'A');
            if (a != b)
                return false;
        }
        return true;
    };

    if (eq("PLAY"))
    {
        out = interfaceButtonPlay;
        return true;
    }
    if (eq("SHIFT"))
    {
        out = interfaceButtonShift;
        return true;
    }
    if (eq("REC"))
    {
        out = interfaceButtonRec;
        return true;
    }
    if (eq("UP"))
    {
        out = interfaceButtonUp;
        return true;
    }
    if (eq("DOWN"))
    {
        out = interfaceButtonDown;
        return true;
    }
    if (eq("LEFT"))
    {
        out = interfaceButtonLeft;
        return true;
    }
    if (eq("RIGHT"))
    {
        out = interfaceButtonRight;
        return true;
    }
    if (eq("PERFORMANCE"))
    {
        out = interfaceButtonPerformance;
        return true;
    }
    if (eq("SONG"))
    {
        out = interfaceButtonSong;
        return true;
    }
    if (eq("FILE"))
    {
        out = interfaceButtonFile;
        return true;
    }
    if (eq("CONFIG"))
    {
        out = interfaceButtonConfig;
        return true;
    }
    if (eq("ENTER") || eq("INSERT"))
    {
        out = interfaceButtonInsert;
        return true;
    }
    if (eq("DELETE"))
    {
        out = interfaceButtonDelete;
        return true;
    }
    if (eq("COPY"))
    {
        out = interfaceButtonCopy;
        return true;
    }
    return false;
}

bool ScriptRunner::load(const char* path)
{
    steps.clear();
    std::ifstream in(path);
    if (!in)
        return false;
    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;
        Step step{};
        if (cmd == "wait")
        {
            step.type = Step::kWait;
            ss >> step.a;
        }
        else if (cmd == "tap")
        {
            step.type = Step::kTap;
            ss >> step.name;
        }
        else if (cmd == "press")
        {
            step.type = Step::kPress;
            ss >> step.name;
        }
        else if (cmd == "release")
        {
            step.type = Step::kRelease;
            ss >> step.name;
        }
        else if (cmd == "pad")
        {
            step.type = Step::kPad;
            ss >> step.a >> step.name;
        }
        else if (cmd == "enc")
        {
            step.type = Step::kEnc;
            ss >> step.a;
        }
        else if (cmd == "screenshot")
        {
            step.type = Step::kScreenshot;
            ss >> step.path;
        }
        else if (cmd == "quit")
        {
            step.type = Step::kQuit;
        }
        else
        {
            std::fprintf(stderr, "Unknown script command: %s\n", cmd.c_str());
            continue;
        }
        steps.push_back(step);
    }
    return true;
}

bool ScriptRunner::run(PlatformImpl& platform, Tracker* tracker, WavWriter* wav)
{
    platform.timerImpl().setVirtualTime(true);
    platform.timerImpl().setVirtualMicros(0);

    constexpr int kBlock = 128;
    constexpr float kRate = 44117.6470588f;
    const uint32_t usPerBlock = static_cast<uint32_t>((kBlock * 1000000.0f) / kRate);

    int16_t left[kBlock]{};
    int16_t right[kBlock]{};

    for (const Step& step : steps)
    {
        switch (step.type)
        {
            case Step::kWait:
                for (int ms = 0; ms < step.a; ++ms)
                {
                    platform.timerImpl().advanceVirtualMs(1);
                    // About 44 blocks per second of audio at this rate
                    static uint32_t audioAccUs = 0;
                    audioAccUs += 1000;
                    while (audioAccUs >= usPerBlock)
                    {
                        audioAccUs -= usPerBlock;
                        tracker->updateAudioBuffers(left, right, kBlock);
                        if (wav && wav->isOpen())
                            wav->writeInterleaved(left, right, kBlock);
                    }
                    platform.update();
                    tracker->update();
                }
                break;
            case Step::kTap:
            {
                uint8_t id = 0;
                if (!mapButtonName(step.name, id))
                {
                    std::fprintf(stderr, "Unknown button %s\n", step.name.c_str());
                    break;
                }
                platform.buttonsImpl().tap(id);
                break;
            }
            case Step::kPress:
            {
                uint8_t id = 0;
                if (mapButtonName(step.name, id))
                    platform.buttonsImpl().press(id);
                break;
            }
            case Step::kRelease:
            {
                uint8_t id = 0;
                if (mapButtonName(step.name, id))
                    platform.buttonsImpl().release(id);
                break;
            }
            case Step::kPad:
                if (step.name == "tap" || step.name.empty())
                    platform.padsImpl().tap(static_cast<uint8_t>(step.a));
                else if (step.name == "press")
                    platform.padsImpl().press(static_cast<uint8_t>(step.a));
                else if (step.name == "release")
                    platform.padsImpl().release(static_cast<uint8_t>(step.a));
                break;
            case Step::kEnc:
                platform.encoderImpl().turn(step.a);
                break;
            case Step::kScreenshot:
                platform.displayImpl().rasterizer().render();
                if (!platform.displayImpl().rasterizer().savePng(step.path.c_str()))
                    std::fprintf(stderr, "Failed to save screenshot %s\n", step.path.c_str());
                else
                    std::printf("Wrote screenshot %s (unknown cmds=%u)\n",
                                step.path.c_str(),
                                platform.displayImpl().rasterizer().unknownCommandCount());
                break;
            case Step::kQuit:
                return true;
        }
    }
    return true;
}
