#include "../Source/FuncGame/Gameplay/CombatRules.h"
#include <iostream>
#include <set>
#include <vector>

namespace
{
    int Failures=0;
    void Check(bool Condition,const char* Message) { if(!Condition) { std::cerr << "FAIL: " << Message << '\n'; ++Failures; } }
}

int main()
{
    using namespace FuncGameRules;
    Session S;
    S.Start(Difficulty::Hard);
    Check(S.GetMistakesRemaining()==3 && S.GetWave()==1,"initial state");
    S.RegisterObstacleHit(1);
    S.RegisterObstacleHit(1);
    Check(S.GetMistakesRemaining()==2,"obstacle hit is deduplicated");
    S.RegisterObstacleHit(2);
    Check(S.GetState()==State::Playing && S.GetMistakesRemaining()==1,"two mistakes do not fail");
    S.RegisterObstacleHit(3);
    Check(S.GetState()==State::Lost && S.GetMistakesRemaining()==0,"third hard-mode hit fails");
    S.Start(Difficulty::Easy,2);
    for (int I=0; I<10; ++I) S.RegisterObstacleHit(I);
    Check(S.GetMistakesRemaining()==3 && S.GetState()==State::Playing,"easy mode does not lose chances");
    const int Counts[]={2,3,5,8,12};
    std::int64_t NextId=1;
    for (int Wave=0; Wave<5; ++Wave)
    {
        Check(S.GetWave()==Wave+1 && S.GetPendingCount()==Counts[Wave],"wave counts");
        std::set<int> Levels;
        std::vector<std::int64_t> Ids;
        while (S.GetPendingCount()>0)
        {
            Levels.insert(S.GetNextEnemyLevel());
            Ids.push_back(NextId);
            Check(S.RegisterSpawn(NextId++),"spawn succeeds");
        }
        Check(Levels.size()>=2,"wave mixes levels");
        for (auto Id:Ids) Check(S.RegisterDeath(Id),"death succeeds once");
        Check(!S.RegisterDeath(Ids.back()),"duplicate death ignored");
    }
    Check(S.GetState()==State::Won && S.GetSpawnedCount()==30 && S.GetKilledCount()==30,"victory requires all thirty");
    S.Start(Difficulty::Hard);
    Check(S.RegisterSpawn(101),"partial spawn");
    Check(S.RegisterDeath(101),"partial wave death");
    Check(S.GetWave()==1 && S.GetPendingCount()==1 && S.GetState()==State::Playing,"pending enemies block wave advancement");
    S.EnemyReachedDefence();
    Check(S.GetState()==State::Lost && S.GetMistakesRemaining()==3,"defence failure is separate from mistakes");
    Check(!S.SetWavePlan({{1,0,0}}),"single-level wave configuration is rejected");
    Check(!S.SetWavePlan({{-1,2,0}}),"negative wave counts are rejected");
    Check(S.SetWavePlan({{1,1,0},{0,1,1}}),"valid custom wave plan is accepted");
    S.Start(Difficulty::Easy);
    Check(S.GetTotalCount()==4 && S.GetRemainingCount()==4,"configured total is calculated from the waves");
    for (int Id=1;Id<=4;++Id)
    {
        Check(S.RegisterSpawn(Id),"configured spawn");
        Check(S.RegisterDeath(Id),"configured death");
    }
    Check(S.GetState()==State::Won && S.GetRemainingCount()==0,"configured wave plan can be completed");
    if(Failures==0) std::cout << "All combat-rule tests passed.\n";
    return Failures==0 ? 0 : 1;
}
