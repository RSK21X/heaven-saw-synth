#include "PluginEditor.h"

namespace {
const juce::Colour background{0xff171d21},panelFill{0xff1b2328},line{0xff39434b},pale{0xffe6edf3},muted{0xffa4b2c0};
const juce::Colour purple{0xffbc93f2},amber{0xfff2c566},cyan{0xff7cc8db},blue{0xffa2aff4};
constexpr std::array<const char*,5> macroNames{"SIZE","BRIGHT","BODY","AIR","SPACE"};
constexpr std::array<const char*,5> macroCaptions{"ENSEMBLE","TONE","WEIGHT","SHIMMER","REVERB"};
constexpr std::array<const char*,8> detailNames{"ATTACK","DECAY","SUSTAIN","RELEASE","CUTOFF","DETUNE","OUTPUT","PUMP"};
constexpr std::array<const char*,8> units{"ms","ms","%","ms","Hz","cents","dB","%"};
void text(juce::Graphics& g,juce::String s,juce::Rectangle<float> r,float size,juce::Colour colour=pale,juce::Justification align=juce::Justification::centred) {
    g.setColour(colour);g.setFont(juce::FontOptions(size));g.drawText(s,r,align);
}
void panel(juce::Graphics& g,juce::Rectangle<float> r,juce::String title,juce::Colour accent) {
    g.setColour(panelFill);g.fillRoundedRectangle(r,4);g.setColour(line);g.drawRoundedRectangle(r,4,1);
    g.drawLine(r.getX(),r.getY()+31,r.getRight(),r.getY()+31,1);
    g.setColour(accent);g.fillRect(r.getX()+12,r.getY()+11,10.0f,10.0f);
    text(g,title,{r.getX()+36,r.getY()+1,r.getWidth()-70,29},12,pale,juce::Justification::left);
}
}
class HeavenSawSynthAudioProcessorEditor::Theme final : public juce::LookAndFeel_V4 {
public:
    float scale=1;
    Theme(){
        setColour(juce::PopupMenu::backgroundColourId,panelFill);setColour(juce::PopupMenu::textColourId,pale);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(0xff57416d));
        setColour(juce::PopupMenu::highlightedTextColourId,pale);
        setColour(juce::ComboBox::backgroundColourId,background);setColour(juce::ComboBox::textColourId,pale);
        setColour(juce::ComboBox::outlineColourId,line);setColour(juce::ComboBox::arrowColourId,pale);
        setColour(juce::TextButton::textColourOffId,pale);setColour(juce::TextButton::textColourOnId,background);
        setColour(juce::ToggleButton::textColourId,muted);setColour(juce::ToggleButton::tickColourId,purple);
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,juce::Slider& slider)override{
        const float radius=std::min((float)w,(float)h)*0.5f-10*scale;
        const float cx=(float)x+(float)w*0.5f,cy=(float)y+(float)h*0.5f;
        const float thickness=std::min(8*scale,radius*0.15f);
        for(int i=0;i<=8;++i){const float a=start+(end-start)*(float)i/8;const float s=std::sin(a),c=std::cos(a);
            g.setColour(line.brighter(0.35f));g.drawLine(cx+s*(radius+7*scale),cy-c*(radius+7*scale),cx+s*(radius+11*scale),cy-c*(radius+11*scale),1.5f*scale);}
        juce::Path track;track.addCentredArc(cx,cy,radius,radius,0,start,end,true);
        g.setColour(juce::Colour(0xff474f55));g.strokePath(track,juce::PathStrokeType(thickness,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        const float angle=start+position*(end-start);juce::Path active;active.addCentredArc(cx,cy,radius,radius,0,start,angle,true);
        const auto colour=slider.findColour(juce::Slider::rotarySliderFillColourId);
        g.setColour(colour);g.strokePath(active,juce::PathStrokeType(thickness,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        const float thumb=radius>42*scale?7.2f*scale:6*scale;
        juce::Rectangle<float> dot(cx+std::sin(angle)*radius-thumb,cy-std::cos(angle)*radius-thumb,thumb*2,thumb*2);
        g.setColour(background);g.fillEllipse(dot.expanded(1.5f*scale));g.setColour(pale);g.fillEllipse(dot);
    }
    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider)override{
        auto r=slider.getLocalBounds();const int height=juce::roundToInt(29*scale);
        auto box=r.removeFromBottom(height);box=box.withSizeKeepingCentre(juce::roundToInt((slider.getName()=="SIZE"||slider.getWidth()>110*scale?100.0f:76.0f)*scale),height);
        r.removeFromBottom(5);return {r,box};
    }
    juce::Label* createSliderTextBox(juce::Slider& s)override{
        auto* label=juce::LookAndFeel_V4::createSliderTextBox(s);
        label->setColour(juce::Label::backgroundColourId,background);label->setColour(juce::Label::outlineColourId,line);
        label->setColour(juce::Label::textColourId,pale);label->setColour(juce::TextEditor::backgroundColourId,background);
        label->setColour(juce::TextEditor::textColourId,pale);return label;
    }
    void drawLabel(juce::Graphics& g,juce::Label& label)override{
        g.setColour(label.findColour(juce::Label::backgroundColourId));g.fillRoundedRectangle(label.getLocalBounds().toFloat(),3*scale);
        g.setColour(label.findColour(juce::Label::outlineColourId));g.drawRoundedRectangle(label.getLocalBounds().toFloat().reduced(0.5f),3*scale,scale);
        if(!label.isBeingEdited()){const bool combo=dynamic_cast<juce::ComboBox*>(label.getParentComponent())!=nullptr;text(g,label.getText(),label.getLocalBounds().toFloat().reduced(3),(combo?17.0f:15.0f)*scale,label.findColour(juce::Label::textColourId),combo?juce::Justification::left:juce::Justification::centred);}
    }
    void drawComboBox(juce::Graphics& g,int w,int h,bool,int,int,int,int,juce::ComboBox&)override{
        g.setColour(background);g.fillRoundedRectangle(0,0,(float)w,(float)h,3*scale);
        g.setColour(line);g.drawRoundedRectangle(0.5f,0.5f,(float)w-1,(float)h-1,3*scale,scale);
        juce::Path arrow;const float x=(float)w-21*scale,y=(float)h*0.5f;
        arrow.startNewSubPath(x-6*scale,y-3*scale);arrow.lineTo(x,y+3*scale);arrow.lineTo(x+6*scale,y-3*scale);
        g.setColour(pale);g.strokePath(arrow,juce::PathStrokeType(1.3f*scale));
    }
    juce::Font getComboBoxFont(juce::ComboBox&)override{return juce::Font(juce::FontOptions(17*scale));}
    void positionComboBoxText(juce::ComboBox& box,juce::Label& label)override{
        label.setBounds(juce::roundToInt(12*scale),0,box.getWidth()-juce::roundToInt(40*scale),box.getHeight());label.setFont(getComboBoxFont(box));
        label.setColour(juce::Label::backgroundColourId,juce::Colours::transparentBlack);label.setColour(juce::Label::outlineColourId,juce::Colours::transparentBlack);
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& button,const juce::Colour&,bool over,bool down)override{
        if(button.getName().startsWith("Collapse"))return;
        const auto r=button.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(button.getToggleState()?purple:(down?line:background.brighter(over?0.08f:0.015f)));
        g.fillRoundedRectangle(r,3*scale);g.setColour(button.getToggleState()?purple:line);g.drawRoundedRectangle(r,3*scale,scale);
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& button,bool,bool)override{
        const auto r=button.getLocalBounds().toFloat();g.setColour(pale);
        if(button.getName()=="Settings"){
            const auto c=r.getCentre();juce::Path gear;
            for(int i=0;i<32;++i){const float a=(float)i*juce::MathConstants<float>::twoPi/32,rad=((i%4==0||i%4==3)?10.0f:8.0f)*scale;
                const auto p=c+juce::Point<float>(std::sin(a)*rad,std::cos(a)*rad);if(i==0)gear.startNewSubPath(p);else gear.lineTo(p);}
            gear.closeSubPath();g.strokePath(gear,juce::PathStrokeType(1.25f*scale));g.drawEllipse(c.x-3*scale,c.y-3*scale,6*scale,6*scale,1.2f*scale);return;
        }
        if(button.getName().startsWith("Collapse")){
            const auto c=r.getCentre();const float dy=button.getToggleState()?-3*scale:3*scale;juce::Path p;
            p.startNewSubPath(c.x-7*scale,c.y-dy);p.lineTo(c.x,c.y+dy);p.lineTo(c.x+7*scale,c.y-dy);g.strokePath(p,juce::PathStrokeType(1.3f*scale));return;
        }
        if(button.getButtonText()=="SAVE"){
            const float x=15*scale,y=r.getCentreY()-9*scale;
            g.drawRoundedRectangle(x,y,18*scale,18*scale,1.5f*scale,1.3f*scale);g.drawRect(x+4*scale,y,9*scale,6*scale,1.2f*scale);g.drawRect(x+4*scale,y+11*scale,10*scale,7*scale,1.2f*scale);
            text(g,"SAVE",r.withTrimmedLeft(33*scale),14*scale);return;
        }
        text(g,button.getButtonText(),r,11*scale,button.getToggleState()?background:pale);
    }
    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool,bool)override{
        const float s=19*scale,x=2*scale,y=((float)b.getHeight()-s)*0.5f;
        g.setColour(line.brighter(0.3f));g.drawRoundedRectangle(x,y,s,s,2*scale,scale);
        if(b.getToggleState()){juce::Path p;p.startNewSubPath(x+4*scale,y+10*scale);p.lineTo(x+8*scale,y+14*scale);p.lineTo(x+16*scale,y+5*scale);g.setColour(purple);g.strokePath(p,juce::PathStrokeType(2*scale));}
        text(g,b.getButtonText(),b.getLocalBounds().toFloat().withTrimmedLeft(31*scale),12*scale,muted,juce::Justification::left);
    }
};
class HeavenSawSynthAudioProcessorEditor::SpectrumDisplay final : public juce::Component {
public:
    explicit SpectrumDisplay(HeavenSawSynthAudioProcessor& p):processor(p){setInterceptsMouseClicks(false,false);}
    void advance(){
        int received=0;
        for(int pass=0;pass<8;++pass){const int n=processor.readVisualSamples(readL.data(),readR.data(),nfft);if(!n)break;received+=n;
            for(int i=0;i<n;++i){ringL[(size_t)cursor]=readL[(size_t)i];ringR[(size_t)cursor]=readR[(size_t)i];cursor=(cursor+1)%nfft;}}
        if(received){
            fftL.fill(0);fftR.fill(0);for(int i=0;i<nfft;++i){fftL[(size_t)i]=ringL[(size_t)((cursor+i)%nfft)];fftR[(size_t)i]=ringR[(size_t)((cursor+i)%nfft)];}
            window.multiplyWithWindowingTable(fftL.data(),nfft);window.multiplyWithWindowingTable(fftR.data(),nfft);
            fft.performFrequencyOnlyForwardTransform(fftL.data());fft.performFrequencyOnlyForwardTransform(fftR.data());
        }
        const float sr=processor.getVisualSampleRate();
        for(size_t i=0;i<bars.size();++i){float db=-72;
            if(received){const float lo=20*std::pow(1000.0f,(float)i/(float)bars.size()),hi=20*std::pow(1000.0f,(float)(i+1)/(float)bars.size());
                const int first=juce::jlimit(1,nfft/2-1,(int)std::floor(lo*nfft/sr)),last=juce::jlimit(first,nfft/2-1,(int)std::ceil(hi*nfft/sr));
                float power=0;for(int k=first;k<=last;++k)power=std::max(power,(fftL[(size_t)k]*fftL[(size_t)k]+fftR[(size_t)k]*fftR[(size_t)k])*0.5f);
                db=juce::Decibels::gainToDecibels(std::sqrt(power)*2.0f/nfft,-72.0f);}
            bars[i]=std::max(db,bars[i]-2.0f);
        }
        repaint();
    }
    void paint(juce::Graphics& g)override{
        const float s=(float)getWidth()/1076.0f;g.addTransform(juce::AffineTransform::scale(s));const float h=(float)getHeight()/s;
        panel(g,{0,0,1076,h},"S A W  C L O U D",amber);text(g,"INSTANT HEAVEN",{870,0,190,31},10,muted,juce::Justification::right);
        const juce::Rectangle<float> plot{48,43,1014,h-68};
        for(int db=0;db>=-60;db-=12){const float y=plot.getY()+(float)(-db)/72*plot.getHeight();g.setColour(line.withAlpha(0.4f));g.drawLine(plot.getX(),y,plot.getRight(),y,0.7f);text(g,db==0?"0 dB":juce::String(db),{1,y-7,39,14},10,muted,juce::Justification::right);}
        for(int hz:{50,100,250,1000,4000,16000}){const float x=plot.getX()+std::log((float)hz/20)/std::log(1000.0f)*plot.getWidth();g.setColour(line.withAlpha(0.25f));g.drawLine(x,plot.getY(),x,plot.getBottom(),0.7f);text(g,hz>=1000?juce::String(hz/1000)+"k":juce::String(hz),{x-17,plot.getBottom()+4,34,14},9,muted);}
        for(size_t i=0;i<bars.size();++i){const float x=plot.getX()+((float)i+0.5f)*plot.getWidth()/(float)bars.size();const float top=plot.getBottom()-juce::jlimit(0.0f,1.0f,(bars[i]+72)/72)*plot.getHeight();
            g.setColour(purple.withAlpha(0.8f));g.drawLine(x,plot.getBottom(),x,top,1.2f);}
    }
private:
    HeavenSawSynthAudioProcessor& processor;
    static constexpr int nfft=4096;
    juce::dsp::FFT fft{12};juce::dsp::WindowingFunction<float> window{nfft,juce::dsp::WindowingFunction<float>::hann};
    std::array<float,nfft> ringL{},ringR{},readL{},readR{};
    std::array<float,nfft*2> fftL{},fftR{};
    std::array<float,112> bars=[] {std::array<float,112> b;b.fill(-72);return b;}();
    int cursor=0;
};
class HeavenSawSynthAudioProcessorEditor::MeterDisplay final : public juce::Component {
public:
    void advance(std::array<float,2> peaks){for(size_t i=0;i<2;++i)db[i]=std::max(juce::Decibels::gainToDecibels(peaks[i],-60.0f),db[i]-1.1f);repaint();}
    void paint(juce::Graphics& g)override{
        const float s=(float)getWidth()/90;g.addTransform(juce::AffineTransform::scale(s));const float h=(float)getHeight()/s;
        g.setColour(panelFill);g.fillRoundedRectangle(0,0,90,h,4);g.setColour(line);g.drawRoundedRectangle(0.5f,0.5f,89,h-1,4,1);g.drawLine(0,31,90,31,1);
        text(g,"OUTPUT",{0,0,90,31},11);
        const float bottom=h-21,height=h-62;
        for(int i=0;i<2;++i){const float x=11+(float)i*18;g.setColour(background);g.fillRect(x,41.0f,10.0f,height);
            const float amount=juce::jlimit(0.0f,1.0f,(db[(size_t)i]+48)/48)*height;g.setColour(purple);g.fillRect(x,bottom-amount,10.0f,amount);text(g,i?"R":"L",{x-3,bottom+2,16,16},9,muted);}
        for(int value:{0,-12,-24,-36})text(g,juce::String(value),{52,35+(float)(-value)/48*height,32,14},10,muted);
    }
private:std::array<float,2> db{-60,-60};
};
HeavenSawSynthAudioProcessorEditor::HeavenSawSynthAudioProcessorEditor(HeavenSawSynthAudioProcessor& p)
    :AudioProcessorEditor(&p),audioProcessor(p),theme(std::make_unique<Theme>()),spectrum(std::make_unique<SpectrumDisplay>(p)),meter(std::make_unique<MeterDisplay>()) {
    setLookAndFeel(theme.get());setResizable(true,false);addAndMakeVisible(*spectrum);addAndMakeVisible(*meter);
    preset.setName("Factory preset");preset.setTitle("Factory preset");for(int i=0;i<p.getNumPrograms();++i)preset.addItem(p.getProgramName(i),i+1);
    preset.setSelectedId(p.getCurrentProgram()+1,juce::dontSendNotification);preset.onChange=[this]{if(preset.getSelectedId()>0){audioProcessor.setCurrentProgram(preset.getSelectedId()-1);status="FACTORY PRESET";repaint();}};addAndMakeVisible(preset);
    const char* ids[]{"size","bright","body","air","space"};const std::array<juce::Colour,5> colours{purple,amber,cyan,purple,blue};
    auto configure=[&](juce::Slider& k,const char* id,juce::String name,juce::Colour colour,int decimals){
        k.setName(name);k.setTitle(name);k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);k.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,29);k.setNumDecimalPlacesToDisplay(decimals);
        k.setColour(juce::Slider::rotarySliderFillColourId,colour);k.setColour(juce::Slider::textBoxTextColourId,pale);
        k.setColour(juce::Slider::textBoxBackgroundColourId,background);k.setColour(juce::Slider::textBoxOutlineColourId,line);
        auto* parameter=p.parameters.getParameter(id);k.setDoubleClickReturnValue(true,parameter->convertFrom0to1(parameter->getDefaultValue()));addAndMakeVisible(k);
    };
    for(size_t i=0;i<5;++i){configure(knobs[i],ids[i],macroNames[i],colours[i],2);attachments[i]=std::make_unique<HeavenSawSynthAudioProcessor::APVTS::SliderAttachment>(p.parameters,ids[i],knobs[i]);}
    const char* detailIds[]{"attack","decay","sustain","release","cutoff","detune","output","pump"};const std::array<juce::Colour,8> detailColours{purple,purple,cyan,amber,purple,amber,cyan,purple};
    for(size_t i=0;i<8;++i){configure(detailKnobs[i],detailIds[i],detailNames[i],detailColours[i],i<2||i==3||i==4?0:(i==7?2:1));detailAttachments[i]=std::make_unique<HeavenSawSynthAudioProcessor::APVTS::SliderAttachment>(p.parameters,detailIds[i],detailKnobs[i]);}
    pumpRate.setName("Pump rate");pumpRate.setTitle("Pump rate");pumpRate.addItemList({"1/2","1/4","1/8","OFF"},1);addAndMakeVisible(pumpRate);pumpRateAttachment=std::make_unique<HeavenSawSynthAudioProcessor::APVTS::ComboBoxAttachment>(p.parameters,"pumpRate",pumpRate);
    constexpr std::array<const char*,4> shortcutNames{"AIRY","WARM","WIDE","BRIGHT"};constexpr std::array<int,4> programs{16,19,25,8};
    for(size_t i=0;i<4;++i){shortcuts[i].setButtonText(shortcutNames[i]);shortcuts[i].setName(shortcutNames[i]);shortcuts[i].onClick=[this,i,programs]{audioProcessor.setCurrentProgram(programs[i]);refreshDisplays();status="FACTORY PRESET";repaint();};addAndMakeVisible(shortcuts[i]);}
    saveButton.setName("Save preset");saveButton.onClick=[this]{choosePresetFile(true);};addAndMakeVisible(saveButton);
    settingsButton.setName("Settings");settingsButton.setTitle("Settings");settingsButton.onClick=[this]{showSettings();};addAndMakeVisible(settingsButton);
    for(size_t i=0;i<3;++i){panelButtons[i].setName("Collapse "+juce::String((int)i));panelButtons[i].setTitle("Collapse "+juce::String(i==0?"Envelope":(i==1?"Tone and voice":"Rhythm")));panelButtons[i].setToggleState(true,juce::dontSendNotification);panelButtons[i].onClick=[this,i]{panelsOpen[i]=!panelsOpen[i];panelButtons[i].setToggleState(panelsOpen[i],juce::dontSendNotification);resized();repaint();};addAndMakeVisible(panelButtons[i]);}
    advanced.setToggleState(true,juce::dontSendNotification);advanced.onClick=[this]{updateAdvanced();};addAndMakeVisible(advanced);
    setResizeLimits(976,632,1600,1036);getConstrainer()->setFixedAspectRatio(1220.0/790.0);setSize(1220,790);
    audioProcessor.beginVisualisation();startTimerHz(30);
}
HeavenSawSynthAudioProcessorEditor::~HeavenSawSynthAudioProcessorEditor(){stopTimer();audioProcessor.endVisualisation();setLookAndFeel(nullptr);}
void HeavenSawSynthAudioProcessorEditor::timerCallback(){refreshDisplays();}
void HeavenSawSynthAudioProcessorEditor::refreshDisplays(){
    spectrum->advance();meter->advance(audioProcessor.takeOutputPeaks());const int program=audioProcessor.getCurrentProgram();
    if(preset.getSelectedId()!=program+1)preset.setSelectedId(program+1,juce::dontSendNotification);
    const auto family=audioProcessor.getProgramName(program).upToFirstOccurrenceOf(" -",false,false);
    shortcuts[0].setToggleState(family=="AIRY",juce::dontSendNotification);shortcuts[1].setToggleState(family=="DARK",juce::dontSendNotification);
    shortcuts[2].setToggleState(family=="WIDE",juce::dontSendNotification);shortcuts[3].setToggleState(program==8,juce::dontSendNotification);
}
void HeavenSawSynthAudioProcessorEditor::updateAdvanced(){
    showAdvanced=advanced.getToggleState();const int width=getWidth();const int height=showAdvanced?790:580;
    setResizeLimits(976,juce::roundToInt((float)height*976/1220),1600,juce::roundToInt((float)height*1600/1220));getConstrainer()->setFixedAspectRatio(1220.0/(double)height);
    setSize(width,juce::roundToInt((float)width*(float)height/1220));resized();repaint();
}
void HeavenSawSynthAudioProcessorEditor::choosePresetFile(bool save){
    if(fileChooser)return;
    fileChooser=std::make_unique<juce::FileChooser>(save?"Save Heaven Saw preset":"Load Heaven Saw preset",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Heaven Saw.heavensaw"),"*.heavensaw");
    saveButton.setEnabled(false);settingsButton.setEnabled(false);
    const int flags=save?(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting):(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles);
    fileChooser->launchAsync(flags,[safe=juce::Component::SafePointer<HeavenSawSynthAudioProcessorEditor>(this),save](const juce::FileChooser& chooser){
        if(!safe)return;auto file=chooser.getResult();
        if(file!=juce::File{}){if(save)file=file.withFileExtension("heavensaw");const bool ok=save?safe->audioProcessor.saveUserPreset(file):safe->audioProcessor.loadUserPreset(file);
            safe->status=(ok?(save?"SAVED: ":"LOADED: "):"COULD NOT "+juce::String(save?"SAVE: ":"LOAD: "))+file.getFileName();safe->refreshDisplays();safe->repaint();}
        safe->saveButton.setEnabled(true);safe->settingsButton.setEnabled(true);safe->fileChooser.reset();
    });
}
void HeavenSawSynthAudioProcessorEditor::showSettings(){
    juce::PopupMenu menu;menu.addItem(1,"Load preset…");menu.addItem(2,"Reset to Heaven Saw");menu.addSeparator();
    menu.addItem(3,"Interface size: 80%");menu.addItem(4,"Interface size: 100%");menu.addItem(5,"Interface size: 120%");
    menu.addSeparator();menu.addItem(6,"Show advanced controls",true,showAdvanced);menu.addItem(7,"About Heaven Saw");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settingsButton),[safe=juce::Component::SafePointer<HeavenSawSynthAudioProcessorEditor>(this)](int id){
        if(!safe)return;if(id==1)safe->choosePresetFile(false);if(id==2){safe->audioProcessor.loadPreset(0);safe->refreshDisplays();}
        if(id>=3&&id<=5){const float scale=id==3?0.8f:(id==5?1.2f:1.0f);safe->setSize(juce::roundToInt(1220*scale),juce::roundToInt((safe->showAdvanced?790:580)*scale));}
        if(id==6){safe->advanced.setToggleState(!safe->showAdvanced,juce::dontSendNotification);safe->updateAdvanced();}
        if(id==7){safe->status="HEAVEN SAW " JucePlugin_VersionString "   /   16 VOICES   /   APPLE SILICON";safe->repaint();}
    });
}
void HeavenSawSynthAudioProcessorEditor::paint(juce::Graphics& g){
    g.fillAll(background);g.addTransform(juce::AffineTransform::scale((float)getWidth()/1220));const float height=showAdvanced?790.0f:580.0f;
    juce::ColourGradient wash(juce::Colour(0xff252e34),0,0,background,1220,height,false);g.setGradientFill(wash);g.fillRect(0.0f,0.0f,1220.0f,height);
    // Track the letters with font metrics rather than inserting space glyphs.
    // Reserve the version's own field, ending 16 logical pixels before the divider.
    g.setFont(juce::FontOptions("Helvetica Neue",27,juce::Font::plain).withKerningFactor(0.25f));
    g.setColour(pale);g.drawText("HEAVEN",juce::Rectangle<float>{25,19,159,36},juce::Justification::left);
    g.setColour(purple);g.drawText("SAW",juce::Rectangle<float>{189,19,79,36},juce::Justification::left);
    text(g,"v" JucePlugin_VersionString,{272,30,42,18},10,muted,juce::Justification::right);text(g,"PLAY. INSTANT HEAVEN.",{27,56,290,18},10,muted,juce::Justification::left);
    g.setColour(line);g.drawLine(330,21,330,70,1);g.drawLine(0,88,1220,88,1);g.drawLine(1125,22,1125,69,1);
    panel(g,{20,296,1180,242},"M A C R O  C O N T R O L S",amber);
    for(size_t i=0;i<5;++i){const float centre=138+(float)i*236;text(g,macroNames[i],{centre-80,336,160,22},15);text(g,macroCaptions[i],{centre-90,510,180,18},11,muted);
        if(i<4){g.setColour(line.withAlpha(0.45f));g.drawLine(256+(float)i*236,340,256+(float)i*236,524,1);}}
    if(showAdvanced){
        constexpr std::array<float,3> xs{20,458,854},ws{424,382,346};constexpr std::array<const char*,3> names{"E N V E L O P E","T O N E  &  V O I C E","R H Y T H M"};const std::array<juce::Colour,3> accents{amber,purple,cyan};
        for(size_t i=0;i<3;++i)panel(g,{xs[i],554,ws[i],panelsOpen[i]?200.0f:32.0f},names[i],accents[i]);
        constexpr std::array<float,8> centres{75,178,281,384,525,649,773,1123};
        for(size_t i=0;i<8;++i)if(panelsOpen[i<4?0:(i<7?1:2)]){text(g,detailNames[i],{centres[i]-52,598,104,20},12);text(g,units[i],{centres[i]-45,727,90,18},11,muted);}
        if(panelsOpen[2]){text(g,"PUMP RATE",{876,627,155,20},12,pale,juce::Justification::left);g.setColour(line.withAlpha(0.5f));g.drawLine(1056,609,1056,737,1);}
    }
    g.setColour(line);g.drawLine(0,height-28,1220,height-28,1);text(g,status,{23,height-26,1000,22},10,muted,juce::Justification::left);
}
void HeavenSawSynthAudioProcessorEditor::resized(){
    const float s=(float)getWidth()/1220;theme->scale=s;
    auto place=[s](juce::Component& c,float x,float y,float w,float h){c.setBounds(juce::roundToInt(x*s),juce::roundToInt(y*s),juce::roundToInt(w*s),juce::roundToInt(h*s));};
    place(preset,350,22,281,46);for(size_t i=0;i<4;++i)place(shortcuts[i],647+(float)i*76,26,68,40);
    place(saveButton,963,26,146,40);place(settingsButton,1144,26,43,40);
    place(*spectrum,20,104,1076,176);place(*meter,1110,104,90,176);
    for(size_t i=0;i<5;++i)place(knobs[i],74+(float)i*236,356,128,149);
    constexpr std::array<float,8> centres{75,178,281,384,525,649,773,1123};
    for(size_t i=0;i<8;++i){place(detailKnobs[i],centres[i]-43,620,86,102);detailKnobs[i].setVisible(showAdvanced&&panelsOpen[i<4?0:(i<7?1:2)]);}
    place(pumpRate,876,656,155,43);pumpRate.setVisible(showAdvanced&&panelsOpen[2]);
    constexpr std::array<float,3> ends{444,840,1200};for(size_t i=0;i<3;++i){place(panelButtons[i],ends[i]-43,555,39,29);panelButtons[i].setVisible(showAdvanced);}
    place(advanced,1071,(showAdvanced?790.0f:580.0f)-26,135,23);
}
