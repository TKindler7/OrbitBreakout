#include "../OBEnemyCharacter.h"
#include "../OBPlayerCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"

namespace OBEnemyBehaviorTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

    // No mission game instance, BeginPlay, map travel or save-slot access. The
    // world supplies real collision queries and a separate timer manager only.
    struct FIsolatedWorld
    {
        UWorld* World = nullptr;
        FIsolatedWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            World = UWorld::CreateWorld(EWorldType::Game, false,
                MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("OBEnemyBehaviorTest")), GetTransientPackage());
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
            Pawn->FinishSpawning(Transform);
            return Pawn;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBEnemyAwarenessTest,
    "OrbitBreakout.Enemies.AwarenessAndCover", OBEnemyBehaviorTests::Flags)

bool FOBEnemyAwarenessTest::RunTest(const FString& Parameters)
{
    OBEnemyBehaviorTests::FIsolatedWorld Fixture;
    if (!TestNotNull(TEXT("An isolated collision world exists"), Fixture.World)) return false;
    AOBEnemyCharacter* Guard = Fixture.SpawnPawn<AOBEnemyCharacter>(FVector(0, 0, 100));
    AOBEnemyCharacter* Ally = Fixture.SpawnPawn<AOBEnemyCharacter>(FVector(0, 700, 100));
    AOBPlayerCharacter* Player = Fixture.SpawnPawn<AOBPlayerCharacter>(FVector(-1400, 0, 100));
    if (!TestNotNull(TEXT("Guard spawned"), Guard) || !TestNotNull(TEXT("Ally spawned"), Ally) ||
        !TestNotNull(TEXT("Player spawned"), Player)) return false;
    Guard->bActive = true;
    Guard->State = EOBEnemyState::Idle;
    Ally->bActive = true;
    Ally->State = EOBEnemyState::Idle;
    Guard->EncounterId = Ally->EncounterId = 1;

    TestFalse(TEXT("An unaware guard cannot see a distant player behind it"), Guard->CanVisuallyDetect(Player));
    Player->SetActorLocation(FVector(-500, 0, 100));
    TestTrue(TEXT("A nearby player is noticed from behind in open space"), Guard->CanVisuallyDetect(Player));

    AActor* Cover = Fixture.World->SpawnActor<AActor>();
    UBoxComponent* CoverBox = NewObject<UBoxComponent>(Cover);
    Cover->SetRootComponent(CoverBox);
    CoverBox->SetBoxExtent(FVector(60, 600, 600));
    CoverBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CoverBox->SetCollisionObjectType(ECC_WorldStatic);
    CoverBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    CoverBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    CoverBox->RegisterComponent();
    Cover->SetActorLocation(FVector(-250, 0, 100));
    Guard->RememberTarget(Player->GetActorLocation());
    TestTrue(TEXT("The report makes the guard aware"), Guard->bAlerted);
    TestFalse(TEXT("Awareness cannot see or shoot through actual cover"), Guard->CanVisuallyDetect(Player));
    CoverBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TestTrue(TEXT("Sight returns when cover no longer blocks the ray"), Guard->CanVisuallyDetect(Player));

    const FVector Report(-1400, 150, 100);
    Ally->ReceiveSquadAlert(Guard, Report);
    TestTrue(TEXT("An active squadmate receives the report"), Ally->bAlerted);
    TestTrue(TEXT("The report is the last-known location"), Ally->LastSeenLocation.Equals(Report));
    const FVector RejectedReport(9000, 9000, 100);
    Ally->bActive = false;
    Ally->ReceiveSquadAlert(Guard, RejectedReport);
    TestTrue(TEXT("Dormant waves do not receive reports"), Ally->LastSeenLocation.Equals(Report));
    Ally->bActive = true;
    Ally->EncounterId = 2;
    Ally->ReceiveSquadAlert(Guard, RejectedReport);
    TestTrue(TEXT("Reports never cross encounter boundaries"), Ally->LastSeenLocation.Equals(Report));
    Ally->EncounterId = 1;
    Ally->State = EOBEnemyState::Dead;
    Ally->ReceiveSquadAlert(Guard, RejectedReport);
    TestTrue(TEXT("Dead enemies cannot reacquire the player"), Ally->LastSeenLocation.Equals(Report));

    Player->SetActorLocation(FVector(-1400, 0, 100));
    TestTrue(TEXT("Recent awareness allows a clear target behind the guard"), Guard->CanVisuallyDetect(Player));
    Guard->LastSeenTime = Fixture.World->GetTimeSeconds() - Guard->AlertMemorySeconds - 1.f;
    TestFalse(TEXT("An old report expires instead of tracking forever"), Guard->HasRecentAwareness());
    TestFalse(TEXT("A distant unseen player is not magically tracked after expiry"), Guard->CanVisuallyDetect(Player));
    return true;
}

namespace OBEnemyBehaviorTests
{
    class FCheckCorpseDelay final : public IAutomationLatentCommand
    {
    public:
        FCheckCorpseDelay(FAutomationTestBase* InTest, TSharedPtr<FIsolatedWorld> InFixture,
            AOBEnemyCharacter* InEnemy, FTimerHandle InHideTimer)
            : Test(InTest), Fixture(MoveTemp(InFixture)), Enemy(InEnemy), HideTimer(InHideTimer) {}
        virtual bool Update() override
        {
            // Timer managers advance only once per real frame. Never mutate
            // GFrameCounter or run the owner's world to speed this check up.
            if (LastFrame == GFrameCounter) return false;
            LastFrame = GFrameCounter;
            FTimerManager& Timers = Fixture->World->GetTimerManager();
            Timers.Tick(.1f);
            if (++Frames == 5)
            {
                Test->TestFalse(TEXT("A paused cleanup timer leaves the death pose visible"), Enemy->IsHidden());
                Timers.UnPauseTimer(HideTimer);
            }
            if (Frames == 15)
                Test->TestFalse(TEXT("The death pose remains visible before its short delay"), Enemy->IsHidden());
            if (Frames < 19) return false;
            Test->TestTrue(TEXT("The defeated body disappears after the delay"), Enemy->IsHidden());
            Test->TestFalse(TEXT("Hidden body animation stops ticking"), Enemy->GetMesh()->IsComponentTickEnabled());
            Test->TestFalse(TEXT("Defeated enemy collision stays disabled"), Enemy->GetActorEnableCollision());
            Test->TestTrue(TEXT("Dead actor remains valid for encounter bookkeeping"), IsValid(Enemy));
            Test->TestTrue(TEXT("Cleanup preserves the defeated state"), Enemy->IsDead());
            Test->TestEqual(TEXT("Cleanup preserves the stable mission ID"), Enemy->StableId, FName(TEXT("TEST_SENTINEL")));
            return true;
        }
    private:
        FAutomationTestBase* Test;
        TSharedPtr<FIsolatedWorld> Fixture;
        AOBEnemyCharacter* Enemy;
        FTimerHandle HideTimer;
        uint64 LastFrame = MAX_uint64;
        int32 Frames = 0;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBEnemyCleanupTest,
    "OrbitBreakout.Enemies.DeathDisappears", OBEnemyBehaviorTests::Flags)

bool FOBEnemyCleanupTest::RunTest(const FString& Parameters)
{
    auto Fixture = MakeShared<OBEnemyBehaviorTests::FIsolatedWorld>();
    if (!TestNotNull(TEXT("An isolated timer world exists"), Fixture->World)) return false;
    AOBEnemyCharacter* Enemy = Fixture->SpawnPawn<AOBEnemyCharacter>(FVector(0, 0, 100));
    if (!TestNotNull(TEXT("Enemy spawned"), Enemy)) return false;
    Enemy->StableId = TEXT("TEST_SENTINEL");
    Enemy->bActive = true;
    Enemy->SetActorHiddenInGame(false);
    Enemy->Die(true);
    TestTrue(TEXT("Death is registered immediately"), Enemy->IsDead());
    TestFalse(TEXT("Collision is disabled immediately"), Enemy->GetActorEnableCollision());
    TestFalse(TEXT("Death pose remains briefly visible"), Enemy->IsHidden());
    Fixture->World->GetTimerManager().PauseTimer(Enemy->CorpseHideTimer);
    Fixture->World->GetTimerManager().Tick(0.f);
    ADD_LATENT_AUTOMATION_COMMAND(OBEnemyBehaviorTests::FCheckCorpseDelay(this, Fixture, Enemy, Enemy->CorpseHideTimer));
    return true;
}

#endif
