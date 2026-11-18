#include "../OBLevel2.h"
#include "../OBMachineEnemy.h"
#include "../OBPlayerCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"

namespace OBLevel2MissionTests
{
    struct FWorldFixture
    {
        UWorld* World=nullptr;
        FWorldFixture()
        {
            if (!GEngine) return;
            FWorldContext& Context=GEngine->CreateNewWorldContext(EWorldType::Game);
            World=UWorld::CreateWorld(EWorldType::Game,false,
                MakeUniqueObjectName(GetTransientPackage(),UWorld::StaticClass(),TEXT("OBLevel2MissionRules")),GetTransientPackage());
            Context.SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
        }
        ~FWorldFixture()
        {
            if (!World) return;
            World->SetGameInstance(nullptr);
            GEngine->ShutdownWorldNetDriver(World);
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
        template<class T> T* SpawnPawn(FVector Position)
        {
            const FTransform Transform(FRotator::ZeroRotator,Position);
            T* Pawn=World->SpawnActorDeferred<T>(T::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (!Pawn) return nullptr;
            Pawn->AutoPossessAI=EAutoPossessAI::Disabled;
            Pawn->AutoPossessPlayer=EAutoReceiveInput::Disabled;
            Pawn->FinishSpawning(Transform);
            return Pawn;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBLevel2MissionRulesTest,
    "OrbitBreakout.Level2.Mission.FiniteWavesRecoveryAndHazards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOBLevel2MissionRulesTest::RunTest(const FString& Parameters)
{
    // No BeginPlay, controller, viewport, game-instance Init, save API or map travel.
    OBLevel2MissionTests::FWorldFixture Fixture;
    if (!TestNotNull(TEXT("Isolated mission world"),Fixture.World)) return false;
    UWorld* World=Fixture.World;
    AOBLevel2Director* Director=World->SpawnActor<AOBLevel2Director>();
    AOBPlayerCharacter* Player=Fixture.SpawnPawn<AOBPlayerCharacter>(FVector(0,0,110));
    if (!TestNotNull(TEXT("Director"),Director) || !TestNotNull(TEXT("Player"),Player)) return false;
    Director->Player=Player;
    Director->bReady=true;
    Director->SafeUntil=-1.f;
    Director->Phase=EOBPhase::Corridor;
    Director->MissionState=EOBLevel2State::ActivateStabilizers;
    TestTrue(TEXT("Unpaused ready mission permits gameplay"),Director->IsGameplayActive());
    Director->bCinematic=true;
    TestFalse(TEXT("Cinematic suppresses gameplay and attack allocation"),Director->IsCombatAllowed());
    Director->bCinematic=false;

    // Build the actual 4+4+3+2 finale schedule without involving map assets.
    int32 Index=0;
    for (int32 Encounter=210;Encounter<=213;++Encounter)
    {
        const int32 Count=Encounter<=211 ? 4 : Encounter==212 ? 3 : 2;
        for (int32 N=0;N<Count;++N)
        {
            AOBMachineEnemy* Machine=Fixture.SpawnPawn<AOBMachineEnemy>(FVector(1100+Index*190,1000,110));
            if (!TestNotNull(TEXT("Finale machine"),Machine)) return false;
            Machine->EncounterId=Encounter;
            Machine->StableId=FName(*FString::Printf(TEXT("Test_%d_%d"),Encounter,N));
            Machine->SetActorEnableCollision(false);
            Director->Machines.Add(Machine);
            Director->EncounterRoster.FindOrAdd(Encounter).Add(Machine->StableId);
            ++Index;
        }
        Director->QueueEncounter(Encounter);
        Director->QueueEncounter(Encounter);
    }
    TestEqual(TEXT("Repeated scheduling queues exactly thirteen finale machines"),Director->SpawnQueue.Num(),13);
    TestEqual(TEXT("Every wave schedules once"),Director->ScheduledEncounters.Num(),4);
    TestFalse(TEXT("An unscheduled or absent encounter cannot count as cleared"),Director->IsEncounterCleared(299));
    // Only remove the cue delay; the production spawn constraints still run.
    for (auto& Pair:Director->SpawnNotBefore) Pair.Value=0.f;
    for (int32 I=0;I<20;++I) Director->PumpSpawnQueue(.31f);
    TestEqual(TEXT("Alive arena limit is five"),Director->AliveFinaleMachines(),5);
    TestEqual(TEXT("Remaining reinforcements stay queued"),Director->SpawnQueue.Num(),8);
    for (int32 I=0;I<4;++I)
        TestTrue(TEXT("First four attackers obtain slots"),Director->TryAcquireAttackSlot(Director->Machines[I]));
    TestFalse(TEXT("Fifth simultaneous attack waits"),Director->TryAcquireAttackSlot(Director->Machines[4]));
    TestTrue(TEXT("Existing attack ownership is idempotent"),Director->TryAcquireAttackSlot(Director->Machines[0]));
    Director->ReleaseAttackSlot(Director->Machines[0]);
    TestTrue(TEXT("Released slot becomes available"),Director->TryAcquireAttackSlot(Director->Machines[4]));

    AOBMachineEnemy* First=Director->Machines[0];
    First->SetResolved();
    Director->ResolveEnemy(First->EncounterId,First->StableId);
    Director->ResolveEnemy(First->EncounterId,First->StableId);
    Director->ResolveEnemy(211,TEXT("InventedEnemy"));
    TestEqual(TEXT("Duplicate and unknown deaths do not advance bookkeeping"),Director->ResolvedMachines.Num(),1);
    Director->PumpSpawnQueue(.31f);
    TestEqual(TEXT("One vacancy admits exactly one queued enemy"),Director->AliveFinaleMachines(),5);
    TestEqual(TEXT("Activated total increases once"),Director->FinaleSpawned,6);
    TestFalse(TEXT("Partially defeated encounter remains blocked"),Director->IsEncounterCleared(210));

    // Test the actual Tick transition, including frame overshoot at both thresholds.
    Director->MissionState=EOBLevel2State::TransmitSignal;
    Director->UplinkSeconds=49.7f;
    Director->Tick(1.f);
    TestEqual(TEXT("Overload clamps to fifty seconds despite frame overshoot"),Director->UplinkSeconds,50.f);
    TestTrue(TEXT("Overload pauses the uplink"),Director->IsUplinkPaused());
    Director->Tick(10.f);
    TestEqual(TEXT("Time does not advance while repair is outstanding"),Director->UplinkSeconds,50.f);
    Director->bOverloadRepaired=true;
    Director->Tick(30.f);
    TestEqual(TEXT("Repair resumes and caps transmission at seventy-five"),Director->UplinkSeconds,75.f);
    TestTrue(TEXT("Remaining hostiles prevent the rescue cinematic"),Director->MissionState==EOBLevel2State::ClearFinalHostiles && !Director->bCinematic);
    const int32 QueuedAtCompletion=Director->SpawnQueue.Num();
    Director->QueueEncounter(213);
    TestEqual(TEXT("Completion cannot schedule additional enemies"),Director->SpawnQueue.Num(),QueuedAtCompletion);
    TestTrue(TEXT("Repair does not trigger a second overload"),Director->bOverloadTriggered && Director->bOverloadRepaired && !Director->IsUplinkPaused());

    // Spatial spawn guard also runs without a PlayerController or camera fixture.
    if (!TestTrue(TEXT("Pending reinforcement remains available for spatial test"),!Director->SpawnQueue.IsEmpty())) return false;
    AOBMachineEnemy* Pending=Director->SpawnQueue.Last();
    const FVector OriginalPending=Pending->GetActorLocation();
    Pending->SetActorLocation(Player->GetActorLocation()+FVector(300,0,0));
    TestFalse(TEXT("Reinforcement cannot materialize within seven meters"),Director->CanSpawnAt(Pending));
    Pending->SetActorLocation(OriginalPending);

    // Hazard timing is deterministic and disabled by isolation/cinematics.
    Director->MissionState=EOBLevel2State::RestoreJunction;
    AOBLevel2Hazard* Hazard=World->SpawnActor<AOBLevel2Hazard>();
    Hazard->HazardKind=EOBLevel2HazardKind::LiveWater;
    Director->bJunctionIsolated=false;
    TestTrue(TEXT("Unisolated water is dangerous"),Hazard->IsDangerous());
    Director->bJunctionIsolated=true;
    TestFalse(TEXT("Breaker removes all electrical danger"),Hazard->IsDangerous());
    Hazard->HazardKind=EOBLevel2HazardKind::PressureVent;
    auto CheckVent=[&](float CycleTime,bool Expected,const TCHAR* Description)
    {
        Hazard->CycleOffset=CycleTime-World->GetTimeSeconds();
        TestEqual(Description,Hazard->IsDangerous(),Expected);
    };
    CheckVent(.5f,false,TEXT("One-second vent warning is safe"));
    CheckVent(1.f,true,TEXT("Vent begins at the warning boundary"));
    CheckVent(2.49f,true,TEXT("Vent stays live for one and a half seconds"));
    CheckVent(2.5f,false,TEXT("Vent stops at the safe-gap boundary"));
    CheckVent(4.4f,false,TEXT("Safe gap remains harmless"));
    CheckVent(1.5f,true,TEXT("Live interval is reproducible"));
    Director->bCinematic=true;
    TestFalse(TEXT("Cinematic suppresses vent damage"),Hazard->IsDangerous());
    Director->bCinematic=false;

    // Restore from an in-memory finale snapshot. Never call UOBGameInstance::Init
    // or SaveCheckpoint: even a development slot would be an unnecessary write.
    UOBGameInstance* GI=NewObject<UOBGameInstance>(World);
    GI->bFrontEnd=false;
    GI->MissionSave=NewObject<UOBMissionSave>(GI);
    GI->MissionSave->MissionId=TEXT("DeadSignal");GI->MissionSave->ChapterId=2;GI->MissionSave->SetCheckpointIdentity(3);
    GI->MissionSave->bLevel2IntroComplete=true;
    GI->MissionSave->bJunctionIsolated=GI->MissionSave->bJunctionDrained=true;
    GI->MissionSave->bCouplerOwned=GI->MissionSave->bCouplerInserted=GI->MissionSave->bAuxPowerRestored=true;
    GI->MissionSave->bWardenDefeated=true;
    GI->MissionSave->bArcOwned=GI->MissionSave->bHelixOwned=true;
    GI->MissionSave->ArcMagazine=24;GI->MissionSave->ReserveAmmo=72;
    GI->MissionSave->HelixMagazine=36;GI->MissionSave->HelixReserveAmmo=72;
    World->SetGameInstance(GI);
    Director->bStabilizerA=Director->bStabilizerB=true;
    Director->bDeadHandled=true;Director->bCompletionSaved=true;Director->DeathElapsed=4.f;
    Director->RestoreWorld(3);
    TestTrue(TEXT("Finale retry keeps Warden, power, coupler and junction progress"),Director->bWardenDefeated && Director->bAuxPowerRestored && Director->bCouplerInserted && Director->bJunctionDrained);
    TestTrue(TEXT("Finale retry returns to stabilizer stage"),Director->MissionState==EOBLevel2State::ActivateStabilizers);
    TestFalse(TEXT("Finale retry clears death lock"),Director->bDeadHandled);
    TestFalse(TEXT("Finale retry clears completion lock"),Director->bCompletionSaved);
    TestEqual(TEXT("Finale retry clears death timer"),Director->DeathElapsed,0.f);
    TestFalse(TEXT("Finale retry resets stabilizer A"),Director->bStabilizerA);
    TestFalse(TEXT("Finale retry resets stabilizer B"),Director->bStabilizerB);
    TestFalse(TEXT("Finale retry resets overload"),Director->bOverloadTriggered);
    TestEqual(TEXT("Finale retry resets uplink time"),Director->UplinkSeconds,0.f);
    TestEqual(TEXT("Finale retry clears spawn queue"),Director->SpawnQueue.Num(),0);
    TestEqual(TEXT("Finale retry clears attack slots"),Director->MachineAttackSlots.Num(),0);
    TestEqual(TEXT("Finale retry resets spawned count"),Director->FinaleSpawned,0);
    TestEqual(TEXT("All finale enemies dormant after retry"),Director->AliveFinaleMachines(),0);
    TestTrue(TEXT("Player returns to the finale checkpoint"),Player->GetActorLocation().Equals(Director->FinaleCheckpoint));
    TestEqual(TEXT("Retry restores health"),Player->Health,Player->MaxHealth);
    World->SetGameInstance(nullptr);

    // A generic AI controller supplies an eye direction without a local-player,
    // viewport, camera manager, or SetViewTarget call.
    AAIController* EyeController=World->SpawnActor<AAIController>();
    if (!TestNotNull(TEXT("Non-player eye controller"),EyeController)) return false;
    EyeController->Possess(Player);
    EyeController->SetControlRotation(FRotator::ZeroRotator);
    Player->SetActorLocation(FVector(0,0,110));
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Director->SafeUntil=-1.f;
    Director->CurrentZone=7;
    AOBLevel2Interactable* StabilizerA=World->SpawnActor<AOBLevel2Interactable>(FVector(170,-45,0),FRotator::ZeroRotator);
    AOBLevel2Interactable* StabilizerB=World->SpawnActor<AOBLevel2Interactable>(FVector(170,45,0),FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Stabilizer A"),StabilizerA) || !TestNotNull(TEXT("Stabilizer B"),StabilizerB)) return false;
    StabilizerA->Action=EOBLevel2Action::StabilizerA;StabilizerA->HoldSeconds=1.5f;
    StabilizerB->Action=EOBLevel2Action::StabilizerB;StabilizerB->HoldSeconds=1.5f;
    // The closely spaced fixture controls must not occlude one another.
    StabilizerA->SetActorEnableCollision(false);StabilizerB->SetActorEnableCollision(false);
    Director->Interactions.Add(StabilizerA);Director->Interactions.Add(StabilizerB);
    Player->bInteractHeld=true;
    TestTrue(TEXT("A stabilizer starts a valid hold"),Director->TryInteract(Player,StabilizerB));
    Director->UpdateHold(Player,StabilizerB,1.f);
    TestTrue(TEXT("An incomplete hold reports partial progress"),Player->InteractionProgress>0.f && Player->InteractionProgress<1.f);
    Player->LastDamageTime=World->GetTimeSeconds()+.1f;
    Director->UpdateHold(Player,StabilizerB,.2f);
    TestFalse(TEXT("Damage cancels the interaction"),Player->bInteractionActive);
    TestEqual(TEXT("Interrupted hold progress resets"),Player->InteractionProgress,0.f);
    TestFalse(TEXT("Interrupted stabilizer is not granted"),Director->bStabilizerB);
    TestTrue(TEXT("Hold can restart after damage"),Director->TryInteract(Player,StabilizerB));
    Director->UpdateHold(Player,StabilizerB,1.6f);
    TestTrue(TEXT("Stabilizer B can finish before A"),Director->bStabilizerB && !Director->bStabilizerA);
    TestEqual(TEXT("First stabilizer schedules four opening enemies"),Director->SpawnQueue.Num(),4);
    TestTrue(TEXT("Other stabilizer remains usable"),Director->TryInteract(Player,StabilizerA));
    Director->UpdateHold(Player,StabilizerA,1.6f);
    TestTrue(TEXT("Both stabilizers remain online"),Director->bStabilizerA && Director->bStabilizerB);
    TestEqual(TEXT("Second stabilizer never duplicates the opening wave"),Director->SpawnQueue.Num(),4);
    TestFalse(TEXT("Transmitter waits for opening hostiles"),Director->IsActionAvailable(EOBLevel2Action::Transmitter));
    for (AOBMachineEnemy* Machine:Director->Machines)
        if (Machine->EncounterId==210) { Machine->SetResolved(); Director->ResolveEnemy(210,Machine->StableId); }
    TestTrue(TEXT("Clearing the opening wave unlocks the transmitter"),Director->IsActionAvailable(EOBLevel2Action::Transmitter));
    Director->MissionState=EOBLevel2State::TransmitSignal;
    Director->bOverloadTriggered=true;Director->bOverloadRepaired=false;Director->UplinkSeconds=50.f;
    TestTrue(TEXT("Overloaded stabilizer begins a repair hold"),Director->TryInteract(Player,StabilizerA));
    Director->UpdateHold(Player,StabilizerA,1.6f);
    TestFalse(TEXT("Repair requires longer than initial activation"),Director->bOverloadRepaired);
    Director->UpdateHold(Player,StabilizerA,.41f);
    TestTrue(TEXT("Two-second repair resumes the uplink"),Director->bOverloadRepaired);
    TestEqual(TEXT("Repair preserves fifty seconds of progress"),Director->UplinkSeconds,50.f);
    TestTrue(TEXT("Repair schedules the final wave once"),Director->ScheduledEncounters.Contains(213));
    const int32 QueueAfterRepair=Director->SpawnQueue.Num();
    Director->CompleteAction(StabilizerA);
    TestEqual(TEXT("Repeated repair action grants no duplicate wave"),Director->SpawnQueue.Num(),QueueAfterRepair);

    // Exercise the common crash finalizer for watch and skip outcomes, with no
    // sequence player or game instance, so this cannot persist a checkpoint.
    Director->Checkpoint=0;
    Director->bCinematic=true;Director->bRescueCinematic=false;
    Director->MissionState=EOBLevel2State::CrashIntro;Director->CinematicElapsed=42.f;
    Player->Health=25.f;Player->GetCharacterMovement()->DisableMovement();
    Director->FinishCinematic();
    const FVector WatchedLocation=Player->GetActorLocation();
    const float WatchedHealth=Player->Health;
    const EOBLevel2State WatchedState=Director->MissionState;
    TestFalse(TEXT("Watched crash releases cinematic lock"),Director->IsCinematicActive());
    Director->bCinematic=true;Director->bRescueCinematic=false;
    Director->MissionState=EOBLevel2State::CrashIntro;Director->CinematicElapsed=3.f;
    Director->SetSkipHeld(true);Director->SkipHeldSeconds=1.f;
    Player->SetActorLocation(FVector(1000,1000,110));Player->Health=15.f;Player->GetCharacterMovement()->DisableMovement();
    Director->FinishCinematic();
    TestTrue(TEXT("Skip and watch restore the same player position"),Player->GetActorLocation().Equals(WatchedLocation));
    TestEqual(TEXT("Skip and watch apply the same medical reset"),Player->Health,WatchedHealth);
    TestTrue(TEXT("Skip and watch select the same mission objective"),Director->MissionState==WatchedState);
    TestFalse(TEXT("Skip clears cinematic state"),Director->IsCinematicActive());
    TestEqual(TEXT("Skip clears the held button progress"),Director->GetSkipProgress(),0.f);
    TestTrue(TEXT("Skip restores walking"),Player->GetCharacterMovement()->MovementMode==MOVE_Walking);
    EyeController->UnPossess();

    return true;
}

#endif
