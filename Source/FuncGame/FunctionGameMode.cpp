#include "FunctionGameMode.h"
#include "FunctionGameWidget.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/DefaultValueHelper.h"
#include "UObject/ConstructorHelpers.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double PlayerX=-850.0;
    constexpr double UnitsPerMath=100.0;
    constexpr double ShotHeight=85.0;
    constexpr double ShotRadius=14.0;
    constexpr double EnemyRadius=42.0;
    bool SegmentBox(const FVector2D& Start, const FVector2D& End, const FFunctionObstacle& Box, double& OutTime)
    {
        const FVector2D D=End-Start;
        double Enter=0.0, Exit=1.0;
        const double Starts[]={Start.X,Start.Y};
        const double Deltas[]={D.X,D.Y};
        const double Min[]={Box.Min.X-ShotRadius,Box.Min.Y-ShotRadius};
        const double Max[]={Box.Max.X+ShotRadius,Box.Max.Y+ShotRadius};
        for (int I=0; I<2; ++I)
        {
            if (FMath::Abs(Deltas[I])<1.e-10)
            {
                if (Starts[I]<Min[I] || Starts[I]>Max[I]) return false;
                continue;
            }
            double A=(Min[I]-Starts[I])/Deltas[I], B=(Max[I]-Starts[I])/Deltas[I];
            if (A>B) Swap(A,B);
            Enter=FMath::Max(Enter,A);
            Exit=FMath::Min(Exit,B);
            if (Enter>Exit) return false;
        }
        OutTime=Enter;
        return Enter<=1.0 && Exit>=0.0;
    }

    bool SegmentCircle(const FVector2D& Start, const FVector2D& End, const FVector2D& Center, double Radius, double& OutTime)
    {
        const FVector2D D=End-Start, M=Start-Center;
        const double A=D.SquaredLength(), C=M.SquaredLength()-Radius*Radius;
        if (C<=0.0) { OutTime=0.0; return true; }
        if (A<1.e-12) return false;
        const double B=2.0*FVector2D::DotProduct(M,D), Discriminant=B*B-4.0*A*C;
        if (Discriminant<0.0) return false;
        const double T=(-B-FMath::Sqrt(Discriminant))/(2.0*A);
        if (T<0.0 || T>1.0) return false;
        OutTime=T;
        return true;
    }
}

AFunctionGameMode::AFunctionGameMode()
{
    PrimaryActorTick.bCanEverTick=true;
    DefaultPawnClass=nullptr;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
    static ConstructorHelpers::FClassFinder<UAnimInstance> Anim(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/FuncGame/Materials/M_GameTint.M_GameTint"));
    CubeMesh=Cube.Object;
    SphereMesh=Sphere.Object;
    EnemyMesh=Mesh.Object;
    EnemyAnimation=Anim.Class;
    TintMaterial=Material.Object;
}

void AFunctionGameMode::StartPlay()
{
    Super::StartPlay();
    CreateArena();
    if (APlayerController* PC=GetWorld()->GetFirstPlayerController())
    {
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (ACameraActor* Camera=GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),FVector(0,0,1800),FRotator(-90,-90,0),Spawn))
        {
            Camera->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic);
            Camera->GetCameraComponent()->OrthoWidth=2300.0f;
            PC->SetViewTarget(Camera);
        }
        PC->bShowMouseCursor=true;
        PC->SetInputMode(FInputModeGameAndUI());
        Widget=CreateWidget<UFunctionGameWidget>(PC,UFunctionGameWidget::StaticClass());
        if (Widget)
        {
            Widget->AddToViewport();
            Widget->SetGame(this);
        }
    }
    UpdateExpression(TEXT("x"),TEXT(""));
    RefreshWidget();
}

void AFunctionGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsPlaying() || bHelpOpen || bPaused) return;
    const float Step=FMath::Clamp(DeltaSeconds,0.0f,0.1f);
    SpawnTimer-=Step;
    if (Rules.GetPendingCount()>0 && SpawnTimer<=0.0f)
    {
        TrySpawnEnemy();
        SpawnTimer=0.20f;
    }
    UpdateEnemies(Step);
    if (IsPlaying()) UpdateShots(Step);
    RefreshWidget();
}

AActor* AFunctionGameMode::SpawnShape(UStaticMesh* Mesh,const FVector& Location,const FVector& Scale,const FLinearColor& Color)
{
    if (!Mesh) return nullptr;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Actor=GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),Location,FRotator::ZeroRotator,Spawn);
    if (!Actor) return nullptr;
    UStaticMeshComponent* Component=NewObject<UStaticMeshComponent>(Actor);
    Actor->SetRootComponent(Component);
    Component->SetStaticMesh(Mesh);
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->RegisterComponent();
    Actor->SetActorLocation(Location);
    Component->SetWorldScale3D(Scale);
    if (TintMaterial)
    {
        UMaterialInstanceDynamic* Dynamic=UMaterialInstanceDynamic::Create(TintMaterial,Component);
        Dynamic->SetVectorParameterValue(TEXT("Tint"),Color);
        Component->SetMaterial(0,Dynamic);
    }
    return Actor;
}

void AFunctionGameMode::CreateArena()
{
    ArenaActors.Add(SpawnShape(CubeMesh,FVector(0,0,-12),FVector(23,14,0.20),FLinearColor(0.035f,0.06f,0.10f)));
    ArenaActors.Add(SpawnShape(CubeMesh,FVector(PlayerX,0,55),FVector(0.75,0.75,1.1),FLinearColor(0.1f,0.75f,1.0f)));
    const FFunctionObstacle Layout[]={
        {FVector2D(-220,-340),FVector2D(-90,-175)},
        {FVector2D(150,150),FVector2D(280,315)},
        {FVector2D(500,-80),FVector2D(600,80)}
    };
    for (const FFunctionObstacle& Box:Layout)
    {
        Obstacles.Add(Box);
        const FVector2D Center=(Box.Min+Box.Max)*0.5;
        const FVector2D Size=Box.Max-Box.Min;
        ArenaActors.Add(SpawnShape(CubeMesh,FVector(Center.X,Center.Y,75),FVector(Size.X/100.0,Size.Y/100.0,1.5),FLinearColor(0.8f,0.25f,0.08f)));
    }
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if (AActor* PreviewActor=GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),FVector::ZeroVector,FRotator::ZeroRotator,Spawn))
    {
        UInstancedStaticMeshComponent* Mesh=NewObject<UInstancedStaticMeshComponent>(PreviewActor);
        PreviewActor->SetRootComponent(Mesh);
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(CubeMesh);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->RegisterComponent();
        Mesh->SetVisibility(false);
        if (TintMaterial)
        {
            UMaterialInstanceDynamic* Dynamic=UMaterialInstanceDynamic::Create(TintMaterial,Mesh);
            Dynamic->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1.0f,0.65f,0.05f));
            Mesh->SetMaterial(0,Dynamic);
        }
        PreviewMesh=Mesh;
        ArenaActors.Add(PreviewActor);
    }
    GetWorld()->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(),FVector(0,0,500),FRotator(-55,-35,0));
}

void AFunctionGameMode::StartLevel(bool bHard,int32 InLevel)
{
    ClearCombatants();
    FuncGameRules::Session NewRules;
    std::vector<FuncGameRules::Wave> Plan;
    bool bValidPlan=true;
    for (const FString& Entry:WaveComposition)
    {
        TArray<FString> Counts;
        Entry.ParseIntoArray(Counts,TEXT(","),false);
        FuncGameRules::Wave Row{};
        if (Counts.Num()!=3) { bValidPlan=false; break; }
        for (int32 I=0;I<3;++I)
            if (!FDefaultValueHelper::ParseInt(Counts[I].TrimStartAndEnd(),Row[I])) { bValidPlan=false; break; }
        if (!bValidPlan) break;
        Plan.push_back(Row);
    }
    if (!bValidPlan || !NewRules.SetWavePlan(Plan))
        UE_LOG(LogTemp,Warning,TEXT("Invalid FuncGame WaveComposition; using the default 30-enemy plan."));
    Rules=MoveTemp(NewRules);
    Rules.Start(bHard ? FuncGameRules::Difficulty::Hard : FuncGameRules::Difficulty::Easy,InLevel);
    Random.Initialize(104729+InLevel);
    bHelpOpen=false;
    bPaused=false;
    FailureReason=EFailureReason::None;
    SpawnTimer=0.0f;
    RefreshWidget();
}

void AFunctionGameMode::ReturnToMenu()
{
    ClearCombatants();
    Rules=FuncGameRules::Session{};
    bHelpOpen=false;
    bPaused=false;
    FailureReason=EFailureReason::None;
    RefreshWidget();
}

void AFunctionGameMode::SetHelpOpen(bool bOpen)
{
    bHelpOpen=bOpen;
    RefreshWidget();
}

void AFunctionGameMode::SetPaused(bool bInPaused)
{
    if (!IsPlaying()) return;
    bPaused=bInPaused;
    RefreshWidget();
}

void AFunctionGameMode::ClearCombatants()
{
    for (FFunctionEnemy& Enemy:Enemies) if (Enemy.Actor.IsValid()) Enemy.Actor->Destroy();
    for (FFunctionShot& Shot:Shots)
    {
        if (Shot.Ball.IsValid()) Shot.Ball->Destroy();
        if (Shot.TrailActor.IsValid()) Shot.TrailActor->Destroy();
    }
    Enemies.Empty();
    Shots.Empty();
}

bool AFunctionGameMode::UpdateExpression(const FString& Text,const FString& ManualStart)
{
    bExpressionReady=false;
    if (PreviewMesh.IsValid()) PreviewMesh->ClearInstances();
    FuncMath::Error Error;
    FTCHARToUTF8 Utf8(*Text);
    const std::string Source(Utf8.Get(),Utf8.Length());
    FuncMath::ExpressionLimits Limits;
    Limits.MaxLength=FMath::Clamp(ExpressionMaxBytes,1,4096);
    Limits.MaxNodes=FMath::Clamp(ExpressionMaxNodes,1,1024);
    Limits.MaxDepth=FMath::Clamp(ExpressionMaxDepth,8,512);
    Limits.EvaluationBudget=FMath::Clamp(EvaluationBudget,1,4096);
    if (!FuncMath::Expression::Compile(Source,CompiledExpression,Error,Limits))
    {
        ExpressionStatus=FString::Printf(TEXT("%s（位置 %d）"),UTF8_TO_TCHAR(Error.Message.c_str()),static_cast<int32>(Error.Position+1));
        RefreshWidget();
        return false;
    }
    FuncMath::TrajectoryOptions Options;
    Options.MaxPoints=FMath::Clamp(TrajectoryMaxPoints,2,32768);
    Options.MaxStartMagnitude=FMath::Clamp(AutoStartSearchRange,0.0f,8.0f);
    if (!ManualStart.TrimStartAndEnd().IsEmpty())
    {
        double Value=0.0;
        if (!FDefaultValueHelper::ParseDouble(ManualStart,Value) || !std::isfinite(Value))
        {
            ExpressionStatus=TEXT("起点 x0 必须是有限数字");
            RefreshWidget();
            return false;
        }
        Options.UseManualStart=true;
        Options.ManualStart=Value;
    }
    if (!FuncMath::BuildTrajectory(CompiledExpression,Options,PreviewTrajectory,Error))
    {
        ExpressionStatus=UTF8_TO_TCHAR(Error.Message.c_str());
        RefreshWidget();
        return false;
    }
    bExpressionReady=true;
    RebuildPreview();
    ExpressionStatus=FString::Printf(TEXT("可发射 · x0 = %.3g · 轨迹已平移至发射点%s"),PreviewTrajectory.StartX,
        PreviewTrajectory.StoppedEarly ? TEXT(" · 将在断点或精度限制处停止") : TEXT(""));
    RefreshWidget();
    return true;
}

bool AFunctionGameMode::CanFire() const
{
    return IsPlaying() && !bHelpOpen && !bPaused && bExpressionReady && Shots.Num()<FMath::Clamp(MaximumProjectiles,1,64);
}

FString AFunctionGameMode::GetExpressionStatus() const
{
    if (bHelpOpen) return TEXT("查看帮助时已暂停");
    if (bPaused) return FString::Printf(TEXT("已暂停，可调整函数；继续后发射 · %s"),*ExpressionStatus);
    if (IsPlaying() && Shots.Num()>=FMath::Clamp(MaximumProjectiles,1,64)) return TEXT("当前子弹数量已达上限，请稍候");
    return ExpressionStatus;
}

FVector AFunctionGameMode::PointToWorld(const FuncMath::TrajectoryPoint& Point) const
{
    return FVector(PlayerX+Point.Progress*UnitsPerMath,-Point.Height*UnitsPerMath,ShotHeight);
}

void AFunctionGameMode::RebuildPreview()
{
    if (!PreviewMesh.IsValid()) return;
    PreviewMesh->ClearInstances();
    for (size_t I=1;I<PreviewTrajectory.Points.size();++I)
    {
        const FVector From=PointToWorld(PreviewTrajectory.Points[I-1]);
        FVector To=PointToWorld(PreviewTrajectory.Points[I]);
        double Stop=1.0;
        bool bBlocked=false;
        for (const FFunctionObstacle& Box:Obstacles)
        {
            double Time=0.0;
            if (SegmentBox(FVector2D(From.X,From.Y),FVector2D(To.X,To.Y),Box,Time) && Time<=Stop)
            { Stop=Time; bBlocked=true; }
        }
        To=FMath::Lerp(From,To,Stop);
        const FVector Delta=To-From;
        if (Delta.SizeSquared2D()>0.01 && !((From.Y>800 && To.Y>800) || (From.Y< -800 && To.Y< -800)))
        {
            const FRotator Rotation(0.0,FMath::RadiansToDegrees(FMath::Atan2(Delta.Y,Delta.X)),0.0);
            FVector Center=(From+To)*0.5;
            Center.Z+=4.0;
            PreviewMesh->AddInstance(FTransform(Rotation,Center,FVector(Delta.Size2D()/100.0,0.055,0.025)),true);
        }
        if (bBlocked) break;
    }
}

void AFunctionGameMode::Fire()
{
    if (!CanFire()) return;
    FFunctionShot Shot;
    Shot.Id=NextShotId++;
    Shot.Points=PreviewTrajectory.Points;
    Shot.Ball=SpawnShape(SphereMesh,PointToWorld(Shot.Points.front()),FVector(0.28),FLinearColor(0.2f,0.85f,1.0f));
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* TrailActor=GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
    if (!Shot.Ball.IsValid() || !TrailActor)
    {
        if (Shot.Ball.IsValid()) Shot.Ball->Destroy();
        if (TrailActor) TrailActor->Destroy();
        return;
    }
    UInstancedStaticMeshComponent* Trail=NewObject<UInstancedStaticMeshComponent>(TrailActor);
    TrailActor->SetRootComponent(Trail);
    Trail->SetMobility(EComponentMobility::Movable);
    Trail->SetStaticMesh(CubeMesh);
    Trail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Trail->RegisterComponent();
    if (TintMaterial)
    {
        UMaterialInstanceDynamic* Dynamic=UMaterialInstanceDynamic::Create(TintMaterial,Trail);
        Dynamic->SetVectorParameterValue(TEXT("Tint"),FLinearColor(0.08f,0.7f,0.95f));
        Trail->SetMaterial(0,Dynamic);
    }
    Shot.TrailActor=TrailActor;
    Shot.Trail=Trail;
    Shots.Add(MoveTemp(Shot));
    RefreshWidget();
}

void AFunctionGameMode::AddTrail(FFunctionShot& Shot,const FVector& From,const FVector& To)
{
    if (!Shot.Trail.IsValid()) return;
    const FVector Delta=To-From;
    if (Delta.SizeSquared2D()<0.01) return;
    const FRotator Rotation(0.0,FMath::RadiansToDegrees(FMath::Atan2(Delta.Y,Delta.X)),0.0);
    Shot.Trail->AddInstance(FTransform(Rotation,(From+To)*0.5,FVector(Delta.Size2D()/100.0,0.035,0.035)),true);
}

bool AFunctionGameMode::TrySpawnEnemy()
{
    if (!EnemyMesh || Rules.GetPendingCount()==0) return false;
    int32 Width=1280,Height=720;
    if (APlayerController* PC=GetWorld()->GetFirstPlayerController()) PC->GetViewportSize(Width,Height);
    const float Aspect=Height>0 ? static_cast<float>(Width)/Height : 16.0f/9.0f;
    const float YLimit=FMath::Clamp(1150.0f/FMath::Max(Aspect,0.5f)-190.0f,100.0f,420.0f);
    FVector Position;
    bool bFound=false;
    for (int Attempt=0; Attempt<80 && !bFound; ++Attempt)
    {
        Position=FVector(Random.FRandRange(580,950),Random.FRandRange(-YLimit,YLimit),90);
        bFound=true;
        for (const FFunctionObstacle& Box:Obstacles)
            if (Position.X>=Box.Min.X-EnemyRadius && Position.X<=Box.Max.X+EnemyRadius &&
                Position.Y>=Box.Min.Y-EnemyRadius && Position.Y<=Box.Max.Y+EnemyRadius) { bFound=false; break; }
        if (!bFound) continue;
        for (const FFunctionEnemy& Enemy:Enemies)
            if (Enemy.Actor.IsValid() && FVector::DistSquared2D(Position,Enemy.Actor->GetActorLocation())<FMath::Square(2.0*EnemyRadius+10.0))
            { bFound=false; break; }
    }
    if (!bFound) return false;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ACharacter* Enemy=GetWorld()->SpawnActor<ACharacter>(ACharacter::StaticClass(),Position,FRotator(0,180,0),Spawn);
    if (!Enemy) return false;
    Enemy->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Enemy->GetCharacterMovement()->SetMovementMode(MOVE_None);
    Enemy->GetCharacterMovement()->MaxWalkSpeed=FMath::Clamp(EnemySpeedCmPerSecond,1.0f,1000.0f);
    Enemy->GetMesh()->SetSkeletalMesh(EnemyMesh);
    Enemy->GetMesh()->SetRelativeLocation(FVector(0,0,-89));
    Enemy->GetMesh()->SetRelativeRotation(FRotator(0,270,0));
    if (EnemyAnimation) Enemy->GetMesh()->SetAnimInstanceClass(EnemyAnimation);
    const int32 Level=Rules.GetNextEnemyLevel();
    const FLinearColor Colors[]={FLinearColor::Green,FLinearColor::Yellow,FLinearColor::Red};
    if (TintMaterial)
        for (int32 Slot=0;Slot<Enemy->GetMesh()->GetNumMaterials();++Slot)
        {
            UMaterialInstanceDynamic* Dynamic=UMaterialInstanceDynamic::Create(TintMaterial,Enemy->GetMesh());
            Dynamic->SetVectorParameterValue(TEXT("Tint"),Colors[FMath::Clamp(Level-1,0,2)]);
            Enemy->GetMesh()->SetMaterial(Slot,Dynamic);
        }
    const int64 Id=NextEnemyId++;
    if (!Rules.RegisterSpawn(Id)) { Enemy->Destroy(); return false; }
    FFunctionEnemy Data;
    Data.Actor=Enemy; Data.Id=Id; Data.Health=Level;
    Enemies.Add(MoveTemp(Data));
    return true;
}

void AFunctionGameMode::UpdateEnemies(float DeltaSeconds)
{
    for (FFunctionEnemy& Enemy:Enemies)
    {
        if (!Enemy.Actor.IsValid()) continue;
        ACharacter* Character=Enemy.Actor.Get();
        const float Speed=FMath::Clamp(EnemySpeedCmPerSecond,1.0f,1000.0f);
        Character->SetActorLocation(Character->GetActorLocation()+FVector(-Speed*DeltaSeconds,0,0));
        Character->GetCharacterMovement()->Velocity=FVector(-Speed,0,0);
        if (Character->GetActorLocation().X<=PlayerX+75.0)
        {
            Rules.EnemyReachedDefence();
            FailureReason=EFailureReason::EnemyAtDefence;
            break;
        }
    }
}

void AFunctionGameMode::UpdateShots(float DeltaSeconds)
{
    struct FHit { double Time; int32 EnemyIndex; bool bObstacle; };
    for (int32 ShotIndex=Shots.Num()-1; ShotIndex>=0 && IsPlaying(); --ShotIndex)
    {
        FFunctionShot& Shot=Shots[ShotIndex];
        if (!Shot.Ball.IsValid() || Shot.Points.size()<2) { Shots.RemoveAt(ShotIndex); continue; }
        const double Lifetime=FMath::Clamp(ProjectileLifetimeSeconds,0.1f,120.0f);
        const double Elapsed=FMath::Min(static_cast<double>(DeltaSeconds),FMath::Max(0.0,Lifetime-Shot.AgeSeconds));
        Shot.AgeSeconds+=Elapsed;
        const double Speed=FMath::Clamp(ProjectileMathUnitsPerSecond,0.01f,100.0f);
        const double Target=FMath::Min(Shot.Progress+Speed*Elapsed,Shot.Points.back().Progress);
        bool bFinished=false;
        while (Shot.Progress<Target-1.e-9 && !bFinished && IsPlaying())
        {
            while (Shot.Segment+1<static_cast<int32>(Shot.Points.size()) && Shot.Points[Shot.Segment+1].Progress<=Shot.Progress+1.e-9) ++Shot.Segment;
            if (Shot.Segment+1>=static_cast<int32>(Shot.Points.size())) { bFinished=true; break; }
            const auto& A=Shot.Points[Shot.Segment];
            const auto& B=Shot.Points[Shot.Segment+1];
            const double EndProgress=FMath::Min(Target,B.Progress);
            auto At=[&](double S) { const double Alpha=(S-A.Progress)/(B.Progress-A.Progress); return PointToWorld({S,FMath::Lerp(A.Height,B.Height,Alpha)}); };
            const FVector Start=At(Shot.Progress), End=At(EndProgress);
            TArray<FHit> Hits;
            for (const FFunctionObstacle& Box:Obstacles)
            {
                double Time=0;
                if (SegmentBox(FVector2D(Start.X,Start.Y),FVector2D(End.X,End.Y),Box,Time)) Hits.Add({Time,INDEX_NONE,true});
            }
            for (int32 I=0;I<Enemies.Num();++I)
            {
                FFunctionEnemy& Enemy=Enemies[I];
                if (!Enemy.Actor.IsValid() || Shot.HitEnemies.Contains(Enemy.Id)) continue;
                const FVector P=Enemy.Actor->GetActorLocation();
                double Time=0;
                if (SegmentCircle(FVector2D(Start.X,Start.Y),FVector2D(End.X,End.Y),FVector2D(P.X,P.Y),EnemyRadius+ShotRadius,Time)) Hits.Add({Time,I,false});
            }
            Hits.Sort([](const FHit& L,const FHit& R) { return L.Time==R.Time ? L.bObstacle && !R.bObstacle : L.Time<R.Time; });
            double Stop=1.0;
            for (const FHit& Hit:Hits)
            {
                if (Hit.bObstacle)
                {
                    Stop=Hit.Time;
                    Rules.RegisterObstacleHit(Shot.Id);
                    if (!IsPlaying()) FailureReason=EFailureReason::ObstacleMistakes;
                    bFinished=true;
                    break;
                }
                FFunctionEnemy& Enemy=Enemies[Hit.EnemyIndex];
                if (!Enemy.Actor.IsValid() || Shot.HitEnemies.Contains(Enemy.Id)) continue;
                Shot.HitEnemies.Add(Enemy.Id);
                if (--Enemy.Health<=0)
                {
                    Enemy.Actor->Destroy();
                    Enemy.Actor.Reset();
                    Rules.RegisterDeath(Enemy.Id);
                    if (!IsPlaying()) { Stop=Hit.Time; bFinished=true; break; }
                }
            }
            const FVector ActualEnd=FMath::Lerp(Start,End,Stop);
            AddTrail(Shot,Start,ActualEnd);
            Shot.Ball->SetActorLocation(ActualEnd);
            Shot.Progress=EndProgress;
        }
        if (Shot.Progress>=Shot.Points.back().Progress-1.e-9 || Shot.AgeSeconds>=Lifetime) bFinished=true;
        if (bFinished)
        {
            if (Shot.Ball.IsValid()) Shot.Ball->Destroy();
            if (Shot.TrailActor.IsValid()) Shot.TrailActor->Destroy();
            Shots.RemoveAt(ShotIndex);
        }
    }
    Enemies.RemoveAll([](const FFunctionEnemy& Enemy) { return !Enemy.Actor.IsValid(); });
}

FString AFunctionGameMode::GetHudText() const
{
    return FString::Printf(TEXT("%s  ·  关卡 %d  ·  第 %d 波  ·  剩余敌人 %d  ·  失误 %d/3%s"),
        IsHard()?TEXT("困难"):TEXT("简单"),Rules.GetLevel(),Rules.GetWave(),Rules.GetRemainingCount(),Rules.GetMistakesRemaining(),
        IsHard()?TEXT(""):TEXT("（简单模式不扣除）"));
}

FString AFunctionGameMode::GetResultText() const
{
    if (IsWon()) return FString::Printf(TEXT("关卡胜利！全部 %d 名敌人已被击败。"),Rules.GetTotalCount());
    if (FailureReason==EFailureReason::EnemyAtDefence) return TEXT("挑战失败：敌人抵达防线。");
    if (FailureReason==EFailureReason::ObstacleMistakes) return TEXT("挑战失败：撞击障碍物，失误次数已用尽。");
    return TEXT("挑战失败。");
}

void AFunctionGameMode::RefreshWidget()
{
    if (PreviewMesh.IsValid()) PreviewMesh->SetVisibility(IsPlaying() && bExpressionReady);
    if (Widget) Widget->Refresh();
}
