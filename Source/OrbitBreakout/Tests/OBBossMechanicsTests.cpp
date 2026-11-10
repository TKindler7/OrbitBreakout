#include "../OBEnemyCharacter.h"
#include "../OBPlayerCharacter.h"
#include "../OBCore.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"

namespace OBBossTests
{
    struct FIsolatedWorld
    {
        UWorld* World = nullptr;
        FIsolatedWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            World = UWorld::CreateWorld(EWorldType::Game, false,
                MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("OBBossTest")), GetTransientPackage());
            Context.SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
        }
        ~FIsolatedWorld()
        {
            if (!World) return;
            GEngine->ShutdownWorldNetDriver(World);
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
        template<typename T> T* SpawnPawn(const FVector& Position, bool bHeavy = false)
        {
            const FTransform Pose(FRotator::ZeroRotator, Position);
            T* Pawn = World->SpawnActorDeferred<T>(T::StaticClass(), Pose, nullptr, nullptr,
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (!Pawn) return nullptr;
            Pawn->AutoPossessAI = EAutoPossessAI::Disabled;
            Pawn->AutoPossessPlayer = EAutoReceiveInput::Disabled;
            if (AOBEnemyCharacter* Enemy = Cast<AOBEnemyCharacter>(Pawn)) Enemy->bHeavy = bHeavy;
            Pawn->FinishSpawning(Pose);
            return Pawn;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBBossMechanicsTest,
    "OrbitBreakout.Boss.TelegraphsSweepsJumpAndCover",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOBBossMechanicsTest::RunTest(const FString& Parameters)
{
    // A separate collision world, with no GameInstance, BeginPlay, save slots,
    // map travel or live player input. Damage still uses the normal player API.
    OBBossTests::FIsolatedWorld Fixture;
    if (!TestNotNull(TEXT("Isolated boss collision world exists"), Fixture.World)) return false;
    AOBEnemyCharacter* Boss = Fixture.SpawnPawn<AOBEnemyCharacter>(FVector(0,0,112), true);
    AOBPlayerCharacter* Player = Fixture.SpawnPawn<AOBPlayerCharacter>(FVector(400,0,96));
    AOBMissionDirector* Director = Fixture.World->SpawnActor<AOBMissionDirector>();
    if (!TestNotNull(TEXT("Boss spawned"), Boss) || !TestNotNull(TEXT("Player spawned"), Player) ||
        !TestNotNull(TEXT("Director spawned"), Director)) return false;
    Director->bInitialized = true;
    Director->Player = Player;
    Director->Phase = EOBPhase::Reactor;
    Player->Director = Director;
    Player->MaxHealth = Player->Health = 1000.f;
    Player->EndSpawnGrace();
    Boss->Director = Director;
    Boss->Target = Player;
    Boss->bActive = true;
    Boss->State = EOBEnemyState::Acquire;
    Boss->Health = Boss->SentinelHealth;
    Boss->bCanSeeTarget = true;
    Boss->SetActorEnableCollision(true);
    TestEqual(TEXT("Bull health is increased to 2400"), Boss->SentinelHealth, 2400.f);

    auto StartWave = [&]()
    {
        Boss->CancelAttack();
        Boss->BossAttackSequence = 2;
        Boss->BeginCharge();
    };
    auto StartRush = [&]()
    {
        Boss->CancelAttack();
        Boss->BossAttackSequence = 1;
        Boss->BeginCharge();
    };

    StartWave();
    TestTrue(TEXT("The third attack begins with the shockwave warning"), Boss->BossAttack==EOBBossAttack::WaveWarning);
    TestEqual(TEXT("A shockwave creates a readable ring"), Boss->BossRingSegments.Num(), 32);
    TestFalse(TEXT("The ring is visible throughout the warning"), Boss->BossRingSegments[0]->bHiddenInGame);
    Boss->TickBossAttack(1.3f);
    TestTrue(TEXT("The boss cannot skip its full shockwave warning"), Boss->BossAttack==EOBBossAttack::WaveWarning);
    TestEqual(TEXT("The warning never deals damage"), Player->Health, 1000.f);
    Boss->TickBossAttack(.06f);
    TestTrue(TEXT("Warning resolves into an expanding wave"), Boss->BossAttack==EOBBossAttack::Shockwave);
    Boss->TickBossAttack(.75f);
    TestEqual(TEXT("The swept expanding ring deals its intended damage"), Player->Health, 976.f);
    Boss->TickBossAttack(.05f);
    TestEqual(TEXT("The same shockwave cannot damage a player twice"), Player->Health, 976.f);

    StartWave();
    TestFalse(TEXT("A wave cannot hit before its ring reaches the player"), Boss->CanShockwaveHit(Player,0.f,200.f));
    TestTrue(TEXT("A grounded player on the ring is vulnerable"), Boss->CanShockwaveHit(Player,350.f,450.f));
    Player->SetActorLocation(FVector(400,0,196));
    TestFalse(TEXT("Jumping over the low ring prevents a hit"), Boss->CanShockwaveHit(Player,350.f,450.f));
    Player->SetActorLocation(FVector(400,0,-104));
    TestFalse(TEXT("The ring cannot hit someone on another deck"), Boss->CanShockwaveHit(Player,350.f,450.f));
    Player->SetActorLocation(FVector(400,0,96));

    AActor* Cover = Fixture.World->SpawnActor<AActor>();
    UBoxComponent* Box = NewObject<UBoxComponent>(Cover);
    Cover->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(35,180,200));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->SetCollisionObjectType(ECC_WorldStatic);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->RegisterComponent();
    Cover->SetActorLocation(FVector(200,0,100));
    TestFalse(TEXT("Solid cover absorbs the wave"), Boss->CanShockwaveHit(Player,350.f,450.f));
    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TestTrue(TEXT("Wave line of sight returns when cover is removed"), Boss->CanShockwaveHit(Player,350.f,450.f));
    Boss->bBossAttackHitPlayer = true;
    TestFalse(TEXT("A consumed hit is rejected even while the ring overlaps"), Boss->CanShockwaveHit(Player,350.f,450.f));

    Player->SetActorLocation(FVector(650,0,96));
    Boss->SetActorLocation(FVector(0,0,112));
    StartRush();
    TestTrue(TEXT("The second attack begins with a rush lane warning"), Boss->BossAttack==EOBBossAttack::RushWarning);
    const FVector LockedDirection = Boss->RushDirection;
    Player->SetActorLocation(FVector(650,300,96));
    Boss->TickBossAttack(1.1f);
    TestTrue(TEXT("Rush warning remains visible for the full duration"), Boss->BossAttack==EOBBossAttack::RushWarning);
    Boss->TickBossAttack(.11f);
    Boss->TickBossAttack(.4f);
    TestTrue(TEXT("A rush keeps its original direction after the player sidesteps"), Boss->RushDirection.Equals(LockedDirection));
    TestTrue(TEXT("A rush does not steer toward the dodging player"), FMath::Abs(Boss->GetActorLocation().Y)<1.f);
    Boss->TickBossAttack(1.f);
    TestEqual(TEXT("Dodging the marked lane avoids rush damage"), Player->Health,976.f);

    Boss->SetActorLocation(FVector(0,0,112));
    Player->SetActorLocation(FVector(650,0,96));
    Cover->SetActorLocation(FVector(350,0,100));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    StartRush();
    Boss->TickBossAttack(1.21f);
    Boss->TickBossAttack(1.f);
    TestTrue(TEXT("A slow-frame rush sweeps into cover instead of tunnelling through"), Boss->GetActorLocation().X<315.f);
    TestTrue(TEXT("Impact with cover gives a recovery window"), Boss->BossAttack==EOBBossAttack::Recovery);
    TestEqual(TEXT("A player behind cover takes no rush damage"), Player->Health,976.f);

    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Boss->SetActorLocation(FVector(0,0,112));
    StartRush();
    Boss->TickBossAttack(1.21f);
    Boss->TickBossAttack(1.f);
    TestEqual(TEXT("A direct swept capsule hit applies one rush hit"), Player->Health,948.f);
    Boss->TickBossAttack(.25f);
    TestEqual(TEXT("Recovery never repeats rush collision damage"), Player->Health,948.f);
    const float NormalRecovery = Boss->RecoveryDuration;
    Boss->Health = 1201.f;
    TestFalse(TEXT("Overdrive does not start above half health"), Boss->IsBossEnraged());
    Boss->Health = 1200.f;
    TestTrue(TEXT("Overdrive starts at half health"), Boss->IsBossEnraged());
    Boss->FinishBossAttack();
    TestTrue(TEXT("Overdrive shortens recovery"), Boss->RecoveryDuration<NormalRecovery);
    TestEqual(TEXT("Overdrive preserves the rush warning"), Boss->RushWarningSeconds,1.2f);
    TestEqual(TEXT("Overdrive preserves the wave warning"), Boss->WaveWarningSeconds,1.35f);

    Boss->CancelAttack();
    TestTrue(TEXT("Cancelling releases special state"), Boss->BossAttack==EOBBossAttack::None);
    TestTrue(TEXT("Cancelling restores normal movement"), Boss->GetCharacterMovement()->MovementMode==MOVE_Walking);
    TestTrue(TEXT("Cancelling hides the telegraph"), Boss->BossRingSegments[0]->bHiddenInGame);
    Player->SetActorLocation(Boss->GetActorLocation()+FVector(400,0,-16));
    StartWave();
    Boss->Die(false);
    TestTrue(TEXT("Death interrupts a committed special"), Boss->BossAttack==EOBBossAttack::None);
    TestTrue(TEXT("Death clears ground telegraphs"), Boss->BossRingSegments[0]->bHiddenInGame);
    TestFalse(TEXT("Death releases the attack slot"), Boss->bHasAttackSlot);
    return true;
}

#endif
