#include "CombatRules.h"

namespace FuncGameRules
{
    bool Session::SetWavePlan(const std::vector<Wave>& Plan)
    {
        if (Plan.empty() || Plan.size()>50) return false;
        int Total=0;
        for (const Wave& Entry:Plan)
        {
            int Kinds=0;
            for (int Count:Entry)
            {
                if (Count<0 || Count>1000) return false;
                if (Count>0) ++Kinds;
                Total+=Count;
                if (Total>1000) return false;
            }
            if (Kinds<2) return false;
        }
        Waves=Plan;
        TotalEnemies=Total;
        return true;
    }

    void Session::Start(Difficulty InDifficulty, int InLevel)
    {
        CurrentDifficulty=InDifficulty;
        CurrentState=State::Playing;
        Level=InLevel>0 ? InLevel : 1;
        WaveIndex=Spawned=Killed=0;
        Mistakes=3;
        PendingLevels.clear();
        LivingEnemies.clear();
        ProcessedProjectiles.clear();
        BeginWave();
    }

    void Session::BeginWave()
    {
        PendingLevels.clear();
        if (WaveIndex<0 || WaveIndex>=static_cast<int>(Waves.size())) return;
        for (int EnemyLevel=1; EnemyLevel<=3; ++EnemyLevel)
            for (int I=0; I<Waves[WaveIndex][EnemyLevel-1]; ++I)
                PendingLevels.push_back(EnemyLevel);
    }

    bool Session::RegisterSpawn(std::int64_t EnemyId)
    {
        if (CurrentState!=State::Playing || PendingLevels.empty() || LivingEnemies.count(EnemyId)!=0) return false;
        LivingEnemies.insert(EnemyId);
        PendingLevels.pop_back();
        ++Spawned;
        return true;
    }

    bool Session::RegisterDeath(std::int64_t EnemyId)
    {
        if (CurrentState!=State::Playing || LivingEnemies.erase(EnemyId)==0) return false;
        ++Killed;
        if (LivingEnemies.empty() && PendingLevels.empty())
        {
            if (WaveIndex+1==static_cast<int>(Waves.size()) && Killed==TotalEnemies && Spawned==TotalEnemies) CurrentState=State::Won;
            else { ++WaveIndex; BeginWave(); }
        }
        return true;
    }

    bool Session::RegisterObstacleHit(std::int64_t ProjectileId)
    {
        if (CurrentState!=State::Playing || !ProcessedProjectiles.insert(ProjectileId).second) return false;
        if (CurrentDifficulty==Difficulty::Hard && --Mistakes<=0)
        {
            Mistakes=0;
            CurrentState=State::Lost;
        }
        return true;
    }

    void Session::EnemyReachedDefence()
    {
        if (CurrentState==State::Playing) CurrentState=State::Lost;
    }
}
