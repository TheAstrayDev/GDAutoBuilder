#include "../src/GeneratorCore.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <filesystem>
int main() {
    auto path=std::filesystem::temp_directory_path()/"test_geode_builder_song_0.wav";
    int sampleRate=8000;
    int frames=sampleRate*12;
    auto le=[](std::ofstream& f,uint32_t n,int len) { for(int i=0;i<len;i++) {unsigned char c=static_cast<unsigned char>(n>>(i*8));f.put(static_cast<char>(c));} };
    {std::ofstream f(path,std::ios::binary);
    f.write("RIFF",4);le(f,36+2*frames,4);f.write("WAVE",4);
    f.write("fmt ",4);le(f,16,4);le(f,1,2);le(f,1,2);le(f,sampleRate,4);le(f,sampleRate*2,4);le(f,2,2);le(f,16,2);
    f.write("data",4);le(f,2*frames,4);
    for(int i=0;i<frames;i++) {int inPeriod=i%(sampleRate/2);int16_t s=0;
    if(inPeriod < 650) s=static_cast<int16_t>(std::sin(i*2*3.1415926*440/sampleRate)*22000);
    le(f,static_cast<uint16_t>(s),2);
    }}
    double duration=0;
    auto beats=autobuilder::analyzeWav(path,duration);
    std::cout << "Waveform scan: " << beats.size() << " onsets in " << duration << " s\n";
    assert(std::abs(duration-12)<0.01);
    assert(beats.size()>=15);
    std::filesystem::remove(path);
}
