#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace autobuilder {

enum class Difficulty { Easy, Normal, Hard, Harder, Insane, Demon };
enum class Role { Gameplay, Decoration, RhythmTrigger, DecorationTrigger };
struct Song { const char* name; double bpm; double fallbackSeconds; };
const std::vector<Song>& officialSongs();

struct Beat { double time = 0.0; float strength = 0.5f; };
struct Object {
    int id = 1;
    float x = 0, y = 0;
    int group = 0;
    Role role = Role::Gameplay;
    // Explicit trigger options; unused properties remain at game defaults.
    int targetGroup = 0;
    float duration = 0, dx = 0, dy = 0, opacity = 1;
    uint8_t r = 255, g = 255, b = 255;
    float fadeIn = 0.05f, hold = 0.08f, fadeOut = 0.16f;
};
struct Plan {
    std::vector<Object> objects;
    std::vector<Beat> beats;
    std::vector<float> hazards;
    double lengthSeconds = 0;
    int improvementPass = 0;
    bool waveformAnalyzed = false;
    int obstacleCount = 0;
    std::string summary;
};

// Fully scans PCM WAV (mono/stereo, 8/16-bit), searching amplitude onsets.
// No commercial tracks are distributed with this project.
std::vector<Beat> analyzeWav(const std::filesystem::path& path, double& seconds);
std::vector<Beat> makeBeatGrid(double bpm, double lengthSeconds);
Plan generate(int songIndex, Difficulty difficulty, const std::vector<Beat>& beats,
              double seconds, uint32_t seed = 12345);
// Non-destructive pass that evaluates spacing and decor/trigger coverage,
// returning ONLY new objects suitable for appending to the existing level.
std::vector<Object> improve(Plan& plan, Difficulty difficulty, uint32_t seed = 98765);
// GD's format can be inspected/exported separately; this serializer is for QA and backups.
std::string serialize(const std::vector<Object>& objects);
}
