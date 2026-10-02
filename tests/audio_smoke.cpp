#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>

using Synth=HeavenSawSynthAudioProcessor;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static void set(Synth& synth,const char* id,float raw){auto* p=synth.parameters.getParameter(id);require(p!=nullptr,"Missing parameter");p->setValueNotifyingHost(p->convertTo0to1(raw));}
static float get(Synth& synth,const char* id){return synth.parameters.getRawParameterValue(id)->load();}
static juce::MidiMessage on(int note){return juce::MidiMessage::noteOn(1,note,(juce::uint8)100);}
static void process(Synth& synth,int samples,const std::vector<std::pair<juce::MidiMessage,int>>& events={}){
    juce::AudioBuffer<float> audio(2,samples);juce::MidiBuffer midi;
    for(const auto& event:events)midi.addEvent(event.first,event.second);
    synth.processBlock(audio,midi);
    for(int c=0;c<2;++c)for(int i=0;i<samples;++i){const float v=audio.getSample(c,i);require(std::isfinite(v),"Non-finite audio");require(std::abs(v)<=0.944062f,"Safety ceiling exceeded");}
}
static void settle(Synth& synth,int samples,int block=128){for(int done=0;done<samples;done+=block)process(synth,std::min(block,samples-done));}
static void dry(Synth& synth){set(synth,"space",0);set(synth,"release",50);}
static std::vector<float> render(Synth& synth,int samples,int block,const std::vector<std::pair<juce::MidiMessage,int>>& events){
    std::vector<float> result;result.reserve((size_t)samples*2);
    for(int start=0;start<samples;start+=block){const int n=std::min(block,samples-start);juce::AudioBuffer<float> audio(2,n);juce::MidiBuffer midi;for(const auto& event:events)if(event.second>=start&&event.second<start+n)midi.addEvent(event.first,event.second-start);synth.processBlock(audio,midi);for(int i=0;i<n;++i){result.push_back(audio.getSample(0,i));result.push_back(audio.getSample(1,i));}}
    return result;
}
static float difference(const std::vector<float>& a,const std::vector<float>& b){require(a.size()==b.size(),"Audio lengths differ");float max=0;for(size_t i=0;i<a.size();++i)max=std::max(max,std::abs(a[i]-b[i]));return max;}
struct Metrics {
    double power=0,mid=0,side=0,correlation=0;
};
static Metrics measure(const std::vector<float>& audio) {
    Metrics result;double left=0,right=0,cross=0;
    const size_t begin=48000*2;
    require(audio.size()>begin,"Measurement needs more than one second");
    for(size_t i=begin;i<audio.size();i+=2){
        const double l=audio[i],r=audio[i+1];
        left+=l*l;right+=r*r;cross+=l*r;
        result.mid+=(l+r)*(l+r)*0.25;result.side+=(l-r)*(l-r)*0.25;
    }
    const double samples=(double)((audio.size()-begin)/2);
    result.power=(left+right)/(2*samples);result.mid/=samples;result.side/=samples;
    result.correlation=cross/std::sqrt(std::max(1.0e-30,left*right));
    return result;
}
static double bandFraction(const std::vector<float>& audio,float low,float high) {
    constexpr int order=14,n=1<<order;
    juce::dsp::FFT fft(order);juce::dsp::WindowingFunction<float> window(n,juce::dsp::WindowingFunction<float>::hann);
    double selected=0,total=0;
    std::vector<float> data(n*2);
    for(int second:{1,2,3})for(size_t channel=0;channel<2;++channel){
        const size_t start=(size_t)second*48000;
        require((start+n)*2<=audio.size(),"Spectral measurement needs four seconds");
        std::fill(data.begin(),data.end(),0.0f);
        for(size_t i=0;i<n;++i)data[i]=audio[(start+i)*2+channel];
        window.multiplyWithWindowingTable(data.data(),n);fft.performFrequencyOnlyForwardTransform(data.data());
        for(int i=1;i<n/2;++i){const double power=(double)data[(size_t)i]*data[(size_t)i];const float hz=(float)i*48000.0f/n;total+=power;if(hz>=low&&hz<high)selected+=power;}
    }
    return selected/std::max(total,1.0e-30);
}
static std::vector<float> sound(float size=72,float body=50,float air=58) {
    Synth synth;set(synth,"size",size);set(synth,"body",body);set(synth,"air",air);set(synth,"space",0);
    synth.prepareToPlay(48000,128);
    return render(synth,48000*4,128,{{on(48),0},{on(55),0},{on(60),0},{on(64),0}});
}
struct Clock final:juce::AudioPlayHead {
    double bpm=128,ppq=3.375;bool playing=true;
    juce::Optional<PositionInfo> getPosition()const override{PositionInfo info;info.setBpm(bpm);info.setPpqPosition(ppq);info.setIsPlaying(playing);return info;}
};
static std::vector<float> pumped(int block){
    Synth synth;dry(synth);set(synth,"pump",80);synth.prepareToPlay(48000,block);Clock clock;synth.setPlayHead(&clock);
    std::vector<float> result;result.reserve(16384);
    for(int done=0;done<8192;done+=block){const int n=std::min(block,8192-done);clock.ppq=3.375+(double)done/48000*128/60;juce::AudioBuffer<float> audio(2,n);juce::MidiBuffer midi;if(done==0)midi.addEvent(on(60),0);synth.processBlock(audio,midi);for(int i=0;i<n;++i){result.push_back(audio.getSample(0,i));result.push_back(audio.getSample(1,i));}}
    synth.setPlayHead(nullptr);return result;
}
static void image(juce::AudioProcessorEditor& editor,const juce::File& path){auto snapshot=editor.createComponentSnapshot(editor.getLocalBounds(),true,1.0f);juce::MemoryOutputStream stream;juce::PNGImageFormat png;require(png.writeImageToStream(snapshot,stream),"Could not encode screenshot");require(path.replaceWithData(stream.getData(),stream.getDataSize()),"Could not replace screenshot file");}

int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI gui;
    int failures=0,passes=0;
    const auto run=[&](const char* name,const std::function<void()>& test){try{test();++passes;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failures;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
    run("Reference layout, shortcuts, collapse, scales, and live snapshots",[&]{
        Synth synth;synth.prepareToPlay(48000,128);std::unique_ptr<juce::AudioProcessorEditor> editor(synth.createEditor());require(editor&&editor->getWidth()==1220&&editor->getHeight()==790,"Editor failed to open");
        auto* ui=dynamic_cast<HeavenSawSynthAudioProcessorEditor*>(editor.get());require(ui!=nullptr,"Wrong editor type");
        juce::Button* advanced=nullptr;
        for(int i=0;i<editor->getNumChildComponents();++i){auto* c=editor->getChildComponent(i);require(!c->getName().containsIgnoreCase("chord"),"Chord control remains");
            if(auto* b=dynamic_cast<juce::Button*>(c)){if(b->getButtonText()=="ADVANCED")advanced=b;if(b->getButtonText()=="AIRY"){b->onClick();require(synth.getCurrentProgram()==16,"AIRY shortcut failed");}}
        }
        for(const auto& shortcut:std::array<std::pair<const char*,int>,4>{{{"AIRY",16},{"WARM",19},{"WIDE",25},{"BRIGHT",8}}}){
            bool found=false;for(int i=0;i<editor->getNumChildComponents();++i)if(auto* b=dynamic_cast<juce::Button*>(editor->getChildComponent(i)))if(b->getButtonText()==shortcut.first){b->onClick();found=true;require(synth.getCurrentProgram()==shortcut.second,"Preset shortcut selected wrong program");}require(found,"Preset shortcut missing");
        }
        synth.loadPreset(16);ui->refreshDisplays();
        for(int i=0;i<editor->getNumChildComponents();++i)if(auto* k=dynamic_cast<juce::Slider*>(editor->getChildComponent(i)))if(k->getName()=="BODY"){k->setValue(63.25,juce::sendNotificationSync);require(std::abs(get(synth,"body")-63.25f)<0.01f,"Knob did not update processor");set(synth,"body",38);}
        require(advanced!=nullptr,"Advanced button missing");
        process(synth,128,{{on(48),0},{on(55),0},{on(60),0},{on(64),0}});
        for(int i=0;i<12;++i){settle(synth,1600);ui->refreshDisplays();}
        if(argc>1)image(*editor,juce::File(argv[1]).getChildFile("heaven-main.png"));
        for(int i=0;i<editor->getNumChildComponents();++i)if(auto* b=dynamic_cast<juce::Button*>(editor->getChildComponent(i)))if(b->getName()=="Collapse 0"){b->onClick();require(!b->getToggleState(),"Panel failed to collapse");b->onClick();require(b->getToggleState(),"Panel failed to reopen");}
        advanced->setToggleState(false,juce::dontSendNotification);advanced->onClick();require(editor->getHeight()==580,"Advanced panel failed to collapse");
        if(argc>1)image(*editor,juce::File(argv[1]).getChildFile("heaven-compact.png"));
        advanced->setToggleState(true,juce::dontSendNotification);advanced->onClick();require(editor->getHeight()==790,"Advanced panel failed to expand");
        for(int width:{976,1464}){editor->setSize(width,juce::roundToInt((float)width*790/1220));if(argc>1)image(*editor,juce::File(argv[1]).getChildFile("heaven-scale-"+juce::String(width)+".png"));}
    });
    run("Header text stays clear of the divider at all sizes and pixel densities",[&]{
        Synth synth;std::unique_ptr<juce::AudioProcessorEditor> editor(synth.createEditor());
        for(int width:{976,1220,1464})for(float density:{1.0f,2.0f}){
            editor->setSize(width,juce::roundToInt((float)width*790/1220));const float scale=(float)width/1220*density;
            const auto shot=editor->createComponentSnapshot(editor->getLocalBounds(),true,density);
            bool versionVisible=false;
            for(int y=juce::roundToInt(30*scale);y<juce::roundToInt(48*scale);++y){
                for(int x=juce::roundToInt(273*scale);x<juce::roundToInt(314*scale);++x)if(shot.getPixelAt(x,y).getRed()>110)versionVisible=true;
                for(int x=juce::roundToInt(318*scale);x<juce::roundToInt(327*scale);++x)require(shot.getPixelAt(x,y).getRed()<110,"Version text overlaps divider clearance");
            }
            require(versionVisible,"Version text disappeared or was clipped");
            if(argc>1){const auto path=juce::File(argv[1]).getChildFile("heaven-header-"+juce::String(width)+"-"+juce::String((int)density)+"x.png");
                auto crop=shot.getClippedImage({0,0,juce::roundToInt(350*scale),juce::roundToInt(88*scale)});juce::PNGImageFormat png;juce::MemoryOutputStream stream;require(png.writeImageToStream(crop,stream),"Header screenshot encoding failed");require(path.replaceWithData(stream.getData(),stream.getDataSize()),"Header screenshot saving failed");}
        }
    });
    run("Sample rates, buffer sizes, and finite output",[]{
        for(double rate:{44100.0,48000.0,88200.0,96000.0})for(int size:{32,64,128,256,512,1024}){
            Synth synth;dry(synth);synth.prepareToPlay(rate,size);process(synth,size,{{on(48),0},{on(55),size/4},{on(60),size/2},{on(64),size*3/4}});settle(synth,(int)(rate*0.10),size);process(synth,size,{{juce::MidiMessage::allNotesOff(1),0}});settle(synth,(int)(rate*0.10),size);require(synth.activeVoiceCount()==0,"Voices failed to release");
        }
    });
    run("Manually played notes retain independent ownership",[]{
        Synth synth;dry(synth);synth.prepareToPlay(48000,128);
        process(synth,128,{{on(60),0},{on(64),0},{on(67),0}});require(synth.heldVoiceCount()==3,"Manual chord has wrong note count");
        process(synth,128,{{on(72),0}});require(synth.heldVoiceCount()==4,"New note stopped previous notes");
        process(synth,128,{{juce::MidiMessage::noteOff(1,60),0}});require(synth.heldVoiceCount()==3,"One note-off stopped unrelated notes");
        process(synth,128,{{juce::MidiMessage::allNotesOff(1),0}});settle(synth,4800);require(synth.activeVoiceCount()==0,"Manual notes stuck");
    });
    run("One key creates one voice and retriggers release correctly",[]{
        Synth synth;dry(synth);synth.prepareToPlay(48000,128);process(synth,128,{{on(60),0}});require(synth.heldVoiceCount()==1,"A key generated extra notes");
        process(synth,128,{{on(60),0}});require(synth.heldVoiceCount()==1,"Retrigger retained old held note");
        process(synth,128,{{juce::MidiMessage::noteOff(1,60),0}});settle(synth,4800);require(synth.activeVoiceCount()==0,"Retrigger note stuck");
    });
    run("Sustain and panic messages release manually played notes",[]{
        Synth synth;dry(synth);synth.prepareToPlay(48000,128);
        process(synth,128,{{on(60),0},{on(64),0},{juce::MidiMessage::controllerEvent(1,64,127),10},{juce::MidiMessage::noteOff(1,60),20},{juce::MidiMessage::noteOff(1,64),20}});settle(synth,4800);require(synth.activeVoiceCount()==2,"Sustain did not hold notes");
        process(synth,128,{{juce::MidiMessage::controllerEvent(1,64,0),0}});settle(synth,4800);require(synth.activeVoiceCount()==0,"Pedal-up left stuck notes");
        process(synth,128,{{on(60),0},{juce::MidiMessage::allSoundOff(1),64}});require(synth.activeVoiceCount()==0,"All-sound-off failed");
    });
    run("Pitch wheel before note-on matches wheel after note-on",[]{
        Synth a,b;dry(a);dry(b);a.prepareToPlay(48000,128);b.prepareToPlay(48000,128);
        const auto before=render(a,4096,128,{{juce::MidiMessage::pitchWheel(1,16383),0},{on(69),0}});
        const auto after=render(b,4096,128,{{on(69),0},{juce::MidiMessage::pitchWheel(1,16383),0}});
        require(difference(before,after)<0.00001f,"Pre-held bend was ignored");
    });
    run("MIDI note offset leaves the start of the block silent",[]{
        Synth synth;dry(synth);synth.prepareToPlay(48000,512);const auto audio=render(synth,2048,512,{{on(60),317}});
        for(size_t i=0;i<317*2;++i)require(std::abs(audio[i])<0.000001f,"Note began before its sample offset");
        float peak=0;for(float v:audio)peak=std::max(peak,std::abs(v));require(peak>0.005f,"Offset note never sounded");
    });
    run("Oversized buffers match bounded processing",[]{
        Synth a,b;dry(a);dry(b);a.prepareToPlay(48000,32);b.prepareToPlay(48000,1024);
        const std::vector<std::pair<juce::MidiMessage,int>> events{{on(48),0},{on(64),173},{juce::MidiMessage::noteOff(1,48),1703}};
        const auto oversized=render(a,4096,1024,events),bounded=render(b,4096,1024,events);
        require(difference(oversized,bounded)<0.0002f,"Chunk processing changed MIDI timing or sound");
        juce::AudioBuffer<float> empty(2,0);juce::MidiBuffer midi;midi.addEvent(on(70),0);a.processBlock(empty,midi);require(a.heldVoiceCount()>0,"Zero-length callback discarded MIDI");
    });
    run("Host PUMP sync remains consistent across block sizes",[]{require(difference(pumped(32),pumped(512))<0.0002f,"Pump depends on host buffer size");});
    run("Every parameter and the selected program survive state recall",[]{
        Synth source;source.loadPreset(22);set(source,"attack",817);set(source,"release",2100);set(source,"detune",83);set(source,"output",-4);set(source,"pump",75);
        juce::MemoryBlock data;source.getStateInformation(data);Synth restored;restored.setStateInformation(data.getData(),(int)data.getSize());
        require(restored.getCurrentProgram()==22,"Selected program was not restored");
        for(auto* base:source.getParameters())if(auto* p=dynamic_cast<juce::RangedAudioParameter*>(base))require(std::abs(p->getValue()-restored.parameters.getParameter(p->paramID)->getValue())<0.00001f,"Parameter state was not restored");
        restored.setStateInformation("broken",6);require(restored.getCurrentProgram()==22,"Corrupt state destroyed the current program");
        auto state=source.parameters.copyState();state.setProperty("program",999999,nullptr);state.getChildWithProperty("id","size").setProperty("value","nan",nullptr);state.getChildWithProperty("id","body").setProperty("value",9999,nullptr);
        const auto xml=state.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,data);restored.setStateInformation(data.getData(),(int)data.getSize());
        require(restored.getCurrentProgram()==39,"Invalid program index was not clamped");require(std::abs(get(restored,"size")-72)<0.001f,"Non-finite state was not rejected");require(get(restored,"body")<=100,"State range was not enforced");
    });
    run("Factory presets restore settings with no chord parameters",[]{
        Synth synth;set(synth,"attack",1800);set(synth,"release",12000);set(synth,"pumpRate",3);set(synth,"output",6);synth.loadPreset(0);
        require(std::abs(get(synth,"attack")-15)<0.001f&&std::abs(get(synth,"release")-1400)<0.001f,"Factory preset retained edited envelopes");require(std::abs(get(synth,"output"))<0.001f&&std::abs(get(synth,"pumpRate")-1)<0.001f,"Factory preset retained output or pump rate");
        require(synth.parameters.getParameter("chord")==nullptr&&synth.parameters.getParameter("chordType")==nullptr,"Chord parameters remain");
        for(int i=0;i<40;++i){synth.loadPreset(i);synth.prepareToPlay(48000,128);process(synth,128,{{on(60),0}});require(synth.heldVoiceCount()==1,"Factory preset generated a chord");require(!synth.getProgramName(i).contains("CHORD"),"Chord preset name remains");}
        require(Synth::presetNames().size()==40,"Expected forty presets");
    });
    run("Detune changes the rendered sound",[]{
        Synth a,b;dry(a);dry(b);set(a,"detune",0);set(b,"detune",100);set(a,"size",100);set(b,"size",100);a.prepareToPlay(48000,128);b.prepareToPlay(48000,128);
        require(difference(render(a,8192,128,{{on(60),0}}),render(b,8192,128,{{on(60),0}}))>0.01f,"Detune is ineffective");
    });
    run("Reprepare clears voices, effect tails, and delay memory",[]{
        Synth synth;synth.prepareToPlay(48000,128);process(synth,128,{{on(60),0}});settle(synth,4800);synth.prepareToPlay(96000,1024);
        const auto silence=render(synth,4096,1024,{});for(float v:silence)require(std::abs(v)<0.000001f,"Old audio leaked after sample rate change");require(synth.getLatencySamples()>480,"Oversampling or lookahead latency was not reported");
    });
    run("Extreme macro settings and voice stealing respect the ceiling",[]{
        for(double rate:{44100.0,48000.0,96000.0}){
            Synth synth;for(const char* id:{"size","bright","body","air","space"})set(synth,id,100);set(synth,"output",6);synth.prepareToPlay(rate,128);
            std::vector<std::pair<juce::MidiMessage,int>> events;for(int note=48;note<72;++note)events.emplace_back(on(note),(note-48)*4);
            process(synth,128,events);settle(synth,(int)(rate*0.25));require(synth.activeVoiceCount()<=16,"Voice allocator exceeded its budget");
            process(synth,128,{{juce::MidiMessage::allNotesOff(1),0}});settle(synth,(int)(rate*1.5));require(synth.activeVoiceCount()==0,"Stolen voices did not release");
        }
    });
    run("Dry default has true stereo information and retains its mono centre",[]{
        const auto metrics=measure(sound());
        require(metrics.power>0.001,"Default sound is too quiet");
        require(metrics.correlation>0.0&&metrics.correlation<0.90,"Default is anti-correlated or too narrow");
        require(metrics.side/metrics.mid>0.06,"Too little independent stereo information");
        require(metrics.mid/metrics.power>0.60,"Mono centre lost too much energy");
    });
    run("AIR changes upper spectrum without acting as an overall volume control",[]{
        const auto dark=sound(72,50,0),airy=sound(72,50,100);
        require(bandFraction(airy,4000,16000)>bandFraction(dark,4000,16000)*2.0,"AIR has no meaningful upper-band effect");
        const double ratio=measure(airy).power/measure(dark).power;
        require(ratio>0.5&&ratio<2.0,"AIR mostly changes overall level");
    });
    run("BODY adds low-mid harmonics while retaining stereo width",[]{
        const auto lean=sound(72,0,58),full=sound(72,100,58);
        const auto leanMetrics=measure(lean),fullMetrics=measure(full);
        const double leanBand=leanMetrics.power*bandFraction(lean,200,1500);
        const double fullBand=fullMetrics.power*bandFraction(full,200,1500);
        require(fullBand>leanBand*1.30,"BODY does not add sufficient harmonic body");
        require(fullMetrics.correlation<0.92,"BODY collapses the stereo image");
    });
    run("SIZE grows ensemble energy and stereo information",[]{
        const auto small=measure(sound(0)),large=measure(sound(100));
        require(large.power>small.power*1.03,"SIZE reduces the chord's average energy");
        require(large.side/large.mid>small.side/small.mid*4.0,"SIZE does not grow the ensemble width");
    });
    run("Factory families restore distinct envelope, filter, and detune settings",[]{
        Synth synth;synth.loadPreset(6);const float dropAttack=get(synth,"attack");
        synth.loadPreset(11);require(get(synth,"attack")>dropAttack+100,"Soft and drop envelopes are indistinguishable");
        synth.loadPreset(18);const float darkCutoff=get(synth,"cutoff");synth.loadPreset(16);
        require(get(synth,"cutoff")>darkCutoff+5000,"Dark and airy filters are indistinguishable");
        for(int i=0;i<40;++i){synth.loadPreset(i);synth.prepareToPlay(48000,128);process(synth,128,{{on(48),0}});settle(synth,4800);process(synth,128,{{juce::MidiMessage::allSoundOff(1),0}});require(synth.activeVoiceCount()==0,"Preset failed to release its voices");}
    });
    run("Upper layer remains finite through C5-C8 and the highest MIDI notes",[]{
        for(double rate:{44100.0,96000.0}){
            Synth synth;for(const char* id:{"size","bright","body","air","detune"})set(synth,id,100);set(synth,"cutoff",18000);set(synth,"release",20);synth.prepareToPlay(rate,32);
            for(int note:{72,84,96,108,120,127}){process(synth,32,{{on(note),0}});settle(synth,4096,32);process(synth,32,{{juce::MidiMessage::noteOff(1,note),0}});settle(synth,4096,32);require(synth.activeVoiceCount()==0,"High-register note became stuck");}
        }
    });
    run("Legacy chord state is ignored while active settings survive",[]{
        Synth synth;auto state=synth.parameters.copyState();juce::ValueTree chord("PARAM");chord.setProperty("id","chord",nullptr);chord.setProperty("value",1,nullptr);state.appendChild(chord,nullptr);
        state.getChildWithProperty("id","body").setProperty("value",83,nullptr);juce::MemoryBlock data;auto xml=state.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,data);
        synth.setStateInformation(data.getData(),(int)data.getSize());require(std::abs(get(synth,"body")-83)<0.01f,"Legacy active parameter lost");synth.prepareToPlay(48000,128);process(synth,128,{{on(60),0}});require(synth.heldVoiceCount()==1,"Legacy state re-enabled chord generation");
    });
    run("User preset export, import, and corrupt file rejection",[]{
        const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("heaven-preset-test",".heavensaw");
        Synth source;source.loadPreset(16);set(source,"size",33.5f);require(source.saveUserPreset(file),"Save failed");Synth restored;require(restored.loadUserPreset(file),"Load failed");require(restored.getCurrentProgram()==16&&std::abs(get(restored,"size")-33.5f)<0.01f,"Saved settings differ");
        file.replaceWithText("broken");require(!restored.loadUserPreset(file),"Corrupt preset accepted");require(std::abs(get(restored,"size")-33.5f)<0.01f,"Corrupt preset changed state");file.deleteFile();
    });
    run("Visual samples and output peaks represent actual stereo audio",[]{
        Synth synth;synth.prepareToPlay(48000,128);synth.beginVisualisation();process(synth,128,{{on(60),0}});settle(synth,4096);
        std::array<float,8192> left{},right{};const int count=synth.readVisualSamples(left.data(),right.data(),8192);require(count==4224,"Visual queue lost samples");float peak=0;for(int i=0;i<count;++i)peak=std::max(peak,std::max(std::abs(left[(size_t)i]),std::abs(right[(size_t)i])));
        const auto peaks=synth.takeOutputPeaks();require(peak>0.005f&&std::max(peaks[0],peaks[1])>=peak-0.000001f,"Meter differs from audio");require(synth.readVisualSamples(left.data(),right.data(),8192)==0,"Queue failed to drain");
        settle(synth,100000);int total=0,n=0;while((n=synth.readVisualSamples(left.data(),right.data(),8192))>0)total+=n;require(total<32768&&total>32000,"Queue is not bounded");synth.endVisualisation();process(synth,128);require(synth.readVisualSamples(left.data(),right.data(),8192)==0,"Closed editor still collects audio");
        synth.beginVisualisation();settle(synth,5000);synth.endVisualisation();synth.beginVisualisation();require(synth.readVisualSamples(left.data(),right.data(),8192)==0,"Reopened editor retained stale samples");synth.endVisualisation();
    });
    std::cout<<passes<<" checks passed, "<<failures<<" failed\n";
    return failures==0?0:1;
}
