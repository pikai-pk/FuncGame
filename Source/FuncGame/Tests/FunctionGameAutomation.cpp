#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "../FunctionGameMode.h"
#include "../FunctionGameWidget.h"
#include "../Math/FunctionExpression.h"
#include "../Math/FunctionTrajectory.h"
#include "Widgets/SWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFunctionMathAutomation, "FuncGame.Math.ParserAndTrajectory",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFunctionMathAutomation::RunTest(const FString&)
{
    FuncMath::Expression Expr;
    FuncMath::Error Error;
    double Value=0;
    TestTrue(TEXT("compile right-associative power"),FuncMath::Expression::Compile("2^3^2",Expr,Error));
    TestTrue(TEXT("evaluate power"),Expr.Evaluate(0,Value,Error));
    TestEqual(TEXT("power result"),Value,512.0);
    TestTrue(TEXT("compile lazy condition"),FuncMath::Expression::Compile("if(x>=0,sqrt(x),sqrt(-x))",Expr,Error));
    TestTrue(TEXT("unselected branch is not evaluated"),Expr.Evaluate(4,Value,Error));
    TestEqual(TEXT("lazy result"),Value,2.0);
    TestFalse(TEXT("unknown name is rejected"),FuncMath::Expression::Compile("sinx",Expr,Error));
    TestTrue(TEXT("compile logarithm"),FuncMath::Expression::Compile("ln(x)",Expr,Error));
    FuncMath::Trajectory Path;
    TestTrue(TEXT("logarithm finds usable start"),FuncMath::BuildTrajectory(Expr,{},Path,Error));
    TestTrue(TEXT("logarithm starts above zero"),Path.StartX>0);
    TestTrue(TEXT("compile pole"),FuncMath::Expression::Compile("1/(x-2)",Expr,Error));
    TestTrue(TEXT("pole has usable initial trajectory"),FuncMath::BuildTrajectory(Expr,{},Path,Error));
    TestTrue(TEXT("trajectory stops before pole"),Path.StoppedEarly && Path.StartX+Path.Points.back().Progress<2.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFunctionWorldSmoke, "FuncGame.World.PlayableSmoke",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FFunctionWorldSmoke::RunTest(const FString&)
{
    UWorld* World=nullptr;
    if (GEngine)
        for (const FWorldContext& Context:GEngine->GetWorldContexts())
            if (Context.World() && Context.World()->IsGameWorld()) { World=Context.World(); break; }
    AFunctionGameMode* Game=World ? Cast<AFunctionGameMode>(World->GetAuthGameMode()) : nullptr;
    if (!TestNotNull(TEXT("game world uses FunctionGameMode"),Game)) return false;
    UFunctionGameWidget* Widget=Game->GetGameWidget();
    if (!TestNotNull(TEXT("game widget exists"),Widget)) return false;
    TSharedRef<SWidget> SlateWidget=Widget->TakeWidget();
    FChildren* Children=SlateWidget->GetChildren();
    if (TestTrue(TEXT("widget has a Slate child"),Children && Children->Num()>0))
        TestEqual(TEXT("rendered widget root is a canvas"),Children->GetChildAt(0)->GetTypeAsString(),FString(TEXT("SConstraintCanvas")));

    TestFalse(TEXT("unknown expression rejected"),Game->UpdateExpression(TEXT("abcdefg"),TEXT("")));
    TestTrue(TEXT("linear expression accepted"),Game->UpdateExpression(TEXT("0"),TEXT("")));
    Game->StartLevel(false,1);
    for (int I=0;I<6;++I) Game->Tick(0.1f);
    TestEqual(TEXT("first wave creates two enemies"),Game->GetSpawnedEnemyCount(),2);
    TestTrue(TEXT("fire is enabled"),Game->CanFire());
    Game->Fire();
    TestEqual(TEXT("projectile exists"),Game->GetShotCount(),1);
    for (int I=0;I<100 && Game->GetShotCount()>0;++I) Game->Tick(0.1f);
    TestEqual(TEXT("obstacle removes the projectile"),Game->GetShotCount(),0);
    TestEqual(TEXT("easy mode keeps all chances"),Game->GetMistakesRemaining(),3);

    Game->StartLevel(true,1);
    for (int Shot=0;Shot<3;++Shot)
    {
        TestTrue(TEXT("hard mode can fire"),Game->CanFire());
        Game->Fire();
        for (int I=0;I<100 && Game->GetShotCount()>0;++I) Game->Tick(0.1f);
    }
    TestEqual(TEXT("three obstacle hits consume three chances"),Game->GetMistakesRemaining(),0);
    TestTrue(TEXT("hard mode ends after third hit"),Game->IsFinished() && !Game->IsWon());
    TestTrue(TEXT("failure explains the obstacle mistakes"),Game->GetResultText().Contains(TEXT("障碍物")));
    Game->ReturnToMenu();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFunctionWorldCollision, "FuncGame.World.ProjectileCollision",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FFunctionWorldCollision::RunTest(const FString&)
{
    UWorld* World=nullptr;
    if (GEngine)
        for (const FWorldContext& Context:GEngine->GetWorldContexts())
            if (Context.World() && Context.World()->IsGameWorld()) { World=Context.World(); break; }
    AFunctionGameMode* Game=World ? Cast<AFunctionGameMode>(World->GetAuthGameMode()) : nullptr;
    if (!TestNotNull(TEXT("game world uses FunctionGameMode"),Game)) return false;

    auto EnemyTint=[](ACharacter* Enemy)
    {
        if (!Enemy || !Enemy->GetMesh() || Enemy->GetMesh()->GetNumMaterials()==0) return FLinearColor::Black;
        UMaterialInstanceDynamic* Dynamic=Cast<UMaterialInstanceDynamic>(Enemy->GetMesh()->GetMaterial(0));
        return Dynamic ? Dynamic->K2_GetVectorParameterValue(TEXT("Tint")) : FLinearColor::Black;
    };

    auto Prepare=[&]()
    {
        Game->StartLevel(false,1);
        Game->UpdateExpression(TEXT("0"),TEXT(""));
        for (int I=0;I<6;++I) Game->Tick(0.1f);
        return Game->Enemies.Num()==2 && Game->Enemies[0].Actor.IsValid() && Game->Enemies[1].Actor.IsValid();
    };
    auto RunShot=[&]()
    {
        for (int I=0;I<100 && Game->GetShotCount()>0;++I) Game->UpdateShots(0.1f);
    };

    if (!TestTrue(TEXT("two enemies available for piercing test"),Prepare())) return false;
    TestEqual(TEXT("first enemy starts at level two"),Game->Enemies[0].Health,2);
    TestEqual(TEXT("second enemy starts at level one"),Game->Enemies[1].Health,1);
    TestTrue(TEXT("level two enemy starts yellow"),EnemyTint(Game->Enemies[0].Actor.Get()).Equals(FLinearColor::Yellow,0.01f));
    TestTrue(TEXT("level one enemy starts green"),EnemyTint(Game->Enemies[1].Actor.Get()).Equals(FLinearColor::Green,0.01f));
    TestEqual(TEXT("default configured total is thirty"),Game->Rules.GetTotalCount(),30);
    TestEqual(TEXT("default configuration has five waves"),Game->WaveComposition.Num(),5);
    if (TestTrue(TEXT("trajectory preview is present"),Game->PreviewMesh.IsValid()))
    {
        const int32 Count=Game->PreviewMesh->GetInstanceCount();
        if (TestTrue(TEXT("valid expression draws preview"),Count>0))
        {
            FTransform LastSegment;
            TestTrue(TEXT("read last preview segment"),Game->PreviewMesh->GetInstanceTransform(Count-1,LastSegment,true));
            const double PreviewEnd=LastSegment.GetLocation().X+LastSegment.GetScale3D().X*50.0;
            TestTrue(TEXT("preview stops at the obstacle's projectile boundary"),FMath::IsNearlyEqual(PreviewEnd,486.0,1.0));
        }
        TestFalse(TEXT("invalid expression is rejected"),Game->UpdateExpression(TEXT("badname"),TEXT("")));
        TestEqual(TEXT("invalid expression clears preview"),Game->PreviewMesh->GetInstanceCount(),0);
        TestTrue(TEXT("valid expression restores preview"),Game->UpdateExpression(TEXT("0"),TEXT("")));
    }
    Game->Enemies[0].Actor->SetActorLocation(FVector(-650,0,90));
    Game->Enemies[1].Actor->SetActorLocation(FVector(-350,0,90));
    const FVector BeforePause=Game->Enemies[0].Actor->GetActorLocation();
    Game->SetHelpOpen(true);
    TestFalse(TEXT("help disables firing"),Game->CanFire());
    Game->Tick(0.1f);
    TestTrue(TEXT("help freezes enemies"),Game->Enemies[0].Actor->GetActorLocation().Equals(BeforePause));
    Game->SetHelpOpen(false);

    Game->SetPaused(true);
    TestFalse(TEXT("manual pause disables firing"),Game->CanFire());
    Game->Tick(0.1f);
    TestTrue(TEXT("manual pause freezes enemies"),Game->Enemies[0].Actor->GetActorLocation().Equals(BeforePause));
    TestTrue(TEXT("expression can be changed while paused"),Game->UpdateExpression(TEXT("x"),TEXT("")));
    TestFalse(TEXT("changing expression does not resume firing"),Game->CanFire());
    Game->SetHelpOpen(true);
    Game->SetHelpOpen(false);
    TestTrue(TEXT("closing help preserves manual pause"),Game->IsPaused());
    Game->SetPaused(false);
    TestTrue(TEXT("resuming enables firing"),Game->CanFire());
    Game->UpdateExpression(TEXT("0"),TEXT(""));

    Game->Fire();
    TestEqual(TEXT("one projectile was launched"),Game->GetShotCount(),1);
    Game->UpdateExpression(TEXT("x"),TEXT(""));
    TestTrue(TEXT("launched path does not change when input changes"),
        Game->Shots.Num()==1 && FMath::IsNearlyZero(Game->Shots[0].Points.back().Height));
    RunShot();
    TestEqual(TEXT("level two enemy drops to level one after one hit"),Game->Enemies[0].Health,1);
    TestTrue(TEXT("downgraded enemy changes from yellow to green"),EnemyTint(Game->Enemies[0].Actor.Get()).Equals(FLinearColor::Green,0.01f));
    TestEqual(TEXT("piercing projectile kills level one enemy immediately"),Game->Rules.GetKilledCount(),1);
    Game->UpdateExpression(TEXT("0"),TEXT(""));
    Game->Fire();
    RunShot();
    TestEqual(TEXT("a second projectile kills the remaining level one enemy"),Game->Rules.GetKilledCount(),2);
    TestEqual(TEXT("next wave begins only after both deaths"),Game->Rules.GetWave(),2);

    if (!TestTrue(TEXT("two enemies available for obstacle order test"),Prepare())) return false;
    Game->Enemies[0].Actor->SetActorLocation(FVector(400,0,90));
    Game->Enemies[1].Actor->SetActorLocation(FVector(700,0,90));
    Game->Fire();
    RunShot();
    TestEqual(TEXT("enemy before obstacle takes damage"),Game->Enemies[0].Health,1);
    TestEqual(TEXT("enemy behind obstacle is protected"),Game->Enemies[1].Health,1);
    TestEqual(TEXT("obstacle destroys projectile"),Game->GetShotCount(),0);

    Game->Enemies[0].Actor->SetActorLocation(FVector(-774,0,90));
    Game->Tick(0.1f);
    TestTrue(TEXT("reaching the defence ends the level"),Game->IsFinished() && !Game->IsWon());
    TestTrue(TEXT("failure explains the defence breach"),Game->GetResultText().Contains(TEXT("防线")));

    const TArray<FString> TierTestOriginalPlan=Game->WaveComposition;
    Game->WaveComposition={TEXT("1,1,0"),TEXT("0,1,1")};
    Game->StartLevel(false,1);
    TestEqual(TEXT("configured wave total is applied to game mode"),Game->Rules.GetTotalCount(),4);
    Game->WaveComposition=TierTestOriginalPlan;

    Game->StartLevel(false,1);
    Game->UpdateExpression(TEXT("0"),TEXT(""));
    const int32 WaveCounts[]={2,3,5,8,12};
    int32 ExpectedKills=0;
    for (int32 Wave=1;Wave<=5;++Wave)
    {
        for (int I=0;I<400 && Game->Rules.GetPendingCount()>0;++I) Game->Tick(0.1f);
        if (!TestEqual(TEXT("all enemies in the wave can spawn"),Game->Rules.GetPendingCount(),0))
        { Game->ReturnToMenu(); return false; }
        TestEqual(TEXT("wave has the expected living enemies"),Game->Rules.GetAliveCount(),WaveCounts[Wave-1]);
        for (FFunctionEnemy& Enemy:Game->Enemies)
            if (Enemy.Actor.IsValid()) Enemy.Actor->SetActorLocation(FVector(-400,0,90));
        for (int Attack=0;Attack<3 && Game->IsPlaying() && Game->Rules.GetWave()==Wave;++Attack)
        {
            TestTrue(TEXT("wave can be attacked"),Game->CanFire());
            Game->Fire();
            RunShot();
        }
        ExpectedKills+=WaveCounts[Wave-1];
        TestEqual(TEXT("wave deaths are counted once"),Game->Rules.GetKilledCount(),ExpectedKills);
        if (Wave<5) TestEqual(TEXT("the next wave starts after the previous one"),Game->Rules.GetWave(),Wave+1);
    }
    TestTrue(TEXT("defeating all thirty enemies wins the level"),Game->IsWon());
    UFunctionGameWidget* GameWidget=Game->GetGameWidget();
    if (TestNotNull(TEXT("victory widget exists"),GameWidget))
    {
        GameWidget->Refresh();
        if (TestNotNull(TEXT("next-level button exists"),GameWidget->NextButton.Get()))
        {
            TestTrue(TEXT("next-level button is enabled after victory"),GameWidget->NextButton->GetIsEnabled());
            GameWidget->NextButton->OnClicked.Broadcast();
        }
    }
    TestTrue(TEXT("next level starts in the playing state"),Game->IsPlaying());
    TestEqual(TEXT("next level resets enemy count"),Game->Rules.GetRemainingCount(),30);
    TestEqual(TEXT("next level resets mistakes"),Game->GetMistakesRemaining(),3);
    TestEqual(TEXT("next level resets wave"),Game->Rules.GetWave(),1);

    const TArray<FString> OriginalPlan=Game->WaveComposition;
    Game->WaveComposition={TEXT("1,0,1")};
    Game->StartLevel(false,1);
    Game->UpdateExpression(TEXT("0"),TEXT(""));
    for (int I=0;I<6;++I) Game->Tick(0.1f);
    FFunctionEnemy* ThreeLevelEnemy=Game->Enemies.FindByPredicate([](const FFunctionEnemy& Enemy) { return Enemy.Health==3; });
    FFunctionEnemy* OneLevelEnemy=Game->Enemies.FindByPredicate([](const FFunctionEnemy& Enemy) { return Enemy.Health==1; });
    if (TestTrue(TEXT("three-level and one-level enemies spawn"),ThreeLevelEnemy && OneLevelEnemy))
    {
        const int64 ThreeLevelId=ThreeLevelEnemy->Id;
        TWeakObjectPtr<ACharacter> ThreeLevelActor=ThreeLevelEnemy->Actor;
        ThreeLevelEnemy->Actor->SetActorLocation(FVector(-650,0,90));
        OneLevelEnemy->Actor->SetActorLocation(FVector(-350,1000,90));
        TestTrue(TEXT("level three enemy starts red"),EnemyTint(ThreeLevelEnemy->Actor.Get()).Equals(FLinearColor::Red,0.01f));

        Game->Fire();
        RunShot();
        ThreeLevelEnemy=Game->Enemies.FindByPredicate([ThreeLevelId](const FFunctionEnemy& Enemy) { return Enemy.Id==ThreeLevelId; });
        TestTrue(TEXT("level three drops to level two after one hit"),ThreeLevelEnemy && ThreeLevelEnemy->Health==2);
        if (ThreeLevelEnemy) TestTrue(TEXT("level two state is yellow"),EnemyTint(ThreeLevelEnemy->Actor.Get()).Equals(FLinearColor::Yellow,0.01f));

        Game->Fire();
        RunShot();
        ThreeLevelEnemy=Game->Enemies.FindByPredicate([ThreeLevelId](const FFunctionEnemy& Enemy) { return Enemy.Id==ThreeLevelId; });
        TestTrue(TEXT("level two drops to level one after another hit"),ThreeLevelEnemy && ThreeLevelEnemy->Health==1);
        if (ThreeLevelEnemy) TestTrue(TEXT("level one state is green"),EnemyTint(ThreeLevelEnemy->Actor.Get()).Equals(FLinearColor::Green,0.01f));

        Game->Fire();
        RunShot();
        TestFalse(TEXT("level one enemy dies on the next hit"),ThreeLevelActor.IsValid());
        TestEqual(TEXT("three hits count one level three death"),Game->Rules.GetKilledCount(),1);
    }
    Game->WaveComposition=OriginalPlan;
    Game->ReturnToMenu();
    return true;
}

#endif
