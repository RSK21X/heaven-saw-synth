#pragma once
#include <JuceHeader.h>
#include <array>

class HeavenVoiceEngine;

class HeavenSawSynthAudioProcessor final : public juce::AudioProcessor {
public:
    HeavenSawSynthAudioProcessor();
    ~HeavenSawSynthAudioProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }
    int getNumPrograms() override { return 40; }
    int getCurrentProgram() override { return currentProgram.load(std::memory_order_relaxed); }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS parameters;
    static APVTS::ParameterLayout createParameterLayout();
    static const std::array<juce::String, 40>& presetNames();
    void loadPreset(int);
    int heldVoiceCount() const noexcept;
    int activeVoiceCount() const noexcept;
    bool saveUserPreset(const juce::File&);
    bool loadUserPreset(const juce::File&);
    void beginVisualisation();
    void endVisualisation();
    int readVisualSamples(float* left, float* right, int maximum);
    std::array<float,2> takeOutputPeaks();
    float getVisualSampleRate() const noexcept { return visualSampleRate.load(std::memory_order_relaxed); }
private:
    void processEffects(int count, double bpm, double ppq, int absoluteOffset, bool clockValid);
    void publishVisuals(const juce::AudioBuffer<float>&);
    std::unique_ptr<HeavenVoiceEngine> engine;
    juce::dsp::Reverb reverb;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Oversampling<float> saturationOversampling{2,1,juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true};
    juce::AudioBuffer<float> synthBuffer;
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> limiterDelay;
    std::array<juce::SmoothedValue<float>,7> effects;
    juce::dsp::LinkwitzRileyFilter<float> crossoverLow, crossoverHigh, lowAlignment;
    struct DensityBand { float envelope=0, gain=1, target=1; };
    std::array<DensityBand,3> densityBands;
    uint64_t densitySamples=0;
    std::array<std::atomic<float>*,14> parameterValues{};
    static constexpr int visualCapacity=32768;
    juce::AbstractFifo visualFifo{visualCapacity};
    std::array<float,visualCapacity> visualLeft{}, visualRight{};
    std::atomic<int> visualReaders{0};
    std::atomic<float> visualSampleRate{44100.0f}, visualPeakL{0.0f}, visualPeakR{0.0f};
    std::vector<float> limiterPeaks;
    std::vector<uint64_t> limiterPeakPositions;
    int limiterHead=0, limiterTail=0;
    uint64_t limiterSamples=0;
    double sampleRate = 44100.0;
    int chunkCapacity=512, lookaheadSamples=1, limiterIndex=0;
    std::atomic<int> currentProgram{0};
    float duckEnvelope = 0.0f;
    std::array<float,2> reverbLow{};
    float limiterGain = 1.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeavenSawSynthAudioProcessor)
};
