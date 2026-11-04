#pragma once

#include <array>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace FuncGameRules
{
    enum class Difficulty { Easy, Hard };
    enum class State { Menu, Playing, Won, Lost };
    using Wave = std::array<int, 3>;

    class Session
    {
    public:
        // Each row contains level 1, 2 and 3 counts. Invalid plans leave the old plan untouched.
        bool SetWavePlan(const std::vector<Wave>& Plan);
        void Start(Difficulty InDifficulty, int InLevel = 1);
        bool RegisterSpawn(std::int64_t EnemyId);
        bool RegisterDeath(std::int64_t EnemyId);
        bool RegisterObstacleHit(std::int64_t ProjectileId);
        void EnemyReachedDefence();

        State GetState() const { return CurrentState; }
        Difficulty GetDifficulty() const { return CurrentDifficulty; }
        int GetLevel() const { return Level; }
        int GetWave() const { return WaveIndex + 1; }
        int GetPendingCount() const { return static_cast<int>(PendingLevels.size()); }
        int GetAliveCount() const { return static_cast<int>(LivingEnemies.size()); }
        int GetSpawnedCount() const { return Spawned; }
        int GetKilledCount() const { return Killed; }
        int GetTotalCount() const { return TotalEnemies; }
        int GetRemainingCount() const { return TotalEnemies - Killed; }
        int GetMistakesRemaining() const { return Mistakes; }
        int GetNextEnemyLevel() const { return PendingLevels.empty() ? 0 : PendingLevels.back(); }

    private:
        void BeginWave();

        State CurrentState = State::Menu;
        Difficulty CurrentDifficulty = Difficulty::Easy;
        int Level = 1;
        int WaveIndex = 0;
        int Spawned = 0;
        int Killed = 0;
        int Mistakes = 3;
        int TotalEnemies = 30;
        std::vector<Wave> Waves = {{1,1,0},{1,2,0},{1,3,1},{1,4,3},{1,4,7}};
        std::vector<int> PendingLevels;
        std::unordered_set<std::int64_t> LivingEnemies;
        std::unordered_set<std::int64_t> ProcessedProjectiles;
    };
}
