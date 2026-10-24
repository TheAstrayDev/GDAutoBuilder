#include "GeneratorCore.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <numeric>
#include <random>
#include <sstream>

namespace autobuilder {
namespace {
constexpr float START_X = 300.f;
constexpr float UNITS_PER_SECOND = 310.f;  // normal speed approximation
constexpr float GRID = 30.f;
float quant(float x) { return std::round(x / GRID) * GRID + 15.f; }
float toX(double t) { return quant(START_X + static_cast<float>(t) * UNITS_PER_SECOND); }
uint32_t readLE(const unsigned char* p, int n) {
    uint32_t x = 0; for (int i=0;i<n;i++) x |= uint32_t(p[i]) << (8*i); return x;
}
void put(std::vector<Object>& a, int id,float x,float y,Role role, int group=0) {
    Object o; o.id=id; o.x=x; o.y=y; o.role=role; o.group=group;
    a.push_back(o);
}
std::string num(double a) { std::ostringstream s; s << std::fixed << std::setprecision(2) << a; return s.str(); }
}
const std::vector<Song>& officialSongs() {
    static const std::vector<Song> songs = {
        {"Stereo Madness", 100, 95}, {"Back on Track", 128, 95},
        {"Polargeist", 128, 100}, {"Dry Out", 130, 95},
        {"Base After Base", 130, 100}, {"Can't Let Go", 126, 95},
        {"Jumper", 140, 100}, {"Time Machine", 128, 100},
        {"Cycles", 132, 100}, {"xStep", 128, 100},
        {"Clutterfunk", 128, 100}, {"Theory of Everything", 130, 100},
        {"Electroman Adventures", 130, 100}, {"Clubstep", 128, 100},
        {"Electrodynamix", 145, 100}, {"Hexagon Force", 132, 100},
        {"Blast Processing", 145, 100}, {"Theory of Everything 2", 135, 100},
        {"Geometrical Dominator", 145, 100}, {"Deadlocked", 140, 100},
        {"Fingerdash", 135, 100}, {"Dash", 142, 100}
    };
    // BPM and length values above are intentionally rough generation defaults,
    // NOT measurements of official full tracks.
    return songs;
}
std::vector<Beat> makeBeatGrid(double bpm, double seconds) {
    std::vector<Beat> beats;
    bpm = std::clamp(bpm, 40.0, 240.0);
    seconds = std::clamp(seconds, 10.0, 240.0);
    for (int i=0; (i*60.0/bpm)<seconds; ++i)
        beats.push_back({i*60.0/bpm, (i%4==0)?1.f:(i%2==0?0.7f:0.42f)});
    return beats;
}
std::vector<Beat> analyzeWav(const std::filesystem::path& path, double& seconds) {
    seconds = 0;
    std::error_code sizeError;
    const auto inputBytes = std::filesystem::file_size(path, sizeError);
    // Conserve RAM on Android; use a downsampled/short WAV if above 64 MiB.
    if(sizeError || inputBytes > 64ull*1024ull*1024ull) return {};
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::vector<unsigned char> b((std::istreambuf_iterator<char>(in)), {});
    if (b.size()<44 || std::string((char*)b.data(),4)!="RIFF" ||
        std::string((char*)b.data()+8,4)!="WAVE") return {};
    uint32_t sr=0, ch=0, bit=0, fmt=0, dataSize=0;
    size_t dataStart=0;
    for (size_t p=12;p+8<=b.size();) {
        uint32_t n=readLE(&b[p+4],4);
        if (n>b.size()-p-8) return {};
        std::string tag((char*)b.data()+p,4);
        if(tag=="fmt " && n>=16) {
            fmt=readLE(&b[p+8],2); ch=readLE(&b[p+10],2);
            sr=readLE(&b[p+12],4); bit=readLE(&b[p+22],2);
        } else if (tag=="data") { dataStart=p+8; dataSize=n; }
        p+=8+n+(n&1u);
    }
    if(fmt!=1 || (bit!=8 && bit!=16) || ch<1 || ch>2 || sr<8000 || sr>192000 || !dataStart) return {};
    const size_t frameBytes=(bit/8)*ch;
    const size_t frames=dataSize/frameBytes;
    if (frames==0) return {};
    seconds=static_cast<double>(frames)/sr;
    // Whole-track streaming of sound energy, 20ms windows. Each sample contributes.
    size_t stride=std::max<size_t>(1,sr/50);
    std::vector<double> rms;
    for(size_t start=0;start<frames;start+=stride) {
        double sum=0;
        const auto count=std::min(stride,frames-start);
        for(size_t f=start;f<start+count;++f) {
            double sample=0;
            for(size_t c=0;c<ch;c++) {
                size_t offset=dataStart+(f*ch+c)*(bit/8);
                if(bit==16) sample += static_cast<int16_t>(readLE(&b[offset],2))/32768.;
                else sample += (static_cast<int>(b[offset])-128)/128.;
            }
            sample/=ch; sum+=sample*sample;
        }
        rms.push_back(std::sqrt(sum/count));
    }
    std::vector<Beat> beats;
    double smooth=0, last=-1;
    // Adaptive envelope: select attacks that rise substantially above surroundings.
    for(size_t i=0;i<rms.size();i++) {
        smooth=0.91*smooth+0.09*rms[i];
        const double current=rms[i];
        double t=(i*stride+stride*0.5)/static_cast<double>(sr);
        if(i>=2 && i+1<rms.size() && t-last>=0.16 &&
           current>0.022 && current>smooth*1.32 &&
           current>rms[i-1]*1.04 && current>=rms[i+1]) {
           beats.push_back({t,static_cast<float>(std::clamp(current/(smooth+0.0001)/2.4,0.25,1.0))});
           last=t;
        }
    }
    return beats;
}
Plan generate(int songIndex, Difficulty difficulty, const std::vector<Beat>& input,
              double seconds, uint32_t seed) {
    Plan p;
    songIndex=std::clamp(songIndex,0,static_cast<int>(officialSongs().size())-1);
    const auto& song=officialSongs()[songIndex];
    p.lengthSeconds=seconds>0?seconds:song.fallbackSeconds;
    p.beats=input.empty()?makeBeatGrid(song.bpm,p.lengthSeconds):input;
    p.waveformAnalyzed=!input.empty();
    (void)seed;
    const int level=static_cast<int>(difficulty);
    const double minimumGap=level<=1 ? .85 : (level<=3 ? .60 : .45);
    double lastHazard=-5.0;
    int added=0;
    // Stage 1: layout geometry, slowly varying difficulty and obstacle complexity.
    for(size_t i=0;i<p.beats.size();i++) {
        const auto& beat=p.beats[i];
        float x=toX(beat.time);
        if(beat.time < 2.5 || beat.time > p.lengthSeconds-3) continue;
        // Rhythmic hazard choice; gaps are bounded by jump recovery time.
        bool strong=(beat.strength>0.53f);
        if(strong && beat.time-lastHazard >=minimumGap && i%2==0) {
            int spikes = 1 + ((level>=3 && i%7==0)?1:0) + ((level>=5 && i%13==0)?1:0);
            // Don't exceed space reserved for the next jump.
            for(int j=0;j<spikes;j++) {
                put(p.objects,8,x+j*30.f,15,Role::Gameplay);
                p.hazards.push_back(x+j*30.f);
                ++p.obstacleCount;
            }
            lastHazard=beat.time+spikes*.095;
            ++added;
        } else if (i%8==4 && beat.time-lastHazard>minimumGap/2) {
            // Elevated safe cube surface with an orb nearby; allows gameplay variety.
            // Each platform gets its own group so trigger movement can affect gameplay.
            const int platformGroup = 7200 + static_cast<int>(i % 1500);
            put(p.objects,1,x,90,Role::Gameplay,platformGroup);
            if(level>=1) put(p.objects,36,x+45,130,Role::Gameplay); // yellow jump ring
            if(level>=2 && i%16==4) {
                Object lift;
                lift.id=901; lift.x=x-90; lift.y=410;
                lift.role=Role::RhythmTrigger;
                lift.targetGroup=platformGroup;
                lift.duration=0.20f; lift.dy=15.f;
                p.objects.push_back(lift);
                Object lower=lift;
                lower.x=x+130; lower.y=410; lower.dy=-15.f;
                p.objects.push_back(lower);
            }
        }
        // Mode switches at section boundaries (spaced apart, optional for harder modes).
        if(level>=3 && i!=0 && i%64==0) put(p.objects,13,x,75,Role::Gameplay); // ship portal
        if(level>=3 && i!=0 && i%64==32) put(p.objects,12,x,75,Role::Gameplay); // cube portal
    }
    // Stage 2: grouped simple geometry and visual rhythm strips.
    const int decorativeGroup=9000;
    const int animatedGroup=9001;
    for(size_t i=0;i<p.beats.size();i+=2) {
        const auto& beat=p.beats[i];
        const float x=toX(beat.time);
        if (beat.time>p.lengthSeconds-1) break;
        put(p.objects,1,x,240+(i%4==0?30.f:0.f),Role::Decoration,decorativeGroup);
        if(i%4==0) put(p.objects,1,x,300,Role::Decoration,animatedGroup);
    }
    // Stage 3: group-controlled pulses and movement plus ambience shakes.
    for(size_t i=0;i<p.beats.size();i+=4) {
        const auto& beat=p.beats[i];
        const float x=toX(beat.time);
        Object pulse;
        pulse.id=1006; pulse.x=x; pulse.y=405;
        pulse.role=Role::DecorationTrigger; pulse.targetGroup=decorativeGroup;
        pulse.r=(i/4)%2?95:255; pulse.g=(i/4)%2?215:90;
        pulse.b=250; pulse.fadeOut=0.2f;
        p.objects.push_back(pulse);
        if(i%16==0) {
            Object move;
            move.id=901; move.x=x+15; move.y=435;
            move.role=Role::DecorationTrigger; move.targetGroup=animatedGroup;
            move.duration=0.28f;
            move.dy=(i%32==0)?12.f:-12.f;
            p.objects.push_back(move);
        }
        if(level>=4 && i%32==0) {
            Object shake;
            shake.id=1520; shake.x=x+20; shake.y=465;
            shake.role=Role::DecorationTrigger;
            shake.duration=0.10f;
            p.objects.push_back(shake);
        }
    }
    p.summary="Base pass: "+std::to_string(p.obstacleCount)+" hazards, "+
              std::to_string(p.objects.size())+" objects, "+
              std::to_string(p.beats.size())+" rhythm markers";
    return p;
}
std::vector<Object> improve(Plan& plan, Difficulty difficulty, uint32_t seed) {
    std::vector<Object> fixes;
    if(plan.improvementPass>=3) return fixes;
    ++plan.improvementPass;
    std::mt19937 rng(seed+plan.improvementPass*7);
    // Review excessive vertical repetition and insert safe interaction opportunities.
    float previous=-10000;
    const float minDistance=(static_cast<int>(difficulty)<=1)?300.f:210.f;
    for(auto const& beat:plan.beats) {
        float x=toX(beat.time);
        if(x<550 || beat.time>plan.lengthSeconds-3) continue;
        const bool nearObstacle=std::any_of(plan.hazards.begin(),plan.hazards.end(),[&](float h){ return std::abs(h-x)<95.f; });
        if(!nearObstacle && x-previous>minDistance && beat.strength>0.65f && (rng()%4==0)) {
            // Yellow orb is placed above the travel path, not directly in a spike.
            put(fixes,36,x,130,Role::Gameplay);
            previous=x;
        }
    }
    // Add accent lighting to bars, with independent group so existing geometry is untouched.
    const int group=9010+plan.improvementPass;
    for(size_t i=0;i<plan.beats.size();i+=8) {
        float x=toX(plan.beats[i].time);
        put(fixes,1,x,345,Role::Decoration,group);
        Object pulse; pulse.id=1006; pulse.x=x; pulse.y=495;
        pulse.role=Role::DecorationTrigger;
        pulse.targetGroup=group; pulse.r=240; pulse.g=140; pulse.b=255;
        fixes.push_back(pulse);
    }
    plan.objects.insert(plan.objects.end(),fixes.begin(),fixes.end());
    plan.summary="Improvement #"+std::to_string(plan.improvementPass)+
       ": added "+std::to_string(fixes.size())+" objects after gap review";
    return fixes;
}
std::string serialize(const std::vector<Object>& objects) {
    std::ostringstream s;
    for(auto& o:objects) {
        s<<"1,"<<o.id<<",2,"<<num(o.x)<<",3,"<<num(o.y);
        if(o.group) s<<",57,"<<o.group;
        if(o.targetGroup) s<<",51,"<<o.targetGroup;
        if(o.id==901) s<<",10,"<<num(o.duration)<<",28,"<<num(o.dx)<<",29,"<<num(o.dy);
        if(o.id==1006) s<<",7,"<<int(o.r)<<",8,"<<int(o.g)<<",9,"<<int(o.b)
                        <<",45,"<<num(o.fadeIn)<<",46,"<<num(o.hold)<<",47,"<<num(o.fadeOut)<<",52,1";
        if(o.id==1520) s<<",10,"<<num(o.duration)<<",75,0.3,84,0.08";
        s<<";";
    }
    return s.str();
}
}
