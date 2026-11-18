#include "../OBMachineEnemy.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AIController.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBMachineRulesTest,
    "OrbitBreakout.Level2.Machines.ShieldsAndCleanup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOBMachineRulesTest::RunTest(const FString& Parameters)
{
    // No BeginPlay, mission instance, save slot or player controller is involved.
    if (!GEngine) return false;
    FWorldContext& Context=GEngine->CreateNewWorldContext(EWorldType::Game);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,
        MakeUniqueObjectName(GetTransientPackage(),UWorld::StaticClass(),TEXT("OBMachineRules")),GetTransientPackage());
    if (!TestNotNull(TEXT("Isolated collision world"),World)) return false;
    Context.SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const FTransform Transform(FRotator::ZeroRotator,FVector(0,0,110));
    AOBMachineEnemy* Machine=World->SpawnActorDeferred<AOBMachineEnemy>(AOBMachineEnemy::StaticClass(),Transform,nullptr,nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Machine) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
    Machine->AutoPossessAI=EAutoPossessAI::Disabled;
    Machine->Kind=EOBMachineKind::Bulwark;
    Machine->FinishSpawning(Transform);
    TestEqual(TEXT("Bulwark health"),Machine->MaxHealth,260.f);
    TestEqual(TEXT("Front is armored"),Machine->GetDamageMultiplier(FVector(100,0,110)),.2f);
    TestEqual(TEXT("Flank is vulnerable"),Machine->GetDamageMultiplier(FVector(0,100,110)),1.f);
    TestEqual(TEXT("Rear is vulnerable"),Machine->GetDamageMultiplier(FVector(-100,0,110)),1.f);
    Machine->CoreExposedRemaining=2.f;
    TestEqual(TEXT("Recovery exposes frontal core"),Machine->GetDamageMultiplier(FVector(100,0,110)),1.f);
    Machine->CoreExposedRemaining=0;
    FPointDamageEvent FrontHit;
    FrontHit.ShotDirection=FVector(-1,0,0);
    TestEqual(TEXT("Dormant reinforcements cannot take damage"),Machine->TakeDamage(20.f,FrontHit,nullptr,nullptr),0.f);
    Machine->bActive=true; Machine->State=EOBMachineState::Idle;
    TestEqual(TEXT("Actual frontal hit applies reduction"),Machine->TakeDamage(20.f,FrontHit,nullptr,nullptr),4.f);
    FPointDamageEvent SideHit;
    SideHit.ShotDirection=FVector(0,-1,0);
    TestEqual(TEXT("Actual flank hit applies full damage"),Machine->TakeDamage(20.f,SideHit,nullptr,nullptr),20.f);
    Machine->Kind=EOBMachineKind::Watcher;
    Machine->ConfigureVisuals();
    TestEqual(TEXT("Watcher health"),Machine->MaxHealth,130.f);
    TestEqual(TEXT("Watcher has no hidden shield"),Machine->GetDamageMultiplier(FVector(100,0,110)),1.f);
    Machine->Kind=EOBMachineKind::Cutter;
    Machine->ConfigureVisuals();
    TestEqual(TEXT("Cutter health"),Machine->MaxHealth,70.f);
    TestTrue(TEXT("Cutter standing capsule fits the authored navigation envelope"),
        Machine->GetCapsuleComponent()->GetUnscaledCapsuleRadius()<=44.f &&
        Machine->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()>=85.f &&
        Machine->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()<=112.f);
    Machine->Kind=EOBMachineKind::Warden;
    Machine->ConfigureVisuals();
    TestEqual(TEXT("Warden health"),Machine->MaxHealth,650.f);
    Machine->StableId=TEXT("TEST_WARDEN");
    Machine->SetActorHiddenInGame(false);
    Machine->SetActorEnableCollision(true);
    Machine->bHasSlot=true;
    Machine->State=EOBMachineState::PulseWarning;
    Machine->SetWarningVisible(true);
    Machine->Die(false);
    TestTrue(TEXT("Death is immediate"),Machine->IsDead());
    TestFalse(TEXT("Dead machine cannot block movement or shots"),Machine->GetActorEnableCollision());
    TestFalse(TEXT("Death releases attack ownership"),Machine->bHasSlot);
    TestFalse(TEXT("Death cancels pulse telegraph"),Machine->WarningSegments[0]->IsVisible());
    TestFalse(TEXT("Shutdown pose remains briefly visible"),Machine->IsHidden());
    Machine->Tick(1.3f);
    TestTrue(TEXT("Body disappears within two seconds"),Machine->IsHidden());
    TestFalse(TEXT("Cleanup disables actor tick"),Machine->IsActorTickEnabled());
    TestEqual(TEXT("Stable identity survives cleanup"),Machine->StableId,FName(TEXT("TEST_WARDEN")));
    Machine->ResetToSpawn();
    TestFalse(TEXT("Checkpoint reset clears death"),Machine->IsDead());
    TestFalse(TEXT("Reset does not activate future encounters"),Machine->bActive);
    TestEqual(TEXT("Reset restores health"),Machine->Health,Machine->MaxHealth);
    TestTrue(TEXT("Reset hides dormant machines"),Machine->IsHidden());
    // Announced arrivals must investigate a bounded snapshot even when their
    // occluded door starts beyond ordinary sight/hearing distance. No player
    // controller, camera, navmesh, BeginPlay or save access is needed here.
    Machine->Kind=EOBMachineKind::Watcher;
    Machine->ConfigureVisuals();
    Machine->bActive=true;Machine->State=EOBMachineState::Idle;
    const FVector AlarmPosition(3500,0,110);
    Machine->EncounterId=201;
    Machine->BeginAnnouncedInvestigation(AlarmPosition);
    TestFalse(TEXT("Unannounced maintenance encounters gain no remote awareness"),Machine->bAlerted);
    Machine->EncounterId=210;
    Machine->BeginAnnouncedInvestigation(AlarmPosition);
    TestTrue(TEXT("Announced reinforcement investigates the reported area"),Machine->State==EOBMachineState::Search && Machine->bAlerted);
    TestTrue(TEXT("Alarm stores one fixed destination"),Machine->LastKnown.Equals(AlarmPosition));
    TestFalse(TEXT("Alarm does not grant line of sight or permission to fire"),Machine->bVisibleTarget);
    const float SearchAllowance=Machine->InitialInvestigationUntil-World->GetTimeSeconds();
    TestTrue(TEXT("Long entry route gets a bounded travel allowance"),SearchAllowance>Machine->AwarenessMemorySeconds && SearchAllowance<=20.f);
    Machine->ResetToSpawn();
    TestTrue(TEXT("Retry clears the initial alarm report deadline"),Machine->InitialInvestigationUntil<World->GetTimeSeconds());
    TestFalse(TEXT("Retry clears reinforcement awareness"),Machine->bAlerted);
    // Replay the measured north-cover route with the actual engine follower.
    // This uses an AI controller only, with no player/camera, navmesh or saves.
    Machine->Kind=EOBMachineKind::Cutter;
    Machine->ConfigureVisuals();
    Machine->SetActorEnableCollision(true);
    Machine->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Machine->SetActorLocation(FVector(19641.92,9845.62,10.f+Machine->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
    AAIController* AI=World->SpawnActor<AAIController>();
    if (TestNotNull(TEXT("Isolated navigation controller"),AI))
    {
        AI->Possess(Machine);
        UPathFollowingComponent* Follower=AI->GetPathFollowingComponent();
        const TArray<FVector> Route={FVector(19641.92,9845.62,10),FVector(19456,9690,10),FVector(19418,9652,10),FVector(19300,9500,10)};
        auto FollowRecordedRoute=[&](float Acceptance)
        {
            FAIMoveRequest Request(Route.Last());
            Request.SetAcceptanceRadius(Acceptance);
            Request.SetReachTestIncludesAgentRadius(true);
            FNavPathSharedPtr Path=MakeShared<FNavigationPath,ESPMode::ThreadSafe>(Route);
            Follower->RequestMove(Request,Path);
            Follower->TickComponent(.1f,LEVELTICK_All,nullptr);
        };
        FollowRecordedRoute(400.f*.72f);
        TestTrue(TEXT("Recorded wide tolerance reproduces early corner completion"),Follower->GetStatus()==EPathFollowingStatus::Idle);
        TestTrue(TEXT("Early completion remains outside Cutter attack distance"),FVector::Dist2D(Machine->GetActorLocation(),Route.Last())>400.f);
        FollowRecordedRoute(AOBMachineEnemy::ApproachAcceptanceRadius);
        TestTrue(TEXT("Approach follows the corner instead of finishing outside attack range"),Follower->GetStatus()==EPathFollowingStatus::Moving);
        TestTrue(TEXT("Approach retains the measured first corner"),Follower->GetCurrentTargetLocation().Equals(Route[1],1.f));
        Follower->AbortMove(*AI,FPathFollowingResultFlags::ForcedScript);
        AI->UnPossess();
    }
    GEngine->ShutdownWorldNetDriver(World);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

#endif
