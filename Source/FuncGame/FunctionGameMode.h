#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Gameplay/CombatRules.h"
#include "Math/FunctionExpression.h"
#include "Math/FunctionTrajectory.h"
#include "FunctionGameMode.generated.h"

class ACharacter;
class UFunctionGameWidget;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class USkeletalMesh;
class UAnimInstance;

struct FFunctionEnemy
{
    TWeakObjectPtr<ACharacter> Actor;
    int64 Id = 0;
    int32 Health = 0;
};

struct FFunctionObstacle
{
    FVector2D Min;
    FVector2D Max;
};

struct FFunctionShot
{
    TWeakObjectPtr<AActor> Ball;
    TWeakObjectPtr<AActor> TrailActor;
    TWeakObjectPtr<UInstancedStaticMeshComponent> Trail;
    std::vector<FuncMath::TrajectoryPoint> Points;
    TSet<int64> HitEnemies;
    int64 Id = 0;
    int32 Segment = 0;
    double Progress = 0.0;
    double AgeSeconds = 0.0;
};

UCLASS(Config=Game)
class FUNCGAME_API AFunctionGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AFunctionGameMode();
    virtual void StartPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    void StartLevel(bool bHard, int32 InLevel);
    void ReturnToMenu();
    void SetHelpOpen(bool bOpen);
    bool UpdateExpression(const FString& Text, const FString& ManualStart);
    void Fire();
    bool CanFire() const;
    bool IsPlaying() const { return Rules.GetState()==FuncGameRules::State::Playing; }
    bool IsHelpOpen() const { return bHelpOpen; }
    int32 GetLevel() const { return Rules.GetLevel(); }
    bool IsHard() const { return Rules.GetDifficulty()==FuncGameRules::Difficulty::Hard; }
    FString GetHudText() const;
    FString GetExpressionStatus() const;
    FString GetResultText() const;
    bool IsWon() const { return Rules.GetState()==FuncGameRules::State::Won; }
    bool IsFinished() const { return Rules.GetState()==FuncGameRules::State::Won || Rules.GetState()==FuncGameRules::State::Lost; }
    int32 GetSpawnedEnemyCount() const { return Rules.GetSpawnedCount(); }
    int32 GetMistakesRemaining() const { return Rules.GetMistakesRemaining(); }
    int32 GetShotCount() const { return Shots.Num(); }
    UFunctionGameWidget* GetGameWidget() const { return Widget; }

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FFunctionWorldCollision;
#endif
    UPROPERTY(Config, EditDefaultsOnly, Category="FuncGame|Rules")
    float EnemySpeedCmPerSecond = 50.0f;
    UPROPERTY(Config, EditDefaultsOnly, Category="FuncGame|Rules")
    float ProjectileMathUnitsPerSecond = 2.0f;
    UPROPERTY(Config, EditDefaultsOnly, Category="FuncGame|Rules")
    float ProjectileLifetimeSeconds = 12.0f;
    UPROPERTY(Config, EditDefaultsOnly, Category="FuncGame|Rules")
    int32 MaximumProjectiles = 6;
    UPROPERTY(Config, EditDefaultsOnly, Category="FuncGame|Rules")
    TArray<FString> WaveComposition = {TEXT("1,1,0"),TEXT("1,2,0"),TEXT("1,3,1"),TEXT("1,4,3"),TEXT("1,4,7")};
    UPROPERTY(Config, EditDefaultsOnly, Category="FuncGame|Math")
    int32 ExpressionMaxBytes = 512;
    UPROPERTY(Config, EditDefaultsOnly, Category="FuncGame|Math")
    int32 ExpressionMaxNodes = 256;
    UPROPERTY(Config, EditDefaultsOnly, Category="FuncGame|Math")
    int32 ExpressionMaxDepth = 512;
    UPROPERTY(Config, EditDefaultsOnly, Category="FuncGame|Math")
    int32 EvaluationBudget = 257;
    UPROPERTY(Config, EditDefaultsOnly, Category="FuncGame|Math")
    int32 TrajectoryMaxPoints = 8192;
    UPROPERTY(Config, EditDefaultsOnly, Category="FuncGame|Math")
    float AutoStartSearchRange = 8.0f;

    UPROPERTY()
    TObjectPtr<UFunctionGameWidget> Widget;
    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;
    UPROPERTY()
    TObjectPtr<UStaticMesh> SphereMesh;
    UPROPERTY()
    TObjectPtr<USkeletalMesh> EnemyMesh;
    UPROPERTY()
    TObjectPtr<UClass> EnemyAnimation;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> TintMaterial;

    FuncGameRules::Session Rules;
    FuncMath::Expression CompiledExpression;
    FuncMath::Trajectory PreviewTrajectory;
    FString ExpressionStatus;
    bool bExpressionReady = false;
    bool bHelpOpen = false;
    int64 NextEnemyId = 1;
    int64 NextShotId = 1;
    float SpawnTimer = 0.0f;
    FRandomStream Random;
    TArray<FFunctionEnemy> Enemies;
    TArray<FFunctionShot> Shots;
    TArray<FFunctionObstacle> Obstacles;
    TArray<TWeakObjectPtr<AActor>> ArenaActors;

    void CreateArena();
    void ClearCombatants();
    void UpdateEnemies(float DeltaSeconds);
    void UpdateShots(float DeltaSeconds);
    bool TrySpawnEnemy();
    void RefreshWidget();
    AActor* SpawnShape(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FLinearColor& Color);
    FVector PointToWorld(const FuncMath::TrajectoryPoint& Point) const;
    void AddTrail(FFunctionShot& Shot, const FVector& From, const FVector& To);
};
