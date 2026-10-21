# GritBox: guitar distortion VST3

A distortion plugin built for electric guitar, made to run in FL Studio (or any DAW that loads VST3).

## Get the plugin file (Windows, no coding)

You don't need to install any programming tools. GitHub builds it for you for free.

1. Make a free account at github.com and click **New repository** (name it `GritBox`, set it to Private if you like).
2. On the new repo page click **uploading an existing file**, then drag in everything from this folder
   (including the hidden `.github` folder, which holds the build recipe) and click **Commit changes**.
   - Easiest way to make sure `.github` goes up: use **GitHub Desktop** (desktop.github.com), "Add local repository", then Publish.
3. Click the **Actions** tab. A build called "Build GritBox" starts by itself (about 5–10 minutes).
4. When it has a green check, open it and scroll to **Artifacts** at the bottom.
   Download **GritBox-Windows-VST3**. (Also grab **GritBox-Windows-Standalone** if you want to test it outside FL.)
5. Unzip it. You'll get a folder named `GritBox.vst3`.

## Install in FL Studio

1. Copy the whole `GritBox.vst3` folder into: `C:\Program Files\Common Files\VST3\`
   (it'll ask for admin permission, say yes).
2. Open FL Studio: **Options > Manage plugins**, click **Find plugins** (or "Start scan").
   Tick "Verify plugins" if it's off. GritBox should show up under **Installed > Effects**.
3. Star it (click the star by the name) so it's easy to find.
4. Open a **Mixer** channel, click an empty effect slot, and pick **GritBox**.
5. Set up your guitar input: **Options > Audio settings**. Pick an ASIO driver
   (your audio interface's, or **FL Studio ASIO**), then on the mixer track set the **Input** dropdown
   (bottom right of the mixer) to the interface input your guitar is in.
   Press the little record/monitor icon to hear yourself through it.

Tip: if you hear lag, lower the buffer size in the audio settings (try 128 or 256 samples).
GritBox itself only adds about 0.13 ms.

## The knobs

| Knob | What it does |
|---|---|
| **Mode** | Crunch = soft and warm, Lead = singing sustain, Fuzz = fat and ragged |
| **Drive** | How hard the signal gets squashed. More = more saturated and sustain |
| **Tight** | Cuts low rumble *before* the distortion. Higher = tighter, chunkier palm mutes. Lower = fatter |
| **Tone** | Dark to bright |
| **Gate** | Kills hiss and hum between notes. Off at the far left. Raise it until the noise stops |
| **Level** | Output volume |
| **Mix** | Blend of dry guitar and distortion. 100% = all distortion |
| **Cab Sim** | Rolls off harsh highs like a real guitar speaker. Leave ON when plugging straight into an interface |

Double-click any knob to reset it.

Good starting points:
- **Classic rock crunch**: Mode Crunch, Drive 4, Tight 90, Tone 5
- **Lead**: Mode Lead, Drive 6.5, Tight 120, Tone 5.5
- **Heavy rhythm**: Mode Lead, Drive 7, Tight 220, Gate around -55 dB
- **Fuzz**: Mode Fuzz, Drive 5, Tight 70, Cab Sim off for the sizzly version

## What's inside

Signal chain: noise gate, tight filter (24 dB/oct), mid push at 750 Hz, **4x oversampling**,
the waveshaper, DC blocker, tone filter, cab filter, level, then dry/wet mix.
The oversampling stops the harsh, "digital fizz" aliasing that cheap distortion plugins have.
The dry signal is delayed by the same amount as the wet one, so the Mix knob never causes phase smear.

All the sound lives in `Source/GritEngine.h`. The three distortion curves are in the `shape()` function if you want to experiment.

## Building yourself (optional)

Needs CMake 3.22+ and a C++17 compiler (Visual Studio 2022 on Windows).

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

The first run downloads JUCE automatically. Output ends up in `build/GritBox_artefacts/Release/`.
