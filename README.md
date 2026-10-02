# Heaven Saw Synth

[English](README.md) · [简体中文](README.zh-CN.md)

**A native Apple Silicon VST3 synthesizer for thick, wide saw chords.**

Play the complete chord in your DAW and shape it with a small set of controls. The current version is **1.2.1**, released under [AGPLv3](LICENSE).

![Heaven Saw Synth interface](docs/images/interface.png)

## Download

- [Latest release](https://github.com/RSK21X/heaven-saw-synth/releases/latest)
- [v1.2.1 release and downloads](https://github.com/RSK21X/heaven-saw-synth/releases/tag/v1.2.1)

Download `Heaven-Saw-Synth-v1.2.1-macOS-arm64-VST3.zip`. It contains the `.vst3` plugin and installation instructions. Source code and SHA-256 checksums are attached to the same release.

| Requirement | Supported release |
| --- | --- |
| Operating system | macOS 12 or newer |
| Processor | Apple Silicon, arm64 |
| Plugin format | VST3 instrument |
| Audio output | Stereo |
| Input | MIDI notes from the DAW |

The downloadable build uses a local development signature and is not notarized. Intel Mac, Windows, Linux, AU, and VST2 downloads are not provided.

## Install

1. Quit your DAW and unzip the download.
2. Copy `Heaven Saw Synth.vst3` to one of these folders:
   - All users: `/Library/Audio/Plug-Ins/VST3/`
   - Your account: `~/Library/Audio/Plug-Ins/VST3/`
3. Open your DAW and rescan VST3 plugins if needed.
4. Add **Heaven Saw Synth** to a MIDI/instrument track. Confirm **v1.2.1** appears in the header.
5. Select **INIT - Heaven Saw** or **DROP - Festival Wall**, then play a full chord.

See [INSTALL.md](INSTALL.md) for Chinese and English instructions, updates, and preset files.

## Controls

| Control | What it changes |
| --- | --- |
| SIZE | Ensemble size and stereo spread |
| BRIGHT | Tone brightness |
| BODY | Low and middle harmonic weight |
| AIR | Shimmer from the upper octave layer |
| SPACE | Reverb amount |
| PUMP | Rhythmic ducking while the DAW is playing |

- **40 factory presets**, with AIRY, WARM, WIDE, and BRIGHT shortcuts.
- **Envelope:** attack, decay, sustain, and release.
- **Tone & Voice:** cutoff, detune, and output level.
- **Rhythm:** pump depth and divisions of 1/2, 1/4, and 1/8, or OFF. Pump follows the DAW tempo and playback position.
- **SAVE** exports a `.heavensaw` preset. The gear menu loads presets, resets the sound, and sets the interface to 80%, 100%, or 120%.
- Edit knob values directly, or double-click a knob to restore its default. Panel arrows collapse individual panels; **ADVANCED** hides or reveals the lower panels.
- The spectrum and left/right meters display the actual plugin output.

**CHORD mode was removed in v1.2.0.** Each MIDI key starts one note. To play a chord, send every chord note from the DAW. Older sessions retain the other sound settings; their former CHORD settings are ignored.

## Sound engine

Sixteen voices combine a central saw/square/triangle layer, a wide saw ensemble, and a separate octave saw layer. Processing includes shaped detune, frequency-dependent stereo width, density processing, chorus, ducked reverb, tempo-synced pump, and a lookahead output limiter. Sustain and sostenuto pedals and pitch bend are supported.

## Build from source

Requirements: macOS 12+, Apple Silicon, Xcode command-line tools, CMake 3.22+, Git, and network access for the first configure. CMake fetches **JUCE 8.0.12**.

```sh
git clone https://github.com/RSK21X/heaven-saw-synth.git
cd heaven-saw-synth
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target HeavenSawSynth_VST3 -j4
```

The plugin is created at:

```text
build/HeavenSawSynth_artefacts/Release/VST3/Heaven Saw Synth.vst3
```

The macOS build signs and verifies the completed development bundle after generating its VST3 description.

## Verification

Version 1.2.1 passed **24 regression checks** and **pluginval 1.0.4 at strictness 8**. Checks cover audio processing, MIDI timing and releases, preset files, session recall, sound controls, real spectrum/meter data, and the header text at three sizes and two pixel densities.

- Sample rates: 44.1, 48, 88.2, and 96 kHz.
- Buffer sizes: 32, 64, 128, 256, 512, and 1024 samples.
- The previous 1.2.0 revision produced nine audio renders identical to 1.1.0. Version 1.2.1 changes header typography and bundle signing.
- Bundle architecture, signature, installed files, and download archives were checked.

To run the local checks:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHEAVEN_BUILD_AUDIO_SMOKE=ON
cmake --build build --config Release --target HeavenSawSmoke -j4
mkdir -p snapshots
./build/HeavenSawSmoke_artefacts/Release/HeavenSawSmoke "$(pwd)/snapshots"
```

Validation records are in [validation/](validation/). The separate Steinberg validator was unavailable. These results do not establish compatibility with every DAW or a listening match to a particular commercial track.

## Updates and dependencies

See [CHANGELOG.md](CHANGELOG.md), [FONT_UPDATE.md](FONT_UPDATE.md), [UI_UPDATE.md](UI_UPDATE.md), [SOUND_UPDATE.md](SOUND_UPDATE.md), and [REVIEW.md](REVIEW.md).

JUCE is dual-licensed under AGPLv3 and its commercial licence. Its notice and the VST3 SDK notice are preserved in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The project's original source is licensed under [AGPLv3, version 3 only](LICENSE); see [NOTICE](NOTICE).
