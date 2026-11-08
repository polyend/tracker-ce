#include "PlatformImpl.h"
#include "ScriptRunner.h"
#include "Tracker.h"
#include "WavWriter.h"
#include "config.h"
#include "display.h"
#include "interfaceDefs.h"

#include <SDL.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
PlatformImpl* gPlatform = nullptr;
Tracker* gTracker = nullptr;
WavWriter* gWav = nullptr;
std::atomic<bool> gRunning{ true };

// SPSC ring: audio callback produces, main thread drains to WavWriter (no FILE I/O on RT thread).
constexpr size_t kWavRingSamples = 44118 * 2 * 2; // ~2s interleaved stereo
std::vector<int16_t> gWavRing;
std::atomic<uint32_t> gWavRingWrite{ 0 };
std::atomic<uint32_t> gWavRingRead{ 0 };
std::atomic<bool> gWavCapture{ false };

void wavRingPush(const int16_t* interleaved, int frames)
{
    if (!gWavCapture.load(std::memory_order_relaxed) || !interleaved || frames <= 0)
        return;
    if (gWavRing.empty())
        return;
    const uint32_t cap = static_cast<uint32_t>(gWavRing.size());
    uint32_t w = gWavRingWrite.load(std::memory_order_relaxed);
    const uint32_t r = gWavRingRead.load(std::memory_order_acquire);
    const uint32_t used = (w + cap - r) % cap;
    const uint32_t free = (cap - 1) - used;
    const uint32_t want = static_cast<uint32_t>(frames) * 2u;
    const uint32_t n = std::min(want, free);
    for (uint32_t i = 0; i < n; ++i)
    {
        gWavRing[w] = interleaved[i];
        w = (w + 1) % cap;
    }
    gWavRingWrite.store(w, std::memory_order_release);
}

void wavRingDrain()
{
    if (!gWav || !gWav->isOpen() || gWavRing.empty())
        return;
    const uint32_t cap = static_cast<uint32_t>(gWavRing.size());
    uint32_t r = gWavRingRead.load(std::memory_order_relaxed);
    const uint32_t w = gWavRingWrite.load(std::memory_order_acquire);
    int16_t left[256]{};
    int16_t right[256]{};
    while (r != w)
    {
        int frames = 0;
        while (r != w && frames < 256)
        {
            left[frames] = gWavRing[r];
            r = (r + 1) % cap;
            if (r == w)
                break;
            right[frames] = gWavRing[r];
            r = (r + 1) % cap;
            ++frames;
        }
        if (frames > 0)
            gWav->writeInterleaved(left, right, frames);
    }
    gWavRingRead.store(r, std::memory_order_release);
}

void audioCallback(void* /*userdata*/, Uint8* stream, int len)
{
    if (!gTracker || !gPlatform)
    {
        std::memset(stream, 0, len);
        return;
    }
    hostEnterAudioCallback();
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
        wavRingPush(out + offset * 2, n);
        offset += n;
    }
    hostLeaveAudioCallback();
}

void printUsage(const char* argv0)
{
    std::printf(
      "Usage: %s [--sd DIR] [--wav OUT.wav] [--script FILE] [--live-script FILE] [--scale N] "
      "[--headless]\n"
      "  --sd DIR         SD card root folder (default ./sdcard)\n"
      "  --wav OUT.wav    Record master audio\n"
      "  --script FILE    Headless button script (virtual time)\n"
      "  --live-script FILE  Script with real SDL audio thread\n"
      "  --scale N        Window scale (default 1)\n"
      "  --headless       No window (implied by --script)\n",
      argv0);
}

uint8_t mapKeyToButton(SDL_Keycode key, bool& isPad, uint8_t& padIdx)
{
    isPad = false;
    switch (key)
    {
        // --- Tracker Mini transport / edit cluster ---
        case SDLK_SPACE:
            return interfaceButtonPlay;
        case SDLK_r: // Rec / edit mode (takes priority over pad letter mapping below)
            return interfaceButtonRec;
        case SDLK_c: // Copy (Shift+C pastes in firmware handlers)
            return interfaceButtonCopy;
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

        // --- Pattern step columns ---
        case SDLK_n: // Note
            return interfaceButtonNote;
        case SDLK_i: // Instrument
            return interfaceButtonInstr;
        case SDLK_o: // FX1 (Vol key on device)
            return interfaceButtonVol;
        case SDLK_p: // FX2
            return interfaceButtonFx;

        // --- Mini Menu / D-pad center ---
        case SDLK_F10:
        case SDLK_TAB:
        case SDLK_BACKQUOTE: // ` — reliable on macOS (Tab is often swallowed)
            return interfaceButtonMiniMenu;
        case SDLK_SLASH: // / — Mini middle (cycle menu option pages / modes)
            return interfaceButtonMiniMiddle;

        // --- Mini assignable Function buttons (default: Pattern / Instr / Sample Play / Master) ---
        case SDLK_COMMA: // ,
        case SDLK_F9:
            return interfaceButtonMiniFn1;
        case SDLK_PERIOD: // .
        case SDLK_F11:
            return interfaceButtonMiniFn2;
        case SDLK_SEMICOLON: // ;
        case SDLK_F12:
            return interfaceButtonMiniFn3;
        case SDLK_QUOTE: // '
            return interfaceButtonMiniFn4;

        // --- Soft keys under the screen (left → right) ---
        case SDLK_1:
            return interfaceButton0;
        case SDLK_2:
            return interfaceButton1;
        case SDLK_3:
            return interfaceButton2;
        case SDLK_4:
            return interfaceButton3;
        case SDLK_5:
            return interfaceButton4;
        case SDLK_6:
            return interfaceButton5;
        case SDLK_7:
            return interfaceButton6;
        case SDLK_8:
            return interfaceButton7;

        // --- Mini big +/- buttons (±1 / ±10 value steps) ---
        case SDLK_EQUALS: // =
            return interfaceButtonMiniPlus1;
        case SDLK_MINUS: // -
            return interfaceButtonMiniMinus1;
        case SDLK_RIGHTBRACKET: // ]
            return interfaceButtonMiniPlus10;
        case SDLK_LEFTBRACKET: // [
            return interfaceButtonMiniMinus10;

        // --- Direct module shortcuts (also reachable via Menu / Fn bindings) ---
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
        case SDLK_F13:
            return interfaceButtonPattern;
        case SDLK_F14:
            return interfaceButtonSampleRec;
        case SDLK_F15:
            return interfaceButtonSampleLoad;
        case SDLK_F16:
            return interfaceButtonParams;
        case SDLK_F17:
            return interfaceButtonGame;
        default:
            break;
    }
    // Letter rows map to pads 0..25; 9/0 still hit pads 35/26
    // Reserved letters (not pads): C Copy, R Rec, N/I/O/P pattern columns
    if (key >= SDLK_a && key <= SDLK_z)
    {
        isPad = true;
        padIdx = static_cast<uint8_t>(key - SDLK_a);
        return 0;
    }
    if (key == SDLK_9 || key == SDLK_0)
    {
        isPad = true;
        padIdx = static_cast<uint8_t>(key == SDLK_9 ? 35 : 26);
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
    bool liveScript = false;
    bool songStress = false;
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
        else if (std::strcmp(argv[i], "--live-script") == 0 && i + 1 < argc)
        {
            // Real SDL audio thread + wall clock (catches host IRQ/audio deadlocks).
            scriptPath = argv[++i];
            liveScript = true;
            headless = true; // no window; audio still opened below
        }
        else if (std::strcmp(argv[i], "--scale") == 0 && i + 1 < argc)
            scale = std::max(1, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--headless") == 0)
            headless = true;
        else if (std::strcmp(argv[i], "--song-stress") == 0)
            songStress = true;
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
        gWavRing.assign(kWavRingSamples, 0);
        gWavRingWrite.store(0);
        gWavRingRead.store(0);
        gWavCapture.store(true);
    }

    platform.init(sdRoot.c_str());
    auto startDisplay = tracker->getStartDisplayFunc();
    if (startDisplay)
        startDisplay(&platform);
    tracker->init(&platform);
    std::fprintf(stderr, "tracker-host: sdl-audio-lock + display-budget (2026-11-08d)\n");

    if (!scriptPath.empty() && !liveScript)
    {
        // Headless scripts use virtual time (single-threaded audio).
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

    if (SDL_Init((liveScript ? 0 : SDL_INIT_VIDEO) | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
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
    if (!headless || liveScript)
    {
        audioDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
        if (!audioDev)
            std::fprintf(stderr, "SDL audio open failed: %s (continuing silent)\n", SDL_GetError());
        else
        {
            hostSetAudioDevice(reinterpret_cast<void*>(static_cast<uintptr_t>(audioDev)));
            SDL_PauseAudioDevice(audioDev, 0);
        }
    }

    if (liveScript)
    {
        ScriptRunner runner;
        if (!runner.load(scriptPath.c_str()))
        {
            std::fprintf(stderr, "Failed to load script %s\n", scriptPath.c_str());
            if (audioDev)
                SDL_CloseAudioDevice(audioDev);
            SDL_Quit();
            return 1;
        }
        const bool ok = runner.runLive(platform, tracker);
        if (audioDev)
        {
            hostSetAudioDevice(nullptr);
            SDL_CloseAudioDevice(audioDev);
        }
        SDL_Quit();
        wav.close();
        std::printf("Live script finished (%s)\n", ok ? "ok" : "error");
        return ok ? 0 : 1;
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
        // ARGB8888 matches host framebuffer uint32 0x00RRGGBB layout on little-endian.
        texture = SDL_CreateTexture(renderer,
                                    SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    Ft812Rasterizer::kWidth,
                                    Ft812Rasterizer::kHeight);
    }

    uint32_t lastMs = SDL_GetTicks();
    uint32_t stressStartMs = lastMs;
    int stressPhase = songStress ? 0 : -1;
    while (gRunning)
    {
        // Interactive-path stress: Song + Play/Stop with window+audio (repro freezes).
        if (stressPhase >= 0)
        {
            const uint32_t elapsed = SDL_GetTicks() - stressStartMs;
            auto tap = [&](uint8_t btn) {
                hostLockAudio();
                platform.buttonsImpl().press(btn);
                platform.buttonsImpl().release(btn);
                hostUnlockAudio();
            };
            // 0: wait boot, 1: SONG, 2: PLAY, 3: STOP, 4: PLAY, 5: STOP, 6: done
            if (stressPhase == 0 && elapsed > 5000)
            {
                tap(interfaceButtonSong);
                stressPhase = 1;
                stressStartMs = SDL_GetTicks();
                std::fprintf(stderr, "song-stress: SONG\n");
            }
            else if (stressPhase == 1 && elapsed > 400)
            {
                tap(interfaceButtonPlay);
                stressPhase = 2;
                stressStartMs = SDL_GetTicks();
                std::fprintf(stderr, "song-stress: PLAY1\n");
            }
            else if (stressPhase == 2 && elapsed > 1500)
            {
                tap(interfaceButtonPlay);
                stressPhase = 3;
                stressStartMs = SDL_GetTicks();
                std::fprintf(stderr, "song-stress: STOP1\n");
            }
            else if (stressPhase == 3 && elapsed > 500)
            {
                tap(interfaceButtonPlay);
                stressPhase = 4;
                stressStartMs = SDL_GetTicks();
                std::fprintf(stderr, "song-stress: PLAY2\n");
            }
            else if (stressPhase == 4 && elapsed > 1500)
            {
                tap(interfaceButtonPlay);
                stressPhase = 5;
                stressStartMs = SDL_GetTicks();
                std::fprintf(stderr, "song-stress: STOP2\n");
            }
            else if (stressPhase == 5 && elapsed > 500)
            {
                std::fprintf(stderr, "song-stress: OK\n");
                gRunning = false;
            }
        }

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

                // Hold audio lock only around input that mutates the engine
                // (play/stop/notes). Do not hold it across display rebuilds.
                hostLockAudio();
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
                    // Device Menu is hold-to-view; on host toggle open/close on keydown
                    // so a tap works (Tab is also flaky under macOS SDL focus).
                    if (btn == interfaceButtonMiniMenu)
                    {
                        if (ev.type == SDL_KEYDOWN)
                        {
                            static bool menuLatched = false;
                            if (!menuLatched)
                            {
                                platform.buttonsImpl().press(btn);
                                menuLatched = true;
                            }
                            else
                            {
                                platform.buttonsImpl().release(btn);
                                menuLatched = false;
                            }
                        }
                    }
                    else if (btn == interfaceButtonPlay)
                    {
                        // Tap on keydown only — avoids stuck Play if KEYUP is missed.
                        if (ev.type == SDL_KEYDOWN)
                        {
                            std::fprintf(stderr, "play-tap seq=%d\n",
                                         (int)sequencer.getSeqState());
                            if (platform.buttonsImpl().isButtonPressed(btn))
                                platform.buttonsImpl().release(btn);
                            platform.buttonsImpl().press(btn);
                            platform.buttonsImpl().release(btn);
                            std::fprintf(stderr, "play-tap done seq=%d\n",
                                         (int)sequencer.getSeqState());
                        }
                    }
                    else if (ev.type == SDL_KEYDOWN)
                        platform.buttonsImpl().press(btn);
                    else
                        platform.buttonsImpl().release(btn);
                }
                hostUnlockAudio();
            }
        }

        hostLockAudio();
        platform.update();
        hostUnlockAudio();
        // Sequencer tick lives on the audio thread; UI update can run unlocked.
        tracker->update();
        // Drain a few display steps — but never blow the frame budget. Doing
        // dozens of full-screen step3 rasters per loop (old spin=24) froze the
        // UI during play so Space KEYUP was missed and Play looked wedged.
        {
            const uint32_t drainStart = SDL_GetTicks();
            for (int spin = 0; spin < 6 && !display.isIdle(); ++spin)
            {
                display.update();
                if (SDL_GetTicks() - drainStart >= 8)
                    break;
            }
        }
        wavRingDrain();

        if (!headless && texture && renderer)
        {
            auto& eve = platform.displayImpl().rasterizer();
            if (eve.consumeFrameDirty())
            {
                SDL_UpdateTexture(texture,
                                  nullptr,
                                  eve.pixels(),
                                  Ft812Rasterizer::kWidth * static_cast<int>(sizeof(uint32_t)));
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                SDL_RenderPresent(renderer);
            }
        }

        // Pace ~60 Hz; skip delay if we already spent the budget draining display.
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
    {
        hostSetAudioDevice(nullptr);
        SDL_CloseAudioDevice(audioDev);
    }
    gWavCapture.store(false);
    wavRingDrain();
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
