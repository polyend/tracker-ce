#include "PlatformImpl.h"
#include "ScriptRunner.h"
#include "Tracker.h"
#include "WavWriter.h"
#include "interfaceDefs.h"

#include <SDL.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{
PlatformImpl* gPlatform = nullptr;
Tracker* gTracker = nullptr;
WavWriter* gWav = nullptr;
std::atomic<bool> gRunning{ true };

void audioCallback(void* /*userdata*/, Uint8* stream, int len)
{
    if (!gTracker || !gPlatform)
    {
        std::memset(stream, 0, len);
        return;
    }
    const int frames = len / (int)(sizeof(int16_t) * 2);
    int16_t* out = reinterpret_cast<int16_t*>(stream);
    int offset = 0;
    while (offset < frames)
    {
        const int n = std::min(128, frames - offset);
        int16_t left[128]{};
        int16_t right[128]{};
        gTracker->updateAudioBuffers(left, right, n);
        for (int i = 0; i < n; ++i)
        {
            out[(offset + i) * 2] = left[i];
            out[(offset + i) * 2 + 1] = right[i];
        }
        if (gWav && gWav->isOpen())
            gWav->writeInterleaved(left, right, n);
        offset += n;
    }
}

void printUsage(const char* argv0)
{
    std::printf(
      "Usage: %s [--sd DIR] [--wav OUT.wav] [--script FILE] [--scale N] [--headless]\n"
      "  --sd DIR       SD card root folder (default ./sdcard)\n"
      "  --wav OUT.wav  Record master audio\n"
      "  --script FILE  Run a headless button script\n"
      "  --scale N      Window scale (default 1)\n"
      "  --headless     No window (implied by --script)\n",
      argv0);
}

uint8_t mapKeyToButton(SDL_Keycode key, bool& isPad, uint8_t& padIdx)
{
    isPad = false;
    switch (key)
    {
        case SDLK_SPACE:
            return interfaceButtonPlay;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
            return interfaceButtonShift;
        case SDLK_UP:
            return interfaceButtonUp;
        case SDLK_DOWN:
            return interfaceButtonDown;
        case SDLK_LEFT:
            return interfaceButtonLeft;
        case SDLK_RIGHT:
            return interfaceButtonRight;
        case SDLK_RETURN:
            return interfaceButtonInsert;
        case SDLK_BACKSPACE:
        case SDLK_DELETE:
            return interfaceButtonDelete;
        case SDLK_F1:
            return interfaceButtonSong;
        case SDLK_F2:
            return interfaceButtonPerformance;
        case SDLK_F3:
            return interfaceButtonFile;
        case SDLK_F4:
            return interfaceButtonConfig;
        case SDLK_F5:
            return interfaceButtonSamplePlay;
        case SDLK_F6:
            return interfaceButtonSampleEdit;
        case SDLK_F7:
            return interfaceButtonInstr;
        case SDLK_F8:
            return interfaceButtonMaster;
        default:
            break;
    }
    // Letter rows map to pads 0..47 roughly
    if (key >= SDLK_a && key <= SDLK_z)
    {
        isPad = true;
        padIdx = static_cast<uint8_t>(key - SDLK_a);
        return 0;
    }
    if (key >= SDLK_0 && key <= SDLK_9)
    {
        isPad = true;
        padIdx = static_cast<uint8_t>(26 + (key - SDLK_0));
        return 0;
    }
    return 0xFF;
}
} // namespace

extern "C" void _debuglog(const char* msg)
{
    std::fputs(msg, stderr);
    std::fputc('\n', stderr);
}

extern "C" void _debuglogf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
}

extern "C" void assert_func_play(const char* file, int line)
{
    std::fprintf(stderr, "assert failed at %s:%d\n", file, line);
    std::abort();
}

int main(int argc, char** argv)
{
    std::string sdRoot = "./sdcard";
    std::string wavPath;
    std::string scriptPath;
    int scale = 1;
    bool headless = false;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--sd") == 0 && i + 1 < argc)
            sdRoot = argv[++i];
        else if (std::strcmp(argv[i], "--wav") == 0 && i + 1 < argc)
            wavPath = argv[++i];
        else if (std::strcmp(argv[i], "--script") == 0 && i + 1 < argc)
        {
            scriptPath = argv[++i];
            headless = true;
        }
        else if (std::strcmp(argv[i], "--scale") == 0 && i + 1 < argc)
            scale = std::max(1, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--headless") == 0)
            headless = true;
        else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
        {
            printUsage(argv[0]);
            return 0;
        }
        else
        {
            std::fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            printUsage(argv[0]);
            return 1;
        }
    }

    PlatformImpl platform;
    gPlatform = &platform;
    Tracker* tracker = Tracker::getInstance();
    gTracker = tracker;

    WavWriter wav;
    if (!wavPath.empty())
    {
        if (!wav.open(wavPath.c_str(), 44118, 2))
        {
            std::fprintf(stderr, "Failed to open wav %s\n", wavPath.c_str());
            return 1;
        }
        gWav = &wav;
    }

    platform.init(sdRoot.c_str());
    auto startDisplay = tracker->getStartDisplayFunc();
    if (startDisplay)
        startDisplay(&platform);
    tracker->init(&platform);

    if (!scriptPath.empty())
    {
        ScriptRunner runner;
        if (!runner.load(scriptPath.c_str()))
        {
            std::fprintf(stderr, "Failed to load script %s\n", scriptPath.c_str());
            return 1;
        }
        const bool ok = runner.run(platform, tracker, gWav);
        wav.close();
        std::printf("Script finished (%s)\n", ok ? "ok" : "error");
        return ok ? 0 : 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
    {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_AudioSpec want{}, have{};
    want.freq = 44118;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = audioCallback;
    SDL_AudioDeviceID audioDev = 0;
    if (!headless)
    {
        audioDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
        if (!audioDev)
            std::fprintf(stderr, "SDL audio open failed: %s (continuing silent)\n", SDL_GetError());
        else
            SDL_PauseAudioDevice(audioDev, 0);
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    if (!headless)
    {
        window = SDL_CreateWindow("Tracker CE Host",
                                  SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED,
                                  Ft812Rasterizer::kWidth * scale,
                                  Ft812Rasterizer::kHeight * scale,
                                  SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        texture = SDL_CreateTexture(renderer,
                                    SDL_PIXELFORMAT_RGB888,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    Ft812Rasterizer::kWidth,
                                    Ft812Rasterizer::kHeight);
    }

    uint32_t lastMs = SDL_GetTicks();
    while (gRunning)
    {
        SDL_Event ev;
        while (!headless && SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT)
                gRunning = false;
            else if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP)
            {
                if (ev.key.repeat)
                    continue;
                if (ev.key.keysym.sym == SDLK_ESCAPE && ev.type == SDL_KEYDOWN)
                    gRunning = false;
                if (ev.key.keysym.sym == SDLK_PAGEUP && ev.type == SDL_KEYDOWN)
                    platform.encoderImpl().turn(1);
                if (ev.key.keysym.sym == SDLK_PAGEDOWN && ev.type == SDL_KEYDOWN)
                    platform.encoderImpl().turn(-1);

                bool isPad = false;
                uint8_t padIdx = 0;
                uint8_t btn = mapKeyToButton(ev.key.keysym.sym, isPad, padIdx);
                if (isPad)
                {
                    if (ev.type == SDL_KEYDOWN)
                        platform.padsImpl().press(padIdx);
                    else
                        platform.padsImpl().release(padIdx);
                }
                else if (btn != 0xFF)
                {
                    if (ev.type == SDL_KEYDOWN)
                        platform.buttonsImpl().press(btn);
                    else
                        platform.buttonsImpl().release(btn);
                }
            }
        }

        platform.update();
        tracker->update();

        if (!headless && texture && renderer)
        {
            auto& eve = platform.displayImpl().rasterizer();
            // endCoProList already renders; refresh texture each frame
            void* pixels = nullptr;
            int pitch = 0;
            if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) == 0)
            {
                auto* dst = static_cast<uint8_t*>(pixels);
                const uint32_t* src = eve.pixels();
                for (int y = 0; y < Ft812Rasterizer::kHeight; ++y)
                {
                    auto* row = reinterpret_cast<uint32_t*>(dst + y * pitch);
                    for (int x = 0; x < Ft812Rasterizer::kWidth; ++x)
                        row[x] = src[y * Ft812Rasterizer::kWidth + x];
                }
                SDL_UnlockTexture(texture);
            }
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        }

        // Keep UI responsive ~60 Hz when no audio device driving time.
        const uint32_t now = SDL_GetTicks();
        if (now - lastMs < 16)
            SDL_Delay(16 - (now - lastMs));
        lastMs = SDL_GetTicks();

        if (headless)
        {
            // Headless interactive mode: run a few seconds then exit.
            static int frames = 0;
            if (++frames > 120)
                break;
        }
    }

    if (audioDev)
        SDL_CloseAudioDevice(audioDev);
    if (texture)
        SDL_DestroyTexture(texture);
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    SDL_Quit();
    wav.close();
    return 0;
}
