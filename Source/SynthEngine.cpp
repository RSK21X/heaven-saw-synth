#include "SynthEngine.h"

namespace {
constexpr float pi2 = juce::MathConstants<float>::twoPi;
constexpr std::array<float,24> offsets{0,-2.1f,2.8f,-4.6f,5.5f,-7.8f,9.1f,-11.7f,13.2f,-16.4f,18,-20.8f,23.5f,-27,30,34,-1.1f,3.8f,-6.1f,8.2f,-10,14.8f,-18.5f,25.1f};
float blep(float phase, float step) noexcept {
    if (phase < step) { const float t=phase/step; return t+t-t*t-1.0f; }
    if (phase > 1.0f-step) { const float t=(phase-1.0f)/step; return t*t+t+t+1.0f; }
    return 0.0f;
}
float density(float size) noexcept {
    return size <= 0.75f ? 4.0f+16.0f*size : 16.0f+32.0f*(size-0.75f);
}
}

struct HeavenVoiceEngine::Settings {
    float size, bright, body, air, attack, decay, sustain, release, cutoff, detune;
};

struct HeavenVoiceEngine::Voice {
    struct State {
        bool active=false, releasing=false, cacheDirty=true;
        int channel=0, root=0, pitch=60, wheel=8192;
        uint64_t serial=0, age=0;
        float velocity=0, envelope=0, releaseStart=0, releaseElapsed=0, releaseSamples=1;
        float attackSamples=1, decaySamples=1, subPhase=0, subStep=0, norm=1;
        float filterCoefficient=0.5f, coreCoefficient=0.3f, airHighPass=0.2f, airLowPass=0.8f;
        float upperNyquistGain=1, thirdGain=1, fifthGain=1;
        std::array<float,24> phase{}, step{}, panL{}, panR{};
        std::array<float,3> corePhase{}, coreStep{};
        std::array<float,8> upperPhase{}, upperStep{};
        std::array<float,2> filter1{}, filter2{}, coreFilter{}, upperLow{}, upperFilter{};
        juce::SmoothedValue<float> size, bright, body, air, count, cutoff, detune, sustain;

        static float sine(float p) noexcept {
            p-=std::floor(p);
            const float x=pi2*(p>0.5f ? p-1.0f : p);
            return juce::dsp::FastMathApproximations::sin(x);
        }
        static float saw(float p,float increment) noexcept { return 2.0f*p-1.0f-blep(p,increment); }
        static float square(float p,float increment) noexcept {
            float shifted=p+0.5f;
            if(shifted>=1.0f)shifted-=1.0f;
            return (p<0.5f ? -1.0f : 1.0f)-blep(p,increment)+blep(shifted,increment);
        }
        static void advance(float& p,float increment) noexcept { p+=increment;if(p>=1.0f)p-=1.0f; }

        void start(int channelIn, int rootIn, int pitchIn, int wheelIn, float vel,
                   uint64_t number, double sr, const Settings& p) {
            *this=State{};
            active=true; channel=channelIn; root=rootIn; pitch=pitchIn; wheel=wheelIn;
            velocity=std::sqrt(vel); serial=number;
            uint32_t random=0x9e3779b9u ^ (uint32_t)(number*109u) ^ (uint32_t)(pitchIn*37);
            for(int i=0;i<24;++i) {
                random=1664525u*random+1013904223u;
                phase[(size_t)i]=(float)(random>>8)/16777216.0f;
            }
            for(auto& ph:upperPhase){random=1664525u*random+1013904223u;ph=(float)(random>>8)/16777216.0f;}
            // Core oscillators start together; the separate side and upper layers remain diffuse.
            corePhase.fill(0.0f);
            for(auto* smooth : {&size,&bright,&body,&air,&count,&cutoff,&detune,&sustain}) smooth->reset(sr,0.025);
            size.setCurrentAndTargetValue(p.size);bright.setCurrentAndTargetValue(p.bright);
            body.setCurrentAndTargetValue(p.body);air.setCurrentAndTargetValue(p.air);
            count.setCurrentAndTargetValue(density(p.size));cutoff.setCurrentAndTargetValue(p.cutoff);
            detune.setCurrentAndTargetValue(p.detune);sustain.setCurrentAndTargetValue(p.sustain);
            attackSamples=p.attack*0.001f*(float)sr;
            decaySamples=std::max(1.0f,p.decay*0.001f*(float)sr);
        }

        void targets(const Settings& p) {
            size.setTargetValue(p.size);bright.setTargetValue(p.bright);body.setTargetValue(p.body);
            air.setTargetValue(p.air);count.setTargetValue(density(p.size));cutoff.setTargetValue(p.cutoff);
            detune.setTargetValue(p.detune);sustain.setTargetValue(p.sustain);
        }

        void release(float milliseconds, double sr) {
            if (!active || releasing) return;
            releasing=true;releaseStart=envelope;releaseElapsed=0;
            releaseSamples=std::max(1.0f,milliseconds*0.001f*(float)sr);
        }

        std::array<float,2> sample(double sr) {
            if (!active) return {};
            const float s=size.getNextValue(),b=bright.getNextValue(),bo=body.getNextValue(),a=air.getNextValue();
            const float n=count.getNextValue(),cut=cutoff.getNextValue(),det=detune.getNextValue(),sus=sustain.getNextValue();
            const float dt=1.0f/(float)sr;
            if (cacheDirty || (age&31u)==0) {
                cacheDirty=false;
                const float bend=((float)wheel-8192.0f)/8192.0f*2.0f;
                const float base=440.0f*std::exp2(((float)pitch-69.0f+bend)/12.0f);
                const float spread=(0.25f+0.95f*s)*det/55.0f;
                for(int i=0;i<24;++i) {
                    const float drift=0.28f*std::sin((float)(age%1000000000u)*dt*(0.06f+0.013f*(float)i)+(float)i*3.2f);
                    const float cents=offsets[(size_t)i]*spread+drift;
                    step[(size_t)i]=std::min(0.49f,base*std::exp2(cents/1200.0f)*dt);
                    const float pan=std::copysign(std::pow(std::abs(offsets[(size_t)i])/34.0f,0.25f)*(0.50f+0.48f*s),offsets[(size_t)i]);
                    panL[(size_t)i]=std::sqrt(0.5f*(1.0f-pan));panR[(size_t)i]=std::sqrt(0.5f*(1.0f+pan));
                }
                constexpr std::array<float,3> coreOffsets{-1.7f,0.0f,2.0f};
                for(size_t i=0;i<coreStep.size();++i)coreStep[i]=std::min(0.49f,base*std::exp2(coreOffsets[i]*det/55.0f/1200.0f)*dt);
                constexpr std::array<float,8> upperOffsets{-7.3f,8.7f,-15.1f,17.4f,-23.8f,26.2f,-32.6f,35.1f};
                for(size_t i=0;i<upperStep.size();++i)upperStep[i]=std::min(0.49f,base*2.0f*std::exp2(upperOffsets[i]*spread/1200.0f)*dt);
                // Suppress the octave layer before it reaches Nyquist, rather than folding it down.
                upperNyquistGain=juce::jlimit(0.0f,1.0f,((float)sr*0.44f-base*2.0f)/((float)sr*0.12f));
                thirdGain=juce::jlimit(0.0f,1.0f,((float)sr*0.45f-base*3.0f)/((float)sr*0.08f));
                fifthGain=juce::jlimit(0.0f,1.0f,((float)sr*0.45f-base*5.0f)/((float)sr*0.08f));
                subStep=std::min(0.49f,base*0.5f*dt);
                // Constant expected ensemble power, with a modest deliberate density lift.
                norm=(0.85f+0.45f*s)/std::sqrt(n);
                const float opening=0.86f+0.14f*(1.0f-std::exp(-(float)age*dt/0.025f));
                const float hz=std::min(cut*(0.62f+0.64f*b)*opening,(float)sr*0.42f);
                filterCoefficient=1.0f-std::exp(-pi2*hz*dt);
                coreCoefficient=1.0f-std::exp(-pi2*std::min(1800.0f+3200.0f*b,hz)*dt);
                airHighPass=1.0f-std::exp(-pi2*650.0f*dt);
                airLowPass=1.0f-std::exp(-pi2*std::min(9000.0f+8000.0f*a,(float)sr*0.42f)*dt);
            }
            if (releasing) {
                ++releaseElapsed;
                envelope=releaseStart*std::exp(-9.9034876f*releaseElapsed/releaseSamples);
                if(releaseElapsed>=releaseSamples || envelope<0.0000001f){active=false;return {};}
            } else if ((float)age<attackSamples) {
                envelope=std::min(1.0f,((float)age+1.0f)/std::max(1.0f,attackSamples));
            } else {
                const float progress=((float)age-attackSamples)/decaySamples;
                envelope=progress>=1.0f ? sus : sus+(1.0f-sus)*std::exp(-5.0f*progress);
            }
            float l=0,r=0;
            const int activeOscillators=juce::jlimit(4,24,(int)std::ceil(n));
            for(int i=0;i<activeOscillators;++i) {
                const auto k=(size_t)i;
                advance(phase[k],step[k]);
                const float wave=saw(phase[k],step[k])*juce::jlimit(0.0f,1.0f,n-(float)i);
                l+=wave*panL[k];r+=wave*panR[k];
            }
            float core=0;
            for(size_t i=0;i<corePhase.size();++i){advance(corePhase[i],coreStep[i]);core+=0.72f*saw(corePhase[i],coreStep[i])+0.28f*square(corePhase[i],coreStep[i]);}
            core*=1.0f/3.0f;
            const float fundamental=-sine(corePhase[1]);
            const float triangle=0.81056947f*(fundamental+sine(corePhase[1]*3.0f)*thirdGain/9.0f-sine(corePhase[1]*5.0f)*fifthGain/25.0f);
            // In-phase fundamental support; harmonics add body without a dominant mono sine.
            const float centre=core*(0.14f+0.78f*bo)+triangle*(0.025f+0.12f*bo);
            advance(subPhase,subStep);
            const float sub=-sine(subPhase)*std::max(0.0f,bo-0.65f)*0.14f;
            float upperL=0,upperR=0;
            for(size_t i=0;i<upperPhase.size();++i){
                advance(upperPhase[i],upperStep[i]);
                const float wave=saw(upperPhase[i],upperStep[i]);
                const float left=(i&1u)==0?0.98f:0.20f,right=(i&1u)==0?0.20f:0.98f;
                upperL+=wave*left;upperR+=wave*right;
            }
            // A separate octave ensemble fills upper mids; AIR adds brightness to this layer.
            const float upperGain=(0.16f+0.24f*s+0.22f*a*a)*0.35355339f*upperNyquistGain;
            const float amp=envelope*velocity*0.26f;
            const std::array<float,2> swarm{l*norm,r*norm},upper{upperL*upperGain,upperR*upperGain};
            for(size_t c=0;c<2;++c){
                filter1[c]+=(swarm[c]-filter1[c])*filterCoefficient;
                filter2[c]+=(filter1[c]-filter2[c])*filterCoefficient;
                coreFilter[c]+=(centre-coreFilter[c])*coreCoefficient;
                upperLow[c]+=(upper[c]-upperLow[c])*airHighPass;
                upperFilter[c]+=(upper[c]-upperLow[c]-upperFilter[c])*airLowPass;
            }
            ++age;
            return {(filter2[0]+coreFilter[0]+upperFilter[0]+sub)*amp,
                    (filter2[1]+coreFilter[1]+upperFilter[1]+sub)*amp};
        }
    } state, stolen;
    bool keyDown=false, sostenuto=false;
    int tailLeft=0, tailLength=1;

    void start(int channel,int root,int pitch,int wheel,float velocity,uint64_t number,double sr,const Settings& p) {
        if(state.active){stolen=state;tailLength=std::max(1,(int)(sr*0.005));tailLeft=tailLength;}
        else {tailLeft=0;stolen.active=false;}
        keyDown=true;sostenuto=false;state.start(channel,root,pitch,wheel,velocity,number,sr,p);
    }

    void render(juce::AudioBuffer<float>& out,int start,int count,double sr,const Settings& p) {
        state.targets(p);if(tailLeft>0)stolen.targets(p);
        for(int i=0;i<count;++i) {
            auto sample=state.sample(sr);
            if(tailLeft>0){const auto old=stolen.sample(sr);const float fade=(float)tailLeft/(float)tailLength;sample[0]+=old[0]*fade;sample[1]+=old[1]*fade;--tailLeft;}
            out.addSample(0,start+i,sample[0]);out.addSample(1,start+i,sample[1]);
        }
    }
};

HeavenVoiceEngine::HeavenVoiceEngine(juce::AudioProcessorValueTreeState& p):parameters(p) {
    const char* ids[]{"size","bright","body","air","attack","decay","sustain","release","cutoff","detune"};
    for(size_t i=0;i<controls.size();++i) controls[i]=parameters.getRawParameterValue(ids[i]);
    for(auto& voice:voices)voice=std::make_unique<Voice>();
    wheels.fill(8192);
}
HeavenVoiceEngine::~HeavenVoiceEngine()=default;
void HeavenVoiceEngine::prepare(double sr){rate=sr;serial=0;wheels.fill(8192);sustain.fill(false);for(auto& v:voices)*v=Voice{};}
HeavenVoiceEngine::Settings HeavenVoiceEngine::settings() const {
    std::array<float,10> p{};for(size_t i=0;i<p.size();++i)p[i]=controls[i]->load(std::memory_order_relaxed);
    return {p[0]*0.01f,p[1]*0.01f,p[2]*0.01f,p[3]*0.01f,p[4],p[5],p[6]*0.01f,p[7],p[8],p[9]};
}
HeavenVoiceEngine::Voice& HeavenVoiceEngine::allocateVoice() {
    for(auto& v:voices)if(!v->state.active && v->tailLeft==0)return *v;
    Voice* oldestReleased=nullptr;
    for(auto& v:voices)if(v->state.releasing && (!oldestReleased || v->state.serial<oldestReleased->state.serial))oldestReleased=v.get();
    if(oldestReleased)return *oldestReleased;
    auto* quietest=voices[0].get();
    for(auto& v:voices){const float difference=v->state.envelope-quietest->state.envelope;if(difference< -0.000001f || (std::abs(difference)<=0.000001f && v->state.serial<quietest->state.serial))quietest=v.get();}
    return *quietest;
}
void HeavenVoiceEngine::noteOn(int channel,int root,float velocity) {
    const auto p=settings();
    // Retriggers release the previous voice before starting the new voice.
    for(auto& v:voices)if(v->state.active && v->state.channel==channel && v->state.root==root){v->keyDown=false;v->sostenuto=false;v->state.release(20.0f,rate);}
    // Every input note owns exactly one voice; chords are played directly via MIDI.
    allocateVoice().start(channel,root,root,wheels[(size_t)channel],velocity,++serial,rate,p);
}
void HeavenVoiceEngine::noteOff(int channel,int root) {
    const float release=settings().release;
    for(auto& v:voices)if(v->state.active && v->state.channel==channel && v->state.root==root && v->keyDown){v->keyDown=false;if(!sustain[(size_t)channel]&&!v->sostenuto)v->state.release(release,rate);}
}
void HeavenVoiceEngine::handleMidi(const uint8_t* bytes,int byteCount) {
    if(bytes==nullptr || byteCount<2 || bytes[0]<0x80 || bytes[0]>=0xf0)return;
    const int type=bytes[0]&0xf0,channel=bytes[0]&0x0f,first=bytes[1]&0x7f,second=byteCount>=3?bytes[2]&0x7f:0;
    if(type==0x90 && byteCount>=3 && second>0){noteOn(channel,first,(float)second/127.0f);return;}
    if(type==0x80 || (type==0x90 && second==0)){noteOff(channel,first);return;}
    if(type==0xe0 && byteCount>=3){wheels[(size_t)channel]=first|(second<<7);for(auto& v:voices)if(v->state.channel==channel){v->state.wheel=wheels[(size_t)channel];v->state.cacheDirty=true;}return;}
    if(type!=0xb0 || byteCount<3)return;
    const float release=settings().release;
    if(first==64){sustain[(size_t)channel]=second>=64;if(second<64)for(auto& v:voices)if(v->state.channel==channel&&!v->keyDown&&!v->sostenuto)v->state.release(release,rate);}
    else if(first==66){for(auto& v:voices)if(v->state.channel==channel){if(second>=64 && v->keyDown)v->sostenuto=true;else if(second<64){v->sostenuto=false;if(!v->keyDown&&!sustain[(size_t)channel])v->state.release(release,rate);}}}
    else if(first==120 || first==123){sustain[(size_t)channel]=false;for(auto& v:voices)if(v->state.channel==channel){v->keyDown=false;v->sostenuto=false;if(first==120){v->state.active=false;v->tailLeft=0;}else v->state.release(release,rate);}}
    else if(first==121){sustain[(size_t)channel]=false;wheels[(size_t)channel]=8192;for(auto& v:voices)if(v->state.channel==channel){v->sostenuto=false;v->state.wheel=8192;v->state.cacheDirty=true;if(!v->keyDown)v->state.release(release,rate);}}
}
void HeavenVoiceEngine::render(juce::AudioBuffer<float>& output,int start,int count){if(count<=0)return;const auto p=settings();for(auto& v:voices)if(v->state.active||v->tailLeft>0)v->render(output,start,count,rate,p);}
int HeavenVoiceEngine::heldVoiceCount()const noexcept{int n=0;for(auto& v:voices)if(v->state.active&&v->keyDown)++n;return n;}
int HeavenVoiceEngine::activeVoiceCount()const noexcept{int n=0;for(auto& v:voices)if(v->state.active)++n;return n;}
