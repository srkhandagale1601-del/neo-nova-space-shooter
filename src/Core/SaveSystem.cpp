#include "Core/SaveSystem.hpp"
#include "Core/Config.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>

static std::vector<ScoreEntry> leaderboard;
static int cachedHighScore = 0;

void SaveSystem::init()
{
    loadSettings();
    loadScores();
}

void SaveSystem::loadSettings()
{
    std::ifstream file("settings.cfg");
    if (!file.is_open())
    {
        return;
    }

    std::string key;
    float fval;
    int ival;

    while (file >> key)
    {
        if (key == "masterVolume" && (file >> fval)) masterVolume = fval;
        else if (key == "sfxVolume" && (file >> fval)) sfxVolume = fval;
        else if (key == "musicVolume" && (file >> fval)) musicVolume = fval;
        else if (key == "screenShake" && (file >> ival)) screenShakeSetting = ival;
        else if (key == "shipAccent" && (file >> ival)) shipAccentSetting = ival;
        else if (key == "shipArchetype" && (file >> ival)) selectedShipArchetype = ival;
    }
}

void SaveSystem::saveSettings()
{
    std::ofstream file("settings.cfg");
    if (!file.is_open())
        return;

    file << "masterVolume " << masterVolume << "\n";
    file << "sfxVolume " << sfxVolume << "\n";
    file << "musicVolume " << musicVolume << "\n";
    file << "screenShake " << screenShakeSetting << "\n";
    file << "shipAccent " << shipAccentSetting << "\n";
    file << "shipArchetype " << selectedShipArchetype << "\n";
}

void SaveSystem::loadScores()
{
    leaderboard.clear();
    std::ifstream file("leaderboard.dat");
    if (!file.is_open())
    {
        // Default entries if file doesn't exist
        leaderboard.push_back({"ACE-VANGUARD", 1500, 3});
        leaderboard.push_back({"NOVA-PILOT", 850, 2});
        leaderboard.push_back({"STAR-CADET", 350, 1});
        cachedHighScore = 1500;
        return;
    }

    std::string name;
    int sc, sec;
    while (file >> name >> sc >> sec)
    {
        leaderboard.push_back({name, sc, sec});
    }

    std::sort(leaderboard.begin(), leaderboard.end(), [](const ScoreEntry& a, const ScoreEntry& b) {
        return a.score > b.score;
    });

    if (!leaderboard.empty())
    {
        cachedHighScore = leaderboard[0].score;
    }
}

void SaveSystem::saveScores()
{
    std::ofstream file("leaderboard.dat");
    if (!file.is_open())
        return;

    for (const auto& entry : leaderboard)
    {
        file << entry.pilotName << " " << entry.score << " " << entry.sector << "\n";
    }
}

void SaveSystem::registerScore(int score, int sector, const std::string& name)
{
    if (score <= 0) return;

    leaderboard.push_back({name, score, sector});
    std::sort(leaderboard.begin(), leaderboard.end(), [](const ScoreEntry& a, const ScoreEntry& b) {
        return a.score > b.score;
    });

    if (leaderboard.size() > 5)
    {
        leaderboard.resize(5);
    }

    if (!leaderboard.empty())
    {
        cachedHighScore = leaderboard[0].score;
    }

    saveScores();
}

const std::vector<ScoreEntry>& SaveSystem::getLeaderboard()
{
    return leaderboard;
}

int SaveSystem::getHighScore()
{
    return cachedHighScore;
}
