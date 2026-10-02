#pragma once
#include <JuceHeader.h>
#include <array>

// Owned by the audio thread. All voices and stolen-voice tails are allocated at construction.
class HeavenVoiceEngine final {
public:
    explicit HeavenVoiceEngine(juce::AudioProcessorValueTreeState&);
    ~HeavenVoiceEngine();
    void prepare(double sampleRate);
    void handleMidi(const uint8_t* bytes, int byteCount);
    void render(juce::AudioBuffer<float>& output, int start, int count);
    int heldVoiceCount() const noexcept;
    int activeVoiceCount() const noexcept;
private:
    struct Voice;
    struct Settings;
    Settings settings() const;
    void noteOn(int channel, int root, float velocity);
    void noteOff(int channel, int root);
    Voice& allocateVoice();
    juce::AudioProcessorValueTreeState& parameters;
    std::array<std::unique_ptr<Voice>,16> voices;
    std::array<std::atomic<float>*,10> controls{};
    std::array<int,16> wheels{};
    std::array<bool,16> sustain{};
    double rate = 44100.0;
    uint64_t serial = 0;
};
