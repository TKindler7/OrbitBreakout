#include "../OBCore.h"
#include "../OBEnemyCharacter.h"
#include "../OBPlayerCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

namespace OBUpgradeIntegrationTests
{
    // This world never begins play, has no game instance, and never accesses
    // any save slot. Only the explicit combat/progression calls below run.
    struct FIsolatedWorld
    {
        UWorld* World = nullptr;
        FIsolatedWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            World = UWorld::CreateWorld(EWorldType::Game, false,
                MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("OBSoloBossGateTest")), GetTransientPackage());
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
        template<typename T> T* SpawnPawn(const FVector& Location)
        {
            const FTransform Transform(FRotator::ZeroRotator, Location);
            T* Pawn = World->SpawnActorDeferred<T>(T::StaticClass(), Transform, nullptr, nullptr,
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (!Pawn) return nullptr;
            Pawn->AutoPossessAI = EAutoPossessAI::Disabled;
            Pawn->AutoPossessPlayer = EAutoReceiveInput::Disabled;
            Pawn->AIControllerClass = nullptr;
            Pawn->FinishSpawning(Transform);
            return Pawn;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBSoloBossGateTest,
    "OrbitBreakout.Upgrade.SoloBossGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOBSoloBossGateTest::RunTest(const FString& Parameters)
{
    OBUpgradeIntegrationTests::FIsolatedWorld Fixture;
    if (!TestNotNull(TEXT("Isolated game world exists"), Fixture.World)) return false;
    AOBMissionDirector* Director = Fixture.World->SpawnActor<AOBMissionDirector>();
    AOBPlayerCharacter* Player = Fixture.SpawnPawn<AOBPlayerCharacter>(FVector(0, 0, 100));
    AOBEnemyCharacter* Boss = Fixture.SpawnPawn<AOBEnemyCharacter>(FVector(800, 0, 112));
    AOBDoor* Exit = Fixture.World->SpawnActor<AOBDoor>();
    APlayerController* Instigator = Fixture.World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Director exists"), Director) || !TestNotNull(TEXT("Player exists"), Player) ||
        !TestNotNull(TEXT("Solo Bull exists"), Boss) || !TestNotNull(TEXT("Exit exists"), Exit) ||
        !TestNotNull(TEXT("Player damage instigator exists"), Instigator)) return false;

    Player->RestoreLoadout(true, 24, 96);
    Boss->bHeavy = true;
    Boss->StableId = TEXT("E05_EN03");
    Boss->EncounterId = 5;
    Boss->Health = Boss->SentinelHealth;
    Exit->DoorId = 5;
    Director->Player = Player;
    Director->Enemies.Add(Boss);
    Director->Doors.Add(Exit);
    Director->ExpectedRoster.FindOrAdd(5).Add(Boss->StableId);
    Director->ClearedEncounters = {1, 2, 3, 4};
    Director->Phase = EOBPhase::Reactor;
    Director->bTerminalComplete = true;
    Director->bInitialized = true;

    Director->ResolveEnemy(5, Boss->StableId);
    TestFalse(TEXT("An unstarted boss encounter cannot open evacuation"), Exit->bOpen);
    Director->EnterZone(5, false);
    TestEqual(TEXT("Crossing containment without HELIX keeps the boss dormant"), Director->ActiveEncounter, 0);
    TestFalse(TEXT("The new weapon comes before the boss fight"), Boss->bActive);
    Player->AcquireHeavyWeapon();
    Director->EnterZone(5, false);
    TestEqual(TEXT("The one-member boss roster activates"), Director->ActiveEncounter, 5);
    TestTrue(TEXT("The Bull receives activation"), Boss->bActive);
    TestEqual(TEXT("Only the Bull remains in his encounter"), Director->GetRemainingEnemies(), 1);
    TestFalse(TEXT("The exit stays closed while the Bull is alive"), Exit->bOpen);
    Director->ResolveEnemy(5, TEXT("E05_EN01"));
    TestFalse(TEXT("A removed escort's ID cannot clear the solo encounter"), Exit->bOpen);

    TestTrue(TEXT("The solo encounter begins with its entrance animation"), Director->IsBossIntroActive());
    UGameplayStatics::ApplyDamage(Boss, 100.f, Instigator, Player, UDamageType::StaticClass());
    TestEqual(TEXT("The entrance cannot be skipped by shooting the boss"), Boss->Health, Boss->SentinelHealth);
    Director->Tick(Boss->BossIntroSeconds + .01f);
    TestTrue(TEXT("Combat stays gated during the return to the player camera"), Director->IsBossIntroActive());
    Director->Tick(.7f);
    TestFalse(TEXT("Combat unlocks after the intro camera returns"), Director->IsBossIntroActive());

    UGameplayStatics::ApplyDamage(Boss, Boss->SentinelHealth * .5f, Instigator, Player, UDamageType::StaticClass());
    TestTrue(TEXT("Half health enters overdrive"), Boss->IsBossEnraged());
    TestFalse(TEXT("Half health does not unlock the exit"), Exit->bOpen);
    UGameplayStatics::ApplyDamage(Boss, Boss->Health - 1.f, Instigator, Player, UDamageType::StaticClass());
    TestEqual(TEXT("The last hit point still blocks evacuation"), Boss->Health, 1.f);
    TestFalse(TEXT("One living boss still owns the gate"), Exit->bOpen);
    UGameplayStatics::ApplyDamage(Boss, 1.f, Instigator, Player, UDamageType::StaticClass());
    TestTrue(TEXT("Ordinary player damage defeats the Bull"), Boss->IsDead());
    TestTrue(TEXT("The solo boss death opens evacuation"), Exit->bOpen);
    TestEqual(TEXT("Progression advances to the pod bay"), static_cast<int32>(Director->Phase), static_cast<int32>(EOBPhase::PodBay));
    TestEqual(TEXT("The boss leaves no unresolved enemies"), Director->GetRemainingEnemies(), 0);
    TestEqual(TEXT("The encounter clears once"), Director->ClearedEncounters.Num(), 5);
    Director->ResolveEnemy(5, Boss->StableId);
    TestEqual(TEXT("Duplicate death reports do not duplicate progression"), Director->ClearedEncounters.Num(), 5);
    TestTrue(TEXT("This test never attaches a game instance"), Fixture.World->GetGameInstance() == nullptr);
    return true;
}

#endif
