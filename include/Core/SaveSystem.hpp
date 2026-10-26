#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>

// ======================================================
// SAVE SYSTEM - Developer 3
// Handles persistent high scores, settings, and stats.
// File: savegame.dat  (pipe-delimited plain text)
// ======================================================

struct ScoreEntry
{
    std::string pilotName;
    int         score;
    int         sector;
    std::string date;   // "YYYY-MM-DD HH:MM"
};

// Forward declarations of settings variables (defined in main.cpp)
extern float masterVolume;
extern float sfxVolume;
extern float musicVolume;
extern int   screenShakeSetting;
extern int   shipAccentSetting;
extern int   selectedShipArchetype;

class SaveSystem
{
public:
    // ---- Lifecycle ----
    static void init();          // Load settings + scores on startup
    static void shutdown();      // Save settings + scores on exit

    // ---- Settings ----
    static void loadSettings();
    static void saveSettings();

    // ---- High Scores ----
    static void loadScores();
    static void saveScores();

    /// Register a score. Keeps the top-10 sorted descending.
    static void registerScore(int score, int sector, const std::string& name = "PILOT");

    static const std::vector<ScoreEntry>& getLeaderboard();
    static int  getHighScore();

private:
    static std::vector<ScoreEntry> s_board;

    static const int    MAX_SCORES   = 10;
    static const char*  SCORES_FILE;   // "scores.dat"
    static const char*  SETTINGS_FILE; // "settings.dat"

    // Helper: get today's date-time string
    static std::string currentDateStr()
    {
        time_t now = time(nullptr);
        char buf[32];
        struct tm* tm_info = localtime(&now);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm_info);
        return std::string(buf);
    }
};

// ---- Static member definitions (header-only implementation) ----

inline const char* SaveSystem::SCORES_FILE   = "scores.dat";
inline const char* SaveSystem::SETTINGS_FILE = "settings.dat";
inline std::vector<ScoreEntry> SaveSystem::s_board;

inline void SaveSystem::init()
{
    loadSettings();
    loadScores();
}

inline void SaveSystem::shutdown()
{
    saveSettings();
    saveScores();
}

inline void SaveSystem::loadSettings()
{
    std::ifstream f(SETTINGS_FILE);
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string key;
        float val = 0.0f;
        if (std::getline(ss, key, '=') && ss >> val)
        {
            if (key == "masterVolume")       masterVolume       = val;
            else if (key == "sfxVolume")     sfxVolume          = val;
            else if (key == "musicVolume")   musicVolume        = val;
            else if (key == "screenShake")   screenShakeSetting = (int)val;
            else if (key == "shipAccent")    shipAccentSetting  = (int)val;
            else if (key == "shipArchetype") selectedShipArchetype = (int)val;
        }
    }
}

inline void SaveSystem::saveSettings()
{
    std::ofstream f(SETTINGS_FILE);
    if (!f.is_open()) return;

    f << "# NEO NOVA Settings - auto-generated\n";
    f << "masterVolume="       << masterVolume        << "\n";
    f << "sfxVolume="          << sfxVolume           << "\n";
    f << "musicVolume="        << musicVolume         << "\n";
    f << "screenShake="        << screenShakeSetting  << "\n";
    f << "shipAccent="         << shipAccentSetting   << "\n";
    f << "shipArchetype="      << selectedShipArchetype << "\n";
}

inline void SaveSystem::loadScores()
{
    s_board.clear();
    std::ifstream f(SCORES_FILE);
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#') continue;
        // Format: NAME|SCORE|SECTOR|DATE
        std::istringstream ss(line);
        ScoreEntry e;
        std::string scoreStr, sectorStr;
        if (std::getline(ss, e.pilotName, '|') &&
            std::getline(ss, scoreStr,    '|') &&
            std::getline(ss, sectorStr,   '|') &&
            std::getline(ss, e.date))
        {
            e.score  = std::stoi(scoreStr);
            e.sector = std::stoi(sectorStr);
            s_board.push_back(e);
        }
    }
}

inline void SaveSystem::saveScores()
{
    std::ofstream f(SCORES_FILE);
    if (!f.is_open()) return;

    f << "# NEO NOVA High Scores - auto-generated\n";
    for (const auto& e : s_board)
    {
        f << e.pilotName << "|" << e.score << "|" << e.sector << "|" << e.date << "\n";
    }
}

inline void SaveSystem::registerScore(int score, int sector, const std::string& name)
{
    if (score <= 0) return;

    ScoreEntry e;
    e.pilotName = name;
    e.score     = score;
    e.sector    = sector;
    e.date      = currentDateStr();

    s_board.push_back(e);

    // Sort descending by score
    std::sort(s_board.begin(), s_board.end(),
        [](const ScoreEntry& a, const ScoreEntry& b){ return a.score > b.score; });

    // Keep only top MAX_SCORES
    if ((int)s_board.size() > MAX_SCORES)
        s_board.resize(MAX_SCORES);

    saveScores(); // Persist immediately
}

inline const std::vector<ScoreEntry>& SaveSystem::getLeaderboard()
{
    return s_board;
}

inline int SaveSystem::getHighScore()
{
    if (s_board.empty()) return 0;
    return s_board[0].score;
}
