# Review of the initial prototype

Reviewed target: the complete initial implementation. This directory has no Git history; all implementation files were reviewed as newly added code. Line references below describe the original source snapshot, before the fixes in this pass.

[P1] Construct controls before sizing the editor — Source/PluginEditor.cpp:24

`setSize()` immediately calls the editor's `resized()` callback. That callback dereferences every entry in `knobs`, but none of the sliders exists yet. Opening the custom editor in a host can therefore crash the DAW.

[P1] Keep generated notes owned by their input key — Source/PluginProcessor.cpp:170

Generated chord tones are sent through ordinary MIDI note numbers. Two held input keys can generate the same tone; starting the second chord replaces that tone and releasing either key releases it for both. Retriggering a key after disabling chord mode also leaves the old chord mapping in place and can release the wrong notes. Voices must retain the identity of the input key that owns them.

[P1] Prepare and reset reverb at the host sample rate — Source/PluginProcessor.cpp:155

The reverb receives parameters but never receives its `prepare()` call. Its delay network therefore keeps its default sample rate, changing timing and decay at 48, 88.2, and 96 kHz. Re-preparing the processor also leaves old effect tails alive.

[P1] Process variable buffers within preallocated capacity — Source/PluginProcessor.cpp:162

Buffers are resized inside the audio callback while oversampling was allocated only for the size passed to `prepareToPlay()`. A larger host buffer can allocate on the audio thread and overrun the oversampling workspace. The expanding MIDI buffer can allocate too, especially for dense generated chords or copied SysEx. Process bounded chunks and consume supported MIDI without growing temporary buffers.

[P2] Honour the pitch wheel value on note-on — Source/PluginProcessor.cpp:28

The current pitch wheel value passed to `startNote()` is ignored and every new note starts with zero bend. Holding the wheel before playing a note therefore produces an unbent attack until another wheel event arrives.

[P2] Fade stolen voices and clear reused filter state — Source/PluginProcessor.cpp:25

Voice stealing immediately replaces a sounding voice while its filter memories remain nonzero. The next note starts with the previous voice's residual output, which can produce clicks under repeated notes or more than 16 sounding tones.

[P2] Report oversampling delay to the host — Source/PluginProcessor.cpp:154

The oversampling filter delays all output, but the processor reports zero latency. The DAW cannot align the instrument with other tracks or preserve timing when compensating plugins.

[P2] Make release duration agree with the reported tail — Source/PluginProcessor.cpp:75

Release is implemented as an exponential time constant, rather than the selected release duration. The default 1.4-second release takes roughly 13 seconds to become quiet, while the host is told the whole instrument has a nine-second tail. Long-release presets can therefore be cut off during export.

[P2] Smooth global effects and output automation — Source/PluginProcessor.cpp:209

SPACE, PUMP, stereo width, and output gain jump directly between block values. Abrupt host automation or preset changes can therefore produce discontinuities even though the oscillator macros are smoothed. Smooth the final effect mix, width, and output gain per sample.

[P2] Duck only the wet reverb and allow SPACE zero to be dry — Source/PluginProcessor.cpp:221

The reverb output contains 82% dry signal, then a full dry signal is subtracted before ducking. Consequently ducking changes the dry level as well as the tail. SPACE zero also retains a nonzero wet level. Render a wet-only reverb path, then mix it with the unchanged dry signal.

[P2] Restore complete factory settings when loading a preset — Source/PluginProcessor.cpp:241

Factory loading updates only macros and chord settings. Edited envelopes, filter, output, and pump rate carry into the next preset, so selecting the same factory preset no longer reliably produces the same settings. The last two CHORD presets also leave chord mode disabled. Restore all stored factory parameters and apply the correct chord modes.

[P2] Connect the exposed Detune parameter to the sound engine — Source/PluginProcessor.cpp:138

The Detune parameter is serialized and exposed to host automation, but it is never read in the voice engine. Moving it has no audible effect.

[P2] Label advanced controls and keep host-driven selectors synchronized — Source/PluginEditor.cpp:29

Slider `setName()` supplies accessibility text but does not draw a visible label. The advanced sliders are therefore unlabeled. Chord selection bypasses the normal parameter attachment and host gesture handling; host program changes also leave the preset selector stale. Draw the labels, use a parameter attachment for chord choice, and synchronize the displayed program.

[P2] Validate restored state and protect the program index — Source/PluginProcessor.cpp:246

The restored program index is accepted without bounds checks, and state values are trusted without validating non-finite or out-of-range input. The program index is also shared between host and editor callbacks without atomic access. A damaged state can leave invalid settings, and concurrent program changes can race with editor reads.

[P2] Replace the smoke check with independent behavioral checks — tests/audio_smoke.cpp:43

The state check compares a default program index of zero against another default zero and never checks restored parameter values. The chord check runs with the previous note still sounding, so it can succeed without demonstrating correct chord generation. Neither check opens the editor or releases the MIDI notes. These gaps allowed the editor crash and note ownership defects to pass.

[P2] Use correctly decoded display text — Source/PluginProcessor.cpp:116

The original preset names contain UTF-8 em dashes in narrow string literals, which JUCE's ASCII string constructor misreads. The opened editor shows corrupted separators. The same issue appeared in the added advanced labels during visual verification. Display separators now use ASCII, and the final screenshots were checked again.

## Fixes completed in 1.0.1

All 16 findings above were addressed. Controls are allocated before sizing; the fixed-size voice engine records the owning input key; reverb is prepared and reset; oversized callbacks are processed in bounded chunks; pitch bend initializes correctly; stolen voices fade and filters reset; latency includes oversampling and limiter lookahead; releases end within their configured duration; final effect/gain changes are smoothed; ducking affects a wet-only reverb path; factory programs reset every parameter; Detune changes the sound; advanced controls have visible labels and attached selectors; state values and program indices are validated; and the checks now exercise actual behavior. Display separators were corrected after inspecting the rendered editor.

## Verification

- Release build: passed for the arm64 VST3 and regression executable.
- Regression checks: 14 passed, zero failed.
- Editor: creation, advanced expansion/collapse, and both rendered views inspected.
- Audio: 44.1, 48, 88.2, and 96 kHz; 32, 64, 128, 256, 512, and 1024 samples; oversized and zero-frame callbacks included.
- pluginval 1.0.4: strictness 8, seed 1234, the same sample rates and buffers; result SUCCESS. The actual VST3 passed editor, processing, state, automation, background-state, thread-safety, and parameter-fuzz checks.
- Bundle: arm64 architecture, valid Info.plist, and strict ad-hoc code-signature verification.
- System installation: `/Library/Audio/Plug-Ins/VST3/Heaven Saw Synth.vst3`, version 1.0.1. Every installed file matches the validated build; the installed signature was verified.
- Separate Steinberg validator: unavailable, skipped. AU checks do not apply to this VST3-only target.

Overall assessment: the reviewed build is ready for the requested DAW test. These checks do not establish listening quality, compatibility with every DAW, full PRD coverage, commercial release readiness, or performance on an M1. Those remaining areas are outside the concrete defects repaired here.

## Subsequent sound revision: 1.1.0

The 1.0.1 checks above established runtime correctness, not the PRD sound target. The user's DAW feedback exposed insufficient width and harmonic layering. The engine, macro mapping, density processing and presets were subsequently revised. See [SOUND_UPDATE.md](SOUND_UPDATE.md) for measurements, reference-analysis limits and installation. The current 1.1.0 build passed 20 regression checks and final pluginval strictness 8. Historical 1.0.1 logs are preserved in validation/v1.0.1; current logs are in validation.

## Subsequent interface revision: 1.2.0

The reference interface has been implemented and CHORD generation removed. UI_UPDATE.md records the review and checks. Remaining parameter IDs are unchanged; obsolete chord settings cannot re-enable automatic notes. Historical 1.1.0 evidence is in validation/v1.1.0. Current 1.2.0 checks and install records are in validation.

## Header correction: 1.2.1

The version/divider overlap was reproduced by a pixel regression check, then corrected. The full change was reviewed without further findings. All 24 checks and pluginval strictness 8 passed. The header was visually inspected at three sizes and two pixel densities. See FONT_UPDATE.md; previous 1.2.0 evidence is archived in validation/v1.2.0.
