#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class HeavenSawSynthAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit HeavenSawSynthAudioProcessorEditor(HeavenSawSynthAudioProcessor&);
    ~HeavenSawSynthAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshDisplays();
private:
    class Theme;
    class SpectrumDisplay;
    class MeterDisplay;
    void timerCallback() override;
    void updateAdvanced();
    void showSettings();
    void choosePresetFile(bool save);
    HeavenSawSynthAudioProcessor& audioProcessor;
    std::unique_ptr<Theme> theme;
    std::array<juce::Slider,5> knobs;
    std::array<juce::Slider,8> detailKnobs;
    std::array<std::unique_ptr<HeavenSawSynthAudioProcessor::APVTS::SliderAttachment>,5> attachments;
    std::array<std::unique_ptr<HeavenSawSynthAudioProcessor::APVTS::SliderAttachment>,8> detailAttachments;
    juce::ComboBox preset,pumpRate;
    std::unique_ptr<HeavenSawSynthAudioProcessor::APVTS::ComboBoxAttachment> pumpRateAttachment;
    std::array<juce::TextButton,4> shortcuts;
    juce::TextButton saveButton{"SAVE"},settingsButton{""};
    std::array<juce::TextButton,3> panelButtons;
    juce::ToggleButton advanced{"ADVANCED"};
    std::unique_ptr<SpectrumDisplay> spectrum;
    std::unique_ptr<MeterDisplay> meter;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::array<bool,3> panelsOpen{true,true,true};
    bool showAdvanced=true;
    juce::String status{"16 VOICES   /   STEREO SAW ENGINE"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeavenSawSynthAudioProcessorEditor)
};
