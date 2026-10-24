#include "../src/GeneratorCore.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <fstream>
#include <filesystem>
using namespace autobuilder;
int main() {
  assert(officialSongs().size() == 22);
  auto beats=makeBeatGrid(120,32);
  assert(beats.size()==64);
  auto easy=generate(1,Difficulty::Easy,{},35,4);
  assert(easy.objects.size()>20 && easy.beats.size()>20 && easy.obstacleCount>0);
  assert(easy.summary.find("Base pass")!=std::string::npos);
  auto before=easy.objects.size();
  auto upgrade=improve(easy,Difficulty::Easy,4);
  assert(!upgrade.empty() && easy.objects.size()==before+upgrade.size());
  assert(improve(easy,Difficulty::Easy,4).size()>0);
  assert(improve(easy,Difficulty::Easy,4).size()>0);
  assert(improve(easy,Difficulty::Easy,4).empty());
  auto text=serialize(easy.objects);
  assert(text.find("1,1006")!=std::string::npos);
  assert(text.find("51,9000")!=std::string::npos);
  std::cout << easy.summary << "\nserialized bytes: " << text.size() << "\n";
}
