#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthEngine.h"
#include <cstdlib>

const std::array<juce::String,40>& HeavenSawSynthAudioProcessor::presetNames() {
    static const std::array<juce::String,40> names = {
        "INIT - Heaven Saw","HEAVEN - Classic Heaven","HEAVEN - Endless Sky","HEAVEN - Golden Hour","HEAVEN - Above Clouds","HEAVEN - Euphoric 2014",
        "DROP - Festival Wall","DROP - Massive Drop","DROP - Bright Giant","DROP - Mainstage","DROP - Wide AF",
        "SOFT - Dream Saw","SOFT - Cloud Pad","SOFT - Emotional","SOFT - Soft Lights",
        "AIRY - Angel Glass","AIRY - High Heaven","AIRY - Shimmer Saw",
        "DARK - Midnight","DARK - Warm Giant","DARK - Dark Heaven",
        "HEAVEN - Lift","HEAVEN - Blue Horizon","WIDE - Open Horizon","AIRY - Ninth Sky",
        "WIDE - Horizon Bloom","WIDE - Skyline","WIDE - Endless Width","HEAVEN - Celestial","HEAVEN - Afterglow",
        "DROP - Anthem Rise","DROP - Supernova","SOFT - Blue Hour","SOFT - Slow Motion","AIRY - Halo",
        "AIRY - Starlight","DARK - Velvet Night","WIDE - Open Heaven","HEAVEN - Golden Light","INIT - Clean Start"
    };
    return names;
}

HeavenSawSynthAudioProcessor::APVTS::ParameterLayout HeavenSawSynthAudioProcessor::createParameterLayout() {
    APVTS::ParameterLayout l;
    auto macro=[&](const char* id,const char* name,float def){l.add(std::make_unique<juce::AudioParameterFloat>(id,name,juce::NormalisableRange<float>(0,100,0.01f),def));};
    macro("size","SIZE",72); macro("bright","BRIGHT",64); macro("body","BODY",50); macro("air","AIR",58); macro("space","SPACE",28); macro("pump","PUMP",0);
    l.add(std::make_unique<juce::AudioParameterFloat>("attack","Attack",juce::NormalisableRange<float>(0,2000,1,0.45f),15));
    l.add(std::make_unique<juce::AudioParameterFloat>("decay","Decay",juce::NormalisableRange<float>(20,8000,1,0.45f),850));
    l.add(std::make_unique<juce::AudioParameterFloat>("sustain","Sustain",juce::NormalisableRange<float>(0,100,0.1f),88));
    l.add(std::make_unique<juce::AudioParameterFloat>("release","Release",juce::NormalisableRange<float>(20,12000,1,0.45f),1400));
    l.add(std::make_unique<juce::AudioParameterFloat>("cutoff","Filter Cutoff",juce::NormalisableRange<float>(1000,18000,1,0.4f),12000));
    l.add(std::make_unique<juce::AudioParameterFloat>("detune","Detune",juce::NormalisableRange<float>(0,100,0.1f),55));
    l.add(std::make_unique<juce::AudioParameterChoice>("pumpRate","Pump Rate",juce::StringArray{"1/2","1/4","1/8","OFF"},1));
    l.add(std::make_unique<juce::AudioParameterFloat>("output","Output",juce::NormalisableRange<float>(-24,6,0.1f),0));
    return l;
}
HeavenSawSynthAudioProcessor::HeavenSawSynthAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      parameters(*this,nullptr,"STATE",createParameterLayout()),
      engine(std::make_unique<HeavenVoiceEngine>(parameters)) {
    const char* ids[]{"size","bright","body","air","space","pump","attack","decay","sustain","release","cutoff","detune","pumpRate","output"};
    for(size_t i=0;i<parameterValues.size();++i)parameterValues[i]=parameters.getRawParameterValue(ids[i]);
}
HeavenSawSynthAudioProcessor::~HeavenSawSynthAudioProcessor()=default;
void HeavenSawSynthAudioProcessor::prepareToPlay(double sr,int block) {
    sampleRate=std::isfinite(sr)&&sr>0.0?sr:44100.0;
    visualSampleRate.store((float)sampleRate,std::memory_order_relaxed);
    chunkCapacity=std::max(1,block);
    synthBuffer.setSize(2,chunkCapacity);wetBuffer.setSize(2,chunkCapacity);
    synthBuffer.clear();wetBuffer.clear();engine->prepare(sampleRate);
    reverbLow.fill(0);duckEnvelope=0;densityBands={};densitySamples=0;
    const juce::dsp::ProcessSpec spec{sampleRate,(juce::uint32)chunkCapacity,2};
    reverb.prepare(spec);reverb.reset();
    chorus.prepare(spec);chorus.reset();chorus.setCentreDelay(9.0f);chorus.setDepth(0.09f);
    chorus.setRate(0.11f);chorus.setFeedback(0.05f);chorus.setMix(0.08f);
    crossoverLow.prepare(spec);crossoverLow.setCutoffFrequency(180.0f);
    crossoverHigh.prepare(spec);crossoverHigh.setCutoffFrequency(2500.0f);
    lowAlignment.prepare(spec);lowAlignment.setCutoffFrequency(2500.0f);
    lowAlignment.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
    saturationOversampling.initProcessing((size_t)chunkCapacity);saturationOversampling.reset();
    lookaheadSamples=std::max(1,(int)std::round(sampleRate*0.005));
    limiterDelay.setSize(2,lookaheadSamples);limiterDelay.clear();limiterIndex=0;limiterGain=1;
    limiterPeaks.assign((size_t)lookaheadSamples+2,0.0f);
    limiterPeakPositions.assign((size_t)lookaheadSamples+2,0);
    limiterHead=0;limiterTail=0;limiterSamples=0;
    setLatencySamples((int)std::round(saturationOversampling.getLatencyInSamples())+lookaheadSamples);
    const std::array<float,7> initial{parameterValues[0]->load()*0.01f,parameterValues[2]->load()*0.01f,
        parameterValues[4]->load()*0.01f,0.0f,juce::Decibels::decibelsToGain(parameterValues[13]->load()),parameterValues[1]->load()*0.01f,parameterValues[3]->load()*0.01f};
    for(size_t i=0;i<effects.size();++i){effects[i].reset(sampleRate,0.03);effects[i].setCurrentAndTargetValue(initial[i]);}
}
void HeavenSawSynthAudioProcessor::releaseResources(){
    engine->prepare(sampleRate);reverb.reset();chorus.reset();saturationOversampling.reset();
    crossoverLow.reset();crossoverHigh.reset();lowAlignment.reset();densityBands={};densitySamples=0;
}
bool HeavenSawSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& l)const{return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
void HeavenSawSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    double bpm=120.0,ppq=0.0;bool validClock=false;
    if(auto* ph=getPlayHead())if(auto pos=ph->getPosition()){
        const auto tempo=pos->getBpm(),beat=pos->getPpqPosition();
        if(pos->getIsPlaying()&&tempo&&beat&&std::isfinite(*tempo)&&*tempo>0.0&&std::isfinite(*beat)){bpm=*tempo;ppq=*beat;validClock=true;}
    }
    auto event=midi.cbegin();const auto end=midi.cend();
    for(int offset=0;offset<buffer.getNumSamples();) {
        const int count=std::min(chunkCapacity,buffer.getNumSamples()-offset);
        synthBuffer.clear(0,count);int rendered=0;
        while(event!=end && (*event).samplePosition<offset+count){
            const auto message=*event;
            const int local=juce::jlimit(rendered,count,message.samplePosition-offset);
            engine->render(synthBuffer,rendered,local-rendered);
            engine->handleMidi(message.data,message.numBytes);rendered=local;++event;
        }
        engine->render(synthBuffer,rendered,count-rendered);
        processEffects(count,bpm,ppq,offset,validClock);
        for(int c=0;c<std::min(2,buffer.getNumChannels());++c)buffer.copyFrom(c,offset,synthBuffer,c,0,count);
        offset+=count;
    }
    // Zero-length host callbacks and events exactly at the block end still update MIDI state.
    while(event!=end){const auto message=*event;engine->handleMidi(message.data,message.numBytes);++event;}
    publishVisuals(buffer);
    midi.clear();
}
void HeavenSawSynthAudioProcessor::processEffects(int count,double bpm,double ppq,int absoluteOffset,bool clockValid) {
    const float size=parameterValues[0]->load(std::memory_order_relaxed)*0.01f;
    const float body=parameterValues[2]->load(std::memory_order_relaxed)*0.01f;
    const float air=parameterValues[3]->load(std::memory_order_relaxed)*0.01f;
    const float space=parameterValues[4]->load(std::memory_order_relaxed)*0.01f;
    const int pumpRate=juce::jlimit(0,3,(int)parameterValues[12]->load(std::memory_order_relaxed));
    effects[0].setTargetValue(size);effects[1].setTargetValue(body);effects[2].setTargetValue(space);
    effects[3].setTargetValue(clockValid&&pumpRate!=3?parameterValues[5]->load(std::memory_order_relaxed)*0.01f:0.0f);
    effects[4].setTargetValue(juce::Decibels::decibelsToGain(parameterValues[13]->load(std::memory_order_relaxed)));
    auto block=juce::dsp::AudioBlock<float>(synthBuffer).getSubBlock(0,(size_t)count);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    effects[5].setTargetValue(parameterValues[1]->load(std::memory_order_relaxed)*0.01f);
    effects[6].setTargetValue(air);
    chorus.process(ctx);
    auto up=saturationOversampling.processSamplesUp(block);
    for(int c=0;c<2;++c){auto* data=up.getChannelPointer((size_t)c);for(size_t i=0;i<up.getNumSamples();++i)data[i]=std::tanh(data[i]*1.9f)/1.1f;}
    saturationOversampling.processSamplesDown(block);
    const float reverbCut=1.0f-std::exp(-juce::MathConstants<float>::twoPi*280.0f/(float)sampleRate);
    const float detectorAttack=1.0f-std::exp(-1.0f/(0.012f*(float)sampleRate));
    const float detectorRelease=1.0f-std::exp(-1.0f/(0.16f*(float)sampleRate));
    const float gainAttack=1.0f-std::exp(-1.0f/(0.008f*(float)sampleRate));
    const float gainRelease=1.0f-std::exp(-1.0f/(0.10f*(float)sampleRate));
    constexpr std::array<float,3> thresholds{0.22f,0.24f,0.10f};
    for(int i=0;i<count;++i){
        const float s=effects[0].getNextValue(),bo=effects[1].getNextValue();
        const float br=effects[5].getNextValue(),ar=effects[6].getNextValue();
        std::array<std::array<float,2>,3> bands{};
        for(int c=0;c<2;++c){
            float upper=0;
            crossoverLow.processSample(c,synthBuffer.getSample(c,i),bands[0][(size_t)c],upper);
            bands[0][(size_t)c]=lowAlignment.processSample(c,bands[0][(size_t)c]);
            crossoverHigh.processSample(c,upper,bands[1][(size_t)c],bands[2][(size_t)c]);
        }
        // The low band is centred; mid/high retain independent oscillator information.
        bands[0][0]=bands[0][1]=0.5f*(bands[0][0]+bands[0][1]);
        for(size_t k=0;k<densityBands.size();++k){
            auto& d=densityBands[k];
            const float peak=std::max(std::abs(bands[k][0]),std::abs(bands[k][1]));
            d.envelope+=(peak-d.envelope)*(peak>d.envelope?detectorAttack:detectorRelease);
            if((densitySamples&15u)==0){
                const float over=juce::Decibels::gainToDecibels(std::max(0.000001f,d.envelope)/thresholds[k]);
                // Six-dB soft knee, with a gentle 2:1 ratio. Both channels share the gain.
                const float reduction=over<=-3.0f?0.0f:(over>=3.0f?0.5f*over:(over+3.0f)*(over+3.0f)/24.0f);
                d.target=juce::Decibels::decibelsToGain(-std::min(8.0f,reduction));
            }
            d.gain+=(d.target-d.gain)*(d.target<d.gain?gainAttack:gainRelease);
        }
        ++densitySamples;
        const std::array<float,3> makeup{0.98f,0.95f+0.22f*br+0.40f*bo,0.70f+0.30f*br+0.90f*ar*ar};
        const std::array<float,3> widths{0.0f,1.05f+0.26f*s,1.15f+0.25f*s+0.30f*ar};
        float l=0,r=0;
        for(size_t k=0;k<bands.size();++k){
            const float mid=0.5f*(bands[k][0]+bands[k][1]);
            const float side=0.5f*(bands[k][0]-bands[k][1])*widths[k];
            const float gain=densityBands[k].gain*makeup[k];
            l+=(mid+side)*gain;r+=(mid-side)*gain;
        }
        synthBuffer.setSample(0,i,l);synthBuffer.setSample(1,i,r);
        reverbLow[0]+=(l-reverbLow[0])*reverbCut;reverbLow[1]+=(r-reverbLow[1])*reverbCut;
        wetBuffer.setSample(0,i,l-reverbLow[0]);wetBuffer.setSample(1,i,r-reverbLow[1]);
    }
    juce::dsp::Reverb::Parameters rp;
    rp.wetLevel=1.0f;rp.dryLevel=0.0f;rp.roomSize=0.35f+0.60f*effects[2].getCurrentValue();
    rp.damping=0.65f-0.30f*air;rp.width=0.9f;rp.freezeMode=0;reverb.setParameters(rp);
    auto wet=juce::dsp::AudioBlock<float>(wetBuffer).getSubBlock(0,(size_t)count);
    juce::dsp::ProcessContextReplacing<float> wc(wet);reverb.process(wc);
    const float duckRelease=std::exp(-1.0f/(0.34f*(float)sampleRate));
    const float limiterRecovery=1.0f-std::exp(-1.0f/(0.08f*(float)sampleRate));
    constexpr float ceiling=0.944060876f; // -0.5 dBFS
    const double division=pumpRate==0?2.0:(pumpRate==2?0.5:1.0);
    const int queueCapacity=(int)limiterPeaks.size();
    for(int i=0;i<count;++i){
        const float dryL=synthBuffer.getSample(0,i),dryR=synthBuffer.getSample(1,i);
        duckEnvelope=std::max(std::max(std::abs(dryL),std::abs(dryR)),duckEnvelope*duckRelease);
        const float s=effects[2].getNextValue(),depth=effects[3].getNextValue(),output=effects[4].getNextValue();
        const float duck=1.0f-0.48f*s*std::min(1.0f,duckEnvelope*2.0f);
        float pumpGain=1.0f;
        if(clockValid&&pumpRate!=3){
            const double beat=(ppq+(double)(absoluteOffset+i)/sampleRate*bpm/60.0)/division;
            const double phase=beat-std::floor(beat);
            const double attack=std::min(0.1,bpm/60.0*0.003/division);
            const double dip=phase<attack?phase/attack:std::exp(-9.0*(phase-attack)/(1.0-attack));
            pumpGain=1.0f-0.72f*depth*(float)dip;
        }
        const float l=(dryL+wetBuffer.getSample(0,i)*0.55f*s*s*duck)*pumpGain*output;
        const float r=(dryR+wetBuffer.getSample(1,i)*0.55f*s*s*duck)*pumpGain*output;
        const float peak=std::max(std::abs(l),std::abs(r));
        while(limiterHead!=limiterTail && limiterPeakPositions[(size_t)limiterHead]+(uint64_t)lookaheadSamples<limiterSamples)limiterHead=(limiterHead+1)%queueCapacity;
        while(limiterHead!=limiterTail){const int previous=(limiterTail+queueCapacity-1)%queueCapacity;if(limiterPeaks[(size_t)previous]>peak)break;limiterTail=previous;}
        limiterPeaks[(size_t)limiterTail]=peak;limiterPeakPositions[(size_t)limiterTail]=limiterSamples;limiterTail=(limiterTail+1)%queueCapacity;
        const float target=ceiling/std::max(ceiling,limiterPeaks[(size_t)limiterHead]);
        limiterGain=target<limiterGain?target:limiterGain+(target-limiterGain)*limiterRecovery;
        const float delayedL=limiterDelay.getSample(0,limiterIndex),delayedR=limiterDelay.getSample(1,limiterIndex);
        limiterDelay.setSample(0,limiterIndex,l);limiterDelay.setSample(1,limiterIndex,r);
        limiterIndex=(limiterIndex+1)%lookaheadSamples;++limiterSamples;
        synthBuffer.setSample(0,i,juce::jlimit(-ceiling,ceiling,delayedL*limiterGain));
        synthBuffer.setSample(1,i,juce::jlimit(-ceiling,ceiling,delayedR*limiterGain));
    }
}
int HeavenSawSynthAudioProcessor::heldVoiceCount()const noexcept{return engine->heldVoiceCount();}
int HeavenSawSynthAudioProcessor::activeVoiceCount()const noexcept{return engine->activeVoiceCount();}
void HeavenSawSynthAudioProcessor::setCurrentProgram(int i){loadPreset(i);}
const juce::String HeavenSawSynthAudioProcessor::getProgramName(int i){return presetNames()[(size_t)juce::jlimit(0,39,i)];}
void HeavenSawSynthAudioProcessor::loadPreset(int i) {
    const int program=juce::jlimit(0,39,i);currentProgram.store(program,std::memory_order_relaxed); static constexpr float presets[40][6]={
      {72,64,50,58,28,0},{78,60,52,62,40,0},{82,58,46,72,52,0},{68,57,62,42,35,0},{84,58,46,82,48,0},{78,76,48,52,32,0},
      {96,84,46,86,24,60},{94,70,63,58,30,70},{85,86,46,78,22,55},{90,78,57,66,28,62},{98,70,50,82,35,64},
      {58,45,42,55,62,0},{48,42,55,61,68,0},{62,56,46,58,60,0},{54,62,39,68,55,0},{68,78,35,92,45,0},{75,84,38,96,42,0},{64,74,34,100,50,0},
      {62,30,68,20,25,0},{54,40,74,28,32,0},{66,35,61,32,35,0},{74,66,52,60,35,0},{75,60,52,55,35,0},{68,57,44,52,32,0},{78,72,43,74,45,0},
      {90,64,46,78,38,0},{84,60,44,82,40,0},{96,68,46,85,45,0},{76,72,44,82,50,0},{70,60,50,72,58,0},
      {90,84,52,68,28,48},{98,90,48,78,24,68},{48,42,42,65,65,0},{42,48,46,72,75,0},{60,76,35,98,52,0},
      {72,83,37,100,44,0},{56,28,78,25,32,0},{76,62,50,62,38,0},{80,74,48,76,45,0},{28,50,34,20,8,0}};
    const char* ids[]={"size","bright","body","air","space","pump"};
    for(int j=0;j<6;++j) if(auto* p=dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(ids[j]))) p->setValueNotifyingHost(p->convertTo0to1(presets[program][j]));
    // Attack, decay, sustain, release, cutoff, detune, pump division, output.
    // Distinct envelope/filter/detune settings give each family a useful musical role.
    static constexpr float advanced[40][8]={
      {15,850,88,1400,12000,55,1,0},{12,900,94,1800,13500,63,1,-0.5f},{35,1400,92,2800,12500,68,1,-1},
      {28,1100,90,2100,10500,46,1,-0.5f},{45,1600,94,3000,14000,70,1,-1},{9,650,93,850,14500,72,1,-1},
      {6,500,97,450,17000,64,1,-1.5f},{8,700,96,600,13800,74,1,-1.5f},{3,350,96,400,17000,58,2,-1},
      {7,550,98,550,15500,68,1,-1},{5,750,95,700,14600,82,1,-2},
      {180,1800,88,4000,9200,48,1,-2},{300,2300,91,5000,8500,40,1,-2},{120,1400,86,3200,10800,52,1,-1.5f},
      {95,1600,84,3000,11800,45,1,-1.5f},{22,1000,88,1900,15200,52,1,-1.5f},{18,850,91,1500,17000,58,1,-2},
      {65,1600,86,2800,14500,49,1,-2},{22,1000,92,1400,5200,43,1,-0.5f},{35,1200,94,1900,6800,34,1,-1},
      {55,1700,90,2600,6000,46,1,-1},{12,850,94,1700,13000,59,1,-0.5f},{18,1050,92,2000,11800,62,1,-0.5f},
      {14,900,94,1500,11500,52,1,0},{22,1150,90,2300,14600,64,1,-1},{25,1200,92,1900,13200,76,1,-1.5f},
      {18,1050,94,1600,12500,69,1,-1},{40,1500,92,2600,14500,85,1,-2},{32,1200,91,2400,15800,65,1,-1},
      {75,1900,90,3600,12700,57,1,-1.5f},{7,600,96,650,15800,68,1,-1},{4,450,98,450,17800,79,1,-2},
      {250,2200,87,4800,8900,35,1,-1.5f},{500,2800,90,6500,9800,32,1,-2},{95,1600,87,3400,16200,43,1,-2},
      {45,1250,90,2300,17500,55,1,-2},{70,1800,93,3000,4400,30,1,-1},{22,1000,94,2100,12500,62,1,-0.5f},
      {15,900,93,2000,15000,65,1,-1},{8,300,100,300,10000,24,3,-1}};
    const char* advancedIds[]{"attack","decay","sustain","release","cutoff","detune","pumpRate","output"};
    for(size_t j=0;j<8;++j)if(auto* parameter=parameters.getParameter(advancedIds[j]))parameter->setValueNotifyingHost(parameter->convertTo0to1(advanced[program][j]));
}
void HeavenSawSynthAudioProcessor::getStateInformation(juce::MemoryBlock& dest){
    auto state=parameters.copyState();state.setProperty("schema_version",3,nullptr);
    state.setProperty("plugin_version",JucePlugin_VersionString,nullptr);state.setProperty("program",getCurrentProgram(),nullptr);
    const auto xml=state.createXml();copyXmlToBinary(*xml,dest);
}
void HeavenSawSynthAudioProcessor::setStateInformation(const void* data,int size){
    if(size<=0 || data==nullptr)return;
    const auto xml=getXmlFromBinary(data,size);if(!xml || !xml->hasTagName(parameters.state.getType()))return;
    const auto incoming=juce::ValueTree::fromXml(*xml);if(!incoming.isValid())return;
    auto clean=parameters.copyState();
    auto parse=[](const juce::var& v,double fallback){const auto text=v.toString();const auto* start=text.toRawUTF8();char* end=nullptr;const double number=std::strtod(start,&end);return end!=start&&*end=='\0'&&std::isfinite(number)?number:fallback;};
    for(auto* parameter:getParameters())if(auto* p=dynamic_cast<juce::RangedAudioParameter*>(parameter)){
        auto child=clean.getChildWithProperty("id",p->paramID);
        const auto source=incoming.getChildWithProperty("id",p->paramID);
        const double defaultValue=p->convertFrom0to1(p->getDefaultValue());
        const double raw=source.isValid()?parse(source.getProperty("value"),defaultValue):defaultValue;
        const auto range=p->getNormalisableRange();
        const float restored=range.snapToLegalValue((float)juce::jlimit((double)range.start,(double)range.end,raw));
        if(child.isValid())child.setProperty("value",restored,nullptr);
    }
    const double rawProgram=parse(incoming.getProperty("program"),0.0);
    const int program=(int)juce::jlimit(0.0,39.0,rawProgram);
    clean.setProperty("schema_version",3,nullptr);clean.setProperty("program",program,nullptr);
    parameters.replaceState(clean);currentProgram.store(program,std::memory_order_relaxed);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new HeavenSawSynthAudioProcessor();}
juce::AudioProcessorEditor* HeavenSawSynthAudioProcessor::createEditor(){return new HeavenSawSynthAudioProcessorEditor(*this);}

bool HeavenSawSynthAudioProcessor::saveUserPreset(const juce::File& file) {
    juce::MemoryBlock state;getStateInformation(state);
    return file.replaceWithData(state.getData(),state.getSize());
}
bool HeavenSawSynthAudioProcessor::loadUserPreset(const juce::File& file) {
    if(!file.existsAsFile() || file.getSize()>4*1024*1024)return false;
    juce::MemoryBlock state;if(!file.loadFileAsData(state))return false;
    const auto xml=getXmlFromBinary(state.getData(),(int)state.getSize());
    if(!xml || !xml->hasTagName(parameters.state.getType()))return false;
    setStateInformation(state.getData(),(int)state.getSize());return true;
}
void HeavenSawSynthAudioProcessor::beginVisualisation(){
    // The editor is the sole FIFO consumer. Drain stale samples without resetting
    // indices that an in-flight audio callback might still be publishing.
    std::array<float,4096> left{},right{};
    for(int pass=0;pass<8;++pass)if(readVisualSamples(left.data(),right.data(),4096)==0)break;
    takeOutputPeaks();visualReaders.fetch_add(1,std::memory_order_relaxed);
}
void HeavenSawSynthAudioProcessor::endVisualisation(){visualReaders.fetch_sub(1,std::memory_order_relaxed);}
void HeavenSawSynthAudioProcessor::publishVisuals(const juce::AudioBuffer<float>& buffer) {
    if(visualReaders.load(std::memory_order_relaxed)==0 || buffer.getNumChannels()<2)return;
    const auto* l=buffer.getReadPointer(0);const auto* r=buffer.getReadPointer(1);
    float peakL=0,peakR=0;for(int i=0;i<buffer.getNumSamples();++i){peakL=std::max(peakL,std::abs(l[i]));peakR=std::max(peakR,std::abs(r[i]));}
    auto retain=[](std::atomic<float>& target,float value){auto old=target.load(std::memory_order_relaxed);while(old<value&&!target.compare_exchange_weak(old,value,std::memory_order_relaxed)){};};
    retain(visualPeakL,peakL);retain(visualPeakR,peakR);
    int start1,size1,start2,size2;visualFifo.prepareToWrite(buffer.getNumSamples(),start1,size1,start2,size2);
    if(size1){std::copy_n(l,size1,visualLeft.data()+start1);std::copy_n(r,size1,visualRight.data()+start1);}
    if(size2){std::copy_n(l+size1,size2,visualLeft.data()+start2);std::copy_n(r+size1,size2,visualRight.data()+start2);}
    visualFifo.finishedWrite(size1+size2);
}
int HeavenSawSynthAudioProcessor::readVisualSamples(float* left,float* right,int maximum) {
    int start1,size1,start2,size2;visualFifo.prepareToRead(std::max(0,maximum),start1,size1,start2,size2);
    if(size1){std::copy_n(visualLeft.data()+start1,size1,left);std::copy_n(visualRight.data()+start1,size1,right);}
    if(size2){std::copy_n(visualLeft.data()+start2,size2,left+size1);std::copy_n(visualRight.data()+start2,size2,right+size1);}
    visualFifo.finishedRead(size1+size2);return size1+size2;
}
std::array<float,2> HeavenSawSynthAudioProcessor::takeOutputPeaks(){return {visualPeakL.exchange(0,std::memory_order_relaxed),visualPeakR.exchange(0,std::memory_order_relaxed)};}
