#include "../OBCore.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../OBEnemyCharacter.h"
#include "../OBPlayerCharacter.h"
#include "../OBLevel2.h"
#include "../OBMachineEnemy.h"
#include "Camera/PlayerCameraManager.h"
#include "../OBMech.h"
#include "../OBParkourMovement.h"
#include "../OBPilotView.h"
#include "Components/CapsuleComponent.h"
#include "../OBUI.h"
#include "Camera/CameraComponent.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Tests/AutomationCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#if WITH_EDITOR
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#endif
#include "UObject/StrongObjectPtr.h"

// Start and stop our own single-client PIE session with no existing mission save.
// This is an automated state/integration check, not a human playthrough:
// the pawn is positioned at authored interactions and trigger volumes; one guard
// is defeated through simulated aim/fire input and the normal weapon traces;
// other guards receive player-instigated TakeDamage calls; interaction hold time is
// supplied to the public Director API. World timers, deaths, travel, checkpoint
// restoration, door state, and the nine-second ending run through real ticks.
// The game hardcodes its save slot. Refuse any existing progress rather than
// overwrite it, and remove only the save created by this test on completion.
namespace OBMissionTests
{
    constexpr const TCHAR* MissionSlot = TEXT("OB_EmergencyDeparture");
    constexpr const TCHAR* SettingsSlot = TEXT("OB_Settings");
    constexpr const TCHAR* MissionMap = TEXT("/Game/OrbitBreakout/Maps/L_EmergencyDeparture.L_EmergencyDeparture");

    UWorld* FindCampaignWorld(const UOBGameInstance* Session = nullptr)
    {
        UWorld* Result = nullptr;
        if (!GEngine) return nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (!World || World->WorldType != (GIsEditor ? EWorldType::PIE : EWorldType::Game) ||
                (Session && World->GetGameInstance() != Session)) continue;
            if (Result) return nullptr; // Multiple PIE clients need separate isolation.
            Result = World;
        }
        return Result;
    }

    class FProgressionCommand final : public IAutomationLatentCommand
    {
    public:
        FProgressionCommand(FAutomationTestBase* InTest, UOBGameInstance* InSession)
            : Test(InTest), Session(InSession), OriginalSave(InSession->MissionSave.Get()),
              OriginalWarning(InSession->SaveWarning), bOriginalSaveValid(InSession->bSaveValid),
              StageStarted(FPlatformTime::Seconds())
        {
            if (InSession->Settings)
                UGameplayStatics::SaveGameToMemory(InSession->Settings, OriginalSettings);
            bSettingsExisted = UGameplayStatics::DoesSaveGameExist(SettingsSlot, 0);
            if (bSettingsExisted)
                UGameplayStatics::LoadDataFromSlot(OriginalSettingsDisk, SettingsSlot, 0);
        }

        virtual ~FProgressionCommand() override
        {
            // Also release our new save if the automation runner cancels normally.
            // An editor/process crash cannot execute this cleanup.
            ReleaseCombatInput();
            const bool bNeedsWorldReset = bOwnsMissionSlot && !bRestored;
            UWorld* World = bNeedsWorldReset && Session.IsValid() ? FindCampaignWorld(Session.Get()) : nullptr;
            if (AOBMissionDirector* D = World ? AOBMissionDirector::Get(World) : nullptr)
                D->SetActorTickEnabled(false); // Prevent an interrupted ending from saving later.
            RestoreSession();
            if (bNeedsWorldReset && World && !World->bIsTearingDown && Session.IsValid()) Session->ReturnToMenu();
            if (bOwnsMissionSlot && UGameplayStatics::DoesSaveGameExist(MissionSlot, 0))
                UGameplayStatics::DeleteGameInSlot(MissionSlot, 0);
        }

        virtual bool Update() override
        {
            UWorld* World = FindCampaignWorld(Session.Get());
            AOBMissionDirector* D = World ? AOBMissionDirector::Get(World) : nullptr;
            AOBPlayerController* PC = World ? Cast<AOBPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
            AOBPlayerCharacter* P = D ? D->Player.Get() : nullptr;
            const double Now = FPlatformTime::Seconds();
            if (Now - StageStarted > (Stage == EStage::Tunnel ? 260.0 : (Stage == EStage::CrashControl || Stage == EStage::Reopened) ? 100.0 : 60.0))
            {
                Test->AddError(FString::Printf(TEXT("PIE mission stage %d timed out"), static_cast<int32>(Stage)));
                if (Stage == EStage::Cleanup) return true;
                Cleanup(World);
                return false;
            }
            if (!Session.IsValid())
            {
                Test->AddError(TEXT("PIE game instance disappeared during the mission test"));
                return true;
            }
            if (!World || !D || !P || !PC) return false;
            if (bOwnsMissionSlot && !GIsEditor && FParse::Param(FCommandLine::Get(), TEXT("OBRecordCheckpoints")))
                RecordCheckpoint();
            if (Stage == EStage::Cleanup)
            {
                if (World == TravelWorld.Get() || PC->MenuScreen != TEXT("Main")) return false;
                Test->TestFalse(TEXT("Test-created mission save was removed"), UGameplayStatics::DoesSaveGameExist(MissionSlot, 0));
                Test->TestFalse(TEXT("Original empty session still has no Continue"), Session->HasValidSave());
                Test->AddInfo(TEXT("Automated PIE mission state check finished; Main screen restored. No human traversal, aiming, or combat-balancing claim."));
                return true;
            }

            switch (Stage)
            {
            case EStage::Start:
                PC->PerformAction(TEXT("Settings"));
                Test->TestEqual(TEXT("Main opens Settings"), PC->MenuScreen, FName(TEXT("Settings")));
                PC->PerformAction(TEXT("Back"));
                Test->TestEqual(TEXT("Settings Back returns to Main"), PC->MenuScreen, FName(TEXT("Main")));
                PC->PerformAction(TEXT("Controls"));
                PC->PerformAction(TEXT("Back"));
                Test->TestEqual(TEXT("Controls Back returns to Main"), PC->MenuScreen, FName(TEXT("Main")));
                // Recheck immediately before the first write, after any UI work.
                if (UGameplayStatics::DoesSaveGameExist(MissionSlot, 0))
                {
                    Test->AddError(TEXT("Mission save appeared before NewMission; refusing to overwrite it"));
                    return true;
                }
                bOwnsMissionSlot = true;
                TravelWorld = World;
                PC->PerformAction(TEXT("NewMission"));
                Advance(EStage::NewMission);
                break;

            case EStage::NewMission:
                if (World == TravelWorld.Get() || !D->IsGameplayActive()) return false;
                Test->TestEqual(TEXT("New mission starts in Cell"), static_cast<int32>(D->Phase), static_cast<int32>(EOBPhase::Cell));
                Test->TestFalse(TEXT("New mission has no weapon"), P->bWeaponOwned);
                Test->TestEqual(TEXT("New mission has checkpoint zero"), Session->MissionSave->CheckpointId, 0);
                CheckContent(D);
                CheckDisk(0, false);
                D->CommitCheckpoint(2);
                D->BeginEnding();
                Test->TestEqual(TEXT("Unqualified checkpoint is rejected"), Session->MissionSave->CheckpointId, 0);
                Test->TestEqual(TEXT("Early ending is rejected"), static_cast<int32>(D->Phase), static_cast<int32>(EOBPhase::Cell));
                for (AOBEnemyCharacter* Enemy : D->Enemies)
                    if (IsValid(Enemy))
                        Test->TestEqual(TEXT("Dormant guards ignore damage"), Enemy->TakeDamage(20.f, FDamageEvent(), PC, P), 0.f);
                Advance(EStage::CellRelease);
                break;

            case EStage::CellRelease:
                if (!DoorOpen(D, 0)) return false;
                Test->TestFalse(TEXT("Weapon gate remains closed before acquisition"), DoorOpen(D, 1));
                if (!PositionAtInteraction(D, P, PC, EOBInteractionKind::Weapon)) { Cleanup(World); break; }
                GameTime = World->GetTimeSeconds();
                Advance(EStage::Weapon);
                break;

            case EStage::Weapon:
                if (World->GetTimeSeconds() - GameTime < .25f) return false;
                if (!Test->TestTrue(TEXT("Weapon is acquired through validated interaction"), D->TryInteract(P, Interaction.Get()))) { Cleanup(World); break; }
                Test->TestTrue(TEXT("Weapon gate opens on acquisition"), DoorOpen(D, 1));
                Test->TestEqual(TEXT("Weapon grants a full magazine"), P->MagazineAmmo, P->MagazineCapacity);
                Test->TestFalse(TEXT("Used weapon cannot be acquired twice"), D->TryInteract(P, Interaction.Get()));
                if (!EnterTrigger(World, P, 2, false)) { Cleanup(World); break; }
                Advance(EStage::Corridor);
                break;

            case EStage::Corridor:
            {
                if (D->ActiveEncounter != 1) return false;
                Test->TestFalse(TEXT("Corridor gate stays locked during combat"), DoorOpen(D, 2));
                for (AOBEnemyCharacter* E : D->Enemies)
                    if (IsValid(E) && E->StableId == TEXT("E01_EN01")) ShotTarget = E;
                if (!Test->TestTrue(TEXT("Input shooting fixture has a living first corridor trooper"),
                    ShotTarget.IsValid() && !ShotTarget->IsDead() && ShotTarget->bActive)) { Cleanup(World); break; }
                // Controlled starting distance; AI, movement, collision, spread,
                // damage and fire cadence all retain their normal settings.
                FVector Position = ShotTarget->GetActorLocation() - FVector(450.f, 0.f, 0.f);
                Position.Z = 100.f;
                P->GetCharacterMovement()->StopMovementImmediately();
                P->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
                PC->SetControlRotation(FRotator::ZeroRotator);
                InputController = PC;
                bCombatInputHeld = true;
                InitialShotTargetHealth = ShotTarget->Health;
                AmmoBeforeShooting = P->MagazineAmmo;
                LastShotBeforeInput = P->LastShotTime;
                LastHitBeforeInput = P->LastHitTime;
                CombatDeadline = Now + 5.0;
                GameTime = World->GetTimeSeconds();
                PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::RightMouseButton, IE_Pressed, 1.f));
                Advance(EStage::AimAtTrooper);
                break;
            }

            case EStage::AimAtTrooper:
                if (!CheckCombatFixture(P, Now)) { Cleanup(World); break; }
                AimAtShotTarget(P, PC);
                if (World->GetTimeSeconds() - GameTime < .25 || !P->bAiming || !PC->IsInputKeyDown(EKeys::RightMouseButton)) return false;
                // SetControlRotation takes effect on a later camera tick. Wait
                // for all three unobstructed traces before injecting fire.
                if (!WeaponTraceReachesTarget(World, P)) return false;
                if (FParse::Param(FCommandLine::Get(),TEXT("OBLoaderReview")))
                    FScreenshotRequest::RequestScreenshot(TEXT("PilotArcSight.png"),true,false);
                Test->TestTrue(TEXT("Simulated aim input reaches the character's aiming state"), P->bAiming);
                Test->AddInfo(TEXT("Input shooting fixture has camera-to-target, shoulder-to-muzzle and muzzle-to-impact visibility."));
                PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Pressed, 1.f));
                Advance(EStage::ShootTrooper);
                break;

            case EStage::ShootTrooper:
                if (!CheckCombatFixture(P, Now)) { Cleanup(World); break; }
                AimAtShotTarget(P, PC);
                if (!ShotTarget->IsDead()) return false;
                Test->TestTrue(TEXT("Simulated fire invokes the normal FireShot path"), P->LastShotTime > LastShotBeforeInput);
                Test->TestTrue(TEXT("Normal weapon hit trace records an enemy hit"), P->LastHitTime > LastHitBeforeInput);
                Test->TestTrue(TEXT("Input-fired weapon damage reduces guard health"), ShotTarget->Health < InitialShotTargetHealth);
                Test->TestTrue(TEXT("Input shooting consumes enough magazine rounds for the kill"),
                    AmmoBeforeShooting - P->MagazineAmmo >= FMath::CeilToInt(InitialShotTargetHealth / P->ShotDamage));
                Test->TestEqual(TEXT("Input shooting kills the targeted trooper"), ShotTarget->Health, 0.f);
                Test->TestEqual(TEXT("Two corridor guards remain after the input-driven kill"), D->GetRemainingEnemies(), 2);
                Test->TestFalse(TEXT("One input-driven kill does not unlock the corridor gate"), DoorOpen(D, 2));
                Test->AddInfo(FString::Printf(TEXT("Simulated aim/fire defeated E01_EN01 using %d magazine rounds; no direct damage was applied to this guard by the test."),
                    AmmoBeforeShooting - P->MagazineAmmo));
                ReleaseCombatInput();
                Advance(EStage::ReleaseFire);
                break;

            case EStage::ReleaseFire:
                if (Now - StageStarted > 2.0)
                {
                    Test->AddError(TEXT("Simulated aim/fire release did not reach the player input state"));
                    Cleanup(World);
                    break;
                }
                if (PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::RightMouseButton) || P->bAiming) return false;
                AmmoAfterRelease = P->MagazineAmmo;
                GameTime = World->GetTimeSeconds();
                Advance(EStage::ReleasedFireStable);
                break;

            case EStage::ReleasedFireStable:
                if (World->GetTimeSeconds() - GameTime < P->ShotInterval + .1f) return false;
                Test->TestEqual(TEXT("Released fire does not emit stale timer-driven shots"), P->MagazineAmmo, AmmoAfterRelease);
                if (!DefeatEncounter(D, PC, P, 1, 3, 1)) { Cleanup(World); break; }
                Test->TestTrue(TEXT("Corridor gate opens only after all three deaths"), DoorOpen(D, 2));
                DepleteLoadout(P);
                if (!EnterTrigger(World, P, 3, true)) { Cleanup(World); break; }
                Advance(EStage::CargoCheckpoint);
                break;

            case EStage::CargoCheckpoint:
                if (Session->MissionSave->CheckpointId != 1) return false;
                CheckRecovery(P);
                CheckDisk(1, false);
                if (!EnterTrigger(World, P, 3, false)) { Cleanup(World); break; }
                Advance(EStage::CargoBeforeDeath);
                break;

            case EStage::CargoBeforeDeath:
                if (D->ActiveEncounter != 2) return false;
                if (!DefeatEncounter(D, PC, P, 2, 3)) { Cleanup(World); break; }
                Test->TestEqual(TEXT("Reinforcements are delayed after first cargo wave"), D->ActiveEncounter, 0);
                P->EndSpawnGrace();
                P->TakeDamage(P->MaxHealth + 1.f, FDamageEvent(), nullptr, D);
                Test->TestTrue(TEXT("Lethal damage kills the player"), P->bDead);
                Test->TestFalse(TEXT("Dead player cannot keep combat active"), D->IsCombatAllowed());
                Advance(EStage::DeathScreen);
                break;

            case EStage::DeathScreen:
                if (PC->MenuScreen != TEXT("Dead")) return false;
                Test->TestTrue(TEXT("Death screen pauses the world"), UGameplayStatics::IsGamePaused(World));
                Test->TestNull(TEXT("Death cancels interaction hold"), D->HoldTarget.Get());
                TravelWorld = World;
                PC->PerformAction(TEXT("Retry"));
                Advance(EStage::CargoRetry);
                break;

            case EStage::CargoRetry:
                if (World == TravelWorld.Get() || !D->IsGameplayActive()) return false;
                CheckRecovery(P);
                Test->TestEqual(TEXT("Retry restores Cargo phase"), static_cast<int32>(D->Phase), static_cast<int32>(EOBPhase::Cargo));
                Test->TestEqual(TEXT("Retry restores only the cleared corridor"), D->ClearedEncounters.Num(), 1);
                Test->TestTrue(TEXT("Corridor remains cleared after Retry"), D->ClearedEncounters.Contains(1));
                Test->TestFalse(TEXT("Retry resets unsaved terminal completion"), D->bTerminalComplete);
                Test->TestFalse(TEXT("Retry closes the unresolved cargo gate"), DoorOpen(D, 3));
                CheckEncounterRestored(D, 2, 3);
                CheckEncounterRestored(D, 3, 2);
                if (!EnterTrigger(World, P, 3, false)) { Cleanup(World); break; }
                Advance(EStage::CargoAgain);
                break;

            case EStage::CargoAgain:
            {
                if (D->ActiveEncounter != 2) return false;
                if (!DefeatEncounter(D, PC, P, 2, 3)) { Cleanup(World); break; }
                GameTime = World->GetTimeSeconds();
                PC->ShowScreen(TEXT("Pause"));
                Test->TestFalse(TEXT("Pause disables gameplay"), D->IsGameplayActive());
                const float HealthBefore = P->Health;
                P->TakeDamage(20.f, FDamageEvent(), nullptr, D);
                Test->TestEqual(TEXT("Paused player ignores damage"), P->Health, HealthBefore);
                Advance(EStage::PausedWave);
                break;
            }

            case EStage::PausedWave:
                if (Now - StageStarted < .35) return false;
                Test->TestEqual(TEXT("Pause freezes world time"), World->GetTimeSeconds(), GameTime);
                Test->TestEqual(TEXT("Cargo reinforcements do not start while paused"), D->ActiveEncounter, 0);
                PC->PerformAction(TEXT("Settings"));
                PC->PerformAction(TEXT("Back"));
                Test->TestEqual(TEXT("Settings opened from Pause returns to Pause"), PC->MenuScreen, FName(TEXT("Pause")));
                PC->PerformAction(TEXT("Resume"));
                Test->TestTrue(TEXT("Resume re-enables gameplay"), D->IsGameplayActive());
                Advance(EStage::Reinforcements);
                break;

            case EStage::Reinforcements:
                if (D->ActiveEncounter != 3) return false;
                Test->TestTrue(TEXT("Cargo wave starts after its two-second game timer"), World->GetTimeSeconds() - GameTime >= 1.9f);
                Test->TestFalse(TEXT("Cargo gate remains closed until reinforcement deaths"), DoorOpen(D, 3));
                if (!DefeatEncounter(D, PC, P, 3, 2)) { Cleanup(World); break; }
                Test->TestTrue(TEXT("Cargo gate opens after both waves"), DoorOpen(D, 3));
                if (!PositionAtInteraction(D, P, PC, EOBInteractionKind::Terminal)) { Cleanup(World); break; }
                GameTime = World->GetTimeSeconds();
                Advance(EStage::Terminal);
                break;

            case EStage::Terminal:
                if (World->GetTimeSeconds() - GameTime < .25f) return false;
                if (!CheckTerminalHold(D, PC, P)) { Cleanup(World); break; }
                Test->TestFalse(TEXT("Security gate remains closed after terminal alone"), DoorOpen(D, 4));
                if (!DefeatEncounter(D, PC, P, 4, 3)) { Cleanup(World); break; }
                Test->TestTrue(TEXT("Security gate requires terminal and all response deaths"), DoorOpen(D, 4));
                DepleteLoadout(P);
                if (!EnterTrigger(World, P, 5, true)) { Cleanup(World); break; }
                Advance(EStage::ReactorCheckpoint);
                break;

            case EStage::ReactorCheckpoint:
                if (Session->MissionSave->CheckpointId != 2) return false;
                CheckRecovery(P);
                CheckDisk(2, false);
                TravelWorld = World;
                D->RetryCheckpoint();
                Advance(EStage::ReactorRetry);
                break;

            case EStage::ReactorRetry:
                if (World == TravelWorld.Get() || !D->IsGameplayActive()) return false;
                CheckRecovery(P);
                Test->TestEqual(TEXT("CP02 restores Reactor phase"), static_cast<int32>(D->Phase), static_cast<int32>(EOBPhase::Reactor));
                Test->TestEqual(TEXT("CP02 restores four cleared encounters"), D->ClearedEncounters.Num(), 4);
                Test->TestTrue(TEXT("CP02 preserves terminal completion"), D->bTerminalComplete);
                Test->TestTrue(TEXT("CP02 preserves security access"), DoorOpen(D, 4));
                Test->TestFalse(TEXT("CP02 keeps reactor exit locked"), DoorOpen(D, 5));
                CheckEncounterRestored(D, 5, 1);
                if (!PositionAtInteraction(D, P, PC, EOBInteractionKind::HeavyWeapon)) { Cleanup(World); break; }
                if (!Test->TestTrue(TEXT("Pre-reactor armory equips HELIX-12"), D->TryInteract(P, Interaction.Get()))) { Cleanup(World); break; }
                Test->TestTrue(TEXT("Boss loadout is owned before entering the arena"), P->bHelixOwned);
                if (!EnterTrigger(World, P, 5, false)) { Cleanup(World); break; }
                Advance(EStage::Reactor);
                break;

            case EStage::Reactor:
                if (D->ActiveEncounter != 5) return false;
                if (D->IsBossIntroActive()) return false;
                if (!DefeatEncounter(D, PC, P, 5, 1)) { Cleanup(World); break; }
                Test->TestTrue(TEXT("Sentinel encounter opens reactor exit"), DoorOpen(D, 5));
                if (!EnterTrigger(World, P, 6, false)) { Cleanup(World); break; }
                Advance(EStage::PodBay);
                break;

            case EStage::PodBay:
                if (D->ActiveEncounter != 6) return false;
                Test->TestFalse(TEXT("Pod access is locked during final encounter"), DoorOpen(D, 6));
                if (!DefeatEncounter(D, PC, P, 6, 4)) { Cleanup(World); break; }
                Test->TestTrue(TEXT("Final four deaths unlock pod access"), DoorOpen(D, 6));
                if (!PositionAtInteraction(D, P, PC, EOBInteractionKind::Pod)) { Cleanup(World); break; }
                GameTime = World->GetTimeSeconds();
                Advance(EStage::BoardPod);
                break;

            case EStage::BoardPod:
                if (World->GetTimeSeconds() - GameTime < .25f) return false;
                if (!Test->TestTrue(TEXT("Secured pod accepts launch interaction"), D->TryInteract(P, Interaction.Get()))) { Cleanup(World); break; }
                P->bInteractHeld = true;
                D->UpdateHold(P, Interaction.Get(), .4f);
                Test->TestEqual(TEXT("Partial pod hold does not launch"), static_cast<int32>(D->Phase), static_cast<int32>(EOBPhase::BoardPod));
                D->UpdateHold(P, Interaction.Get(), .7f);
                Test->TestEqual(TEXT("Completed pod hold starts ending"), static_cast<int32>(D->Phase), static_cast<int32>(EOBPhase::Ending));
                Test->TestFalse(TEXT("Ending disables combat"), D->IsCombatAllowed());
                PodStart = Interaction->GetActorLocation();
                Advance(EStage::Ending);
                break;

            case EStage::Ending:
                if (auto* Tunnel = Cast<AOBLevel2Director>(D))
                {
                    if (!Tunnel->IsCinematicActive() && !Tunnel->IsGameplayActive()) return false;
                    Test->TestTrue(TEXT("Departure reaches the authored tunnel map"), World->GetMapName().EndsWith(TEXT("L_DeadSignal")));
                    Test->TestFalse(TEXT("Handoff stays in the campaign save slot"), Session->bDevelopmentChapterSession);
                    Test->TestEqual(TEXT("Handoff selects chapter two"), Session->MissionSave->ChapterId, 2);
                    Test->TestTrue(TEXT("Crash retains the pilot and loadout"), P->bWeaponOwned);
                    Advance(EStage::CrashControl);
                }
                else if (D->GetEndingElapsed() > 8.f && Interaction.IsValid())
                    Test->TestTrue(TEXT("Departure moves the Star Sparrow"), Interaction->GetActorLocation().X > PodStart.X + 2000.f);
                break;
            case EStage::CrashControl:
                if (!D->IsGameplayActive()) return false;
                Test->TestTrue(TEXT("Natural crash returns the pilot camera"), PC->GetViewTarget()==P);
                CheckDisk(0,false);
                Session->ReturnToMenu();
                Advance(EStage::Reopen);
                break;
            case EStage::Reopen:
                if (PC->MenuScreen != TEXT("Main")) return false;
                Session->ContinueMission();
                Advance(EStage::Reopened);
                break;
            case EStage::Reopened:
                if (!Cast<AOBLevel2Director>(D) || !D->IsGameplayActive()) return false;
                Test->TestEqual(TEXT("Continue reopens the tunnel checkpoint"),Session->MissionSave->CheckpointKey,FName(TEXT("L2.Crash")));
                Test->AddInfo(TEXT("Tunnel traversal uses authored zone entries, positioned interactions, simulated hold durations and normal damage to active machines."));
                Advance(EStage::Tunnel);
                break;
            case EStage::Tunnel:
                if (DriveTunnel(*CastChecked<AOBLevel2Director>(D),*P,*PC)) Cleanup(World);
                break;

            default: break;
            }
            return false;
        }

    private:
        enum class EStage { Start, NewMission, CellRelease, Weapon, Corridor,
            AimAtTrooper, ShootTrooper, ReleaseFire, ReleasedFireStable,
            CargoCheckpoint, CargoBeforeDeath, DeathScreen, CargoRetry, CargoAgain,
            PausedWave, Reinforcements, Terminal, ReactorCheckpoint, ReactorRetry,
            Reactor, PodBay, BoardPod, Ending, CrashControl, Reopen, Reopened, Tunnel, Cleanup };
        FAutomationTestBase* Test;
        TWeakObjectPtr<UOBGameInstance> Session;
        TStrongObjectPtr<UOBMissionSave> OriginalSave;
        FString OriginalWarning;
        bool bOriginalSaveValid = false;
        bool bOwnsMissionSlot = false;
        bool bSettingsExisted = false;
        bool bRestored = false;
        TArray<uint8> OriginalSettings, OriginalSettingsDisk;
        TWeakObjectPtr<UWorld> TravelWorld;
        TWeakObjectPtr<AOBInteractable> Interaction;
        TWeakObjectPtr<AOBEnemyCharacter> ShotTarget;
        TWeakObjectPtr<AOBPlayerController> InputController;
        bool bCombatInputHeld = false;
        double CombatDeadline = 0.0;
        float InitialShotTargetHealth = 0.f;
        float LastShotBeforeInput = -100.f;
        float LastHitBeforeInput = -100.f;
        int32 AmmoBeforeShooting = 0;
        int32 AmmoAfterRelease = 0;
        EStage Stage = EStage::Start;
        double StageStarted;
        double GameTime = 0.0;
        FVector PodStart = FVector::ZeroVector;
        TArray<float> UplinkFrameMilliseconds;

        void Advance(EStage Next) { Stage = Next; StageStarted = FPlatformTime::Seconds(); }

        TSet<FName> RecordedCheckpoints;
        bool bLoaderFight = false;
        bool bPulsePressed = false;
        bool bCockpitCaptured = false;
        float NextPulseTime = 0.f;
        float HealthAtBoard = 0.f;
        int32 AmmoAtBoard = 0;
        float FightStarted = 0.f;
        float LastFightSample = 0.f;


        void RecordCheckpoint()
        {
            const UOBMissionSave* Save = Session->MissionSave;
            if (!Save || RecordedCheckpoints.Contains(Save->CheckpointKey) || Save->bPendingChapter2 ||
                Save->bMissionCompleted || (Save->ChapterId == 2 && !Save->bLevel2IntroComplete)) return;
            TArray<uint8> Bytes;
            if (!UGameplayStatics::LoadDataFromSlot(Bytes, MissionSlot, 0)) return;
            const auto* Disk = Cast<UOBMissionSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
            if (!Disk || Disk->CheckpointKey != Save->CheckpointKey || !Disk->IsValidSave()) return;
            const FString Backup = TEXT("Review_") + Disk->CheckpointKey.ToString();
            if (Test->TestTrue(TEXT("Archive checkpoint bytes for cold boot"), UGameplayStatics::SaveDataToSlot(Bytes, Backup, 0)))
                RecordedCheckpoints.Add(Disk->CheckpointKey);
        }

        bool DoorOpen(AOBMissionDirector* D, int32 Id) const
        {
            for (AOBDoor* Door : D->Doors)
                if (IsValid(Door) && Door->DoorId == Id) return Door->bOpen;
            return false;
        }

        bool DriveTunnel(AOBLevel2Director& D,AOBPlayerCharacter& P,AOBPlayerController& PC)
        {
            if (PC.MenuScreen == TEXT("Complete"))
            {
                if (!UplinkFrameMilliseconds.IsEmpty())
                {
                    UplinkFrameMilliseconds.Sort();
                    const int32 Last = UplinkFrameMilliseconds.Num() - 1;
                    Test->AddInfo(FString::Printf(TEXT("Rendered uplink automation tick samples: %d, median %.2f ms, p95 %.2f ms"),
                        Last + 1, UplinkFrameMilliseconds[Last / 2], UplinkFrameMilliseconds[FMath::RoundToInt(Last * .95f)]));
                }
                Test->TestEqual(TEXT("Exactly thirteen finale combatants spawned"),D.FinaleSpawned,13);
                Test->TestTrue(TEXT("Campaign saves the rescue response"),Session->MissionSave->bLevel2Completed);
                CheckDisk(3,true);
                return true;
            }
            if (PC.MenuScreen == TEXT("Dead")) { Test->AddError(TEXT("Campaign driver died in tunnel")); return true; }
            if (D.IsCinematicActive() || !D.IsGameplayActive()) return false;
            for (AOBMachineEnemy* Enemy : D.Machines)
                if (IsValid(Enemy) && Enemy->bActive && !Enemy->IsDead() && !(bLoaderFight && Enemy->EncounterId==207))
                    Enemy->TakeDamage(100000.f,FDamageEvent(),&PC,&P);
            const auto Move = [&](FVector Position)
            {
                P.GetCharacterMovement()->StopMovementImmediately();
                P.SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
                P.GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            };
            const auto Zone = [&](int32 Number,FVector Position)
            {
                Move(Position);
                PC.SetControlRotation(FRotator(0,180,0));
                D.EnterZone(Number,false);
            };
            const auto Use = [&](EOBLevel2Action Action)
            {
                if (!D.IsActionAvailable(Action)) return;
                for (AOBInteractable* Item : D.Interactions)
                {
                    auto* Control=Cast<AOBLevel2Interactable>(Item);
                    if (!Control || Control->Action!=Action) continue;
                    // Consoles face different walls. Try accessible sides and let the director
                    // validate distance, facing and visibility before a hold can complete.
                    for (FVector Side : {FVector(-1,0,0),FVector(0,-1,0),FVector(1,0,0),FVector(0,1,0)})
                    {
                        FVector Position=Control->GetActorLocation()+Side*155.f;
                        FHitResult Floor;
                        FCollisionQueryParams Probe(SCENE_QUERY_STAT(OBCampaignFloor),false,&P);
                        if (!P.GetWorld()->LineTraceSingleByChannel(Floor,Position+FVector(0,0,180),Position-FVector(0,0,300),ECC_Pawn,Probe)
                            || !P.GetCharacterMovement()->IsWalkable(Floor)) continue;
                        Position.Z=Floor.ImpactPoint.Z+P.GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3.f;
                        if (P.GetWorld()->OverlapBlockingTestByChannel(Position,FQuat::Identity,ECC_Pawn,
                            P.GetCapsuleComponent()->GetCollisionShape(),FCollisionQueryParams(SCENE_QUERY_STAT(OBCampaignUse),false,&P))) continue;
                        Move(Position); PC.SetControlRotation((-Side).Rotation());
                        PC.PlayerCameraManager->UpdateCamera(.02f);
                        if (!D.TryInteract(&P,Control)) continue;
                        if (D.HoldTarget==Control)
                        {
                            P.bInteractHeld=true;
                            D.UpdateHold(&P,Control,FMath::Max(2.1f,Control->HoldSeconds+.1f));
                            P.bInteractHeld=false;
                        }
                        return;
                    }
                }
            };
            if (D.CurrentZone<2) Zone(2,FVector(2350,0,102));
            else if (D.CurrentZone<3 && D.IsEncounterCleared(202)) Zone(3,FVector(6000,3500,145));
            else if (D.CurrentZone==3)
            {
                if (!D.bJunctionIsolated) Use(EOBLevel2Action::Breaker);
                else if (!D.bJunctionDrained) Use(EOBLevel2Action::Pump);
                else Zone(4,FVector(9700,3000,102));
            }
            else if (D.CurrentZone==4 && D.IsEncounterCleared(205)) Zone(5,FVector(12200,6000,104));
            else if (D.CurrentZone==5)
            {
                if (!D.bCouplerOwned) Use(EOBLevel2Action::Coupler);
                else if (!D.bCouplerInserted) Use(EOBLevel2Action::Socket);
                else if (!D.bAuxPowerRestored) Use(EOBLevel2Action::Power);
                else
                {
                    Zone(6,FVector(17000,6950,102));
                    auto* Loader=D.WardenMech.Get();
                    if (!Test->TestNotNull(TEXT("Authored Warden bay contains the loader"),Loader)) return true;
                    const FVector Dock=Loader->GetActorLocation();
                    Move(FVector(Dock.X-160,Dock.Y,100));
                    const float Health=P.Health;
                    const int32 Ammo=P.MagazineAmmo;
                    // Square uses the same contextual dispatch as a DualSense.
                    PC.InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_FaceButton_Left,IE_Pressed,1.f));
                    PC.InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_FaceButton_Left,IE_Released,0.f));
                    if (!Test->TestTrue(TEXT("Square boards the Warden loader"),PC.GetPawn()==Loader && PC.GetViewTarget()==Loader)) return true;
                    PC.InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_FaceButton_Left,IE_Pressed,1.f));
                    PC.InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_FaceButton_Left,IE_Released,0.f));
                    Test->TestTrue(TEXT("Square exits and preserves inventory"),PC.GetPawn()==&P && P.Health==Health && P.MagazineAmmo==Ammo);
                    if (FParse::Param(FCommandLine::Get(),TEXT("OBLoaderReview")))
                    {
                        HealthAtBoard=Health; AmmoAtBoard=Ammo;
                        bLoaderFight=Loader->Board(P);
                        if (!Test->TestTrue(TEXT("Loader can reboard for the Warden fight"),bLoaderFight)) return true;
                        FightStarted=D.GetWorld()->GetTimeSeconds();
                    }
                }
            }
            else if (D.CurrentZone==6 && bLoaderFight && !D.bWardenDefeated)
            {
                AOBMech* Loader = Cast<AOBMech>(PC.GetPawn());
                if (!Test->TestNotNull(TEXT("Cockpit survives live Warden combat"),Loader)) return true;
                const float Now=D.GetWorld()->GetTimeSeconds();
                if (Now-FightStarted>75.f) { Test->AddError(TEXT("Loader could not defeat Warden within 75 seconds")); return true; }
                if (bPulsePressed && Now >= NextPulseTime - .30f)
                {
                    PC.InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightTrigger,IE_Released,0.f));
                    bPulsePressed=false;
                }
                for (AOBMachineEnemy* Enemy : D.Machines)
                {
                    if (!IsValid(Enemy) || !Enemy->bActive || Enemy->EncounterId!=207 || Enemy->IsDead()) continue;
                    FVector Eye; FRotator Rotation;
                    PC.GetPlayerViewPoint(Eye,Rotation);
                    PC.SetControlRotation((Enemy->GetActorLocation()-Eye).Rotation());
                    if (!bCockpitCaptured && Now-FightStarted>.8f)
                    {
                        FScreenshotRequest::RequestScreenshot(TEXT("LoaderCockpit.png"),true,false);
                        bCockpitCaptured=true;
                    }
                    // Save heat for the exposed core instead of dumping every burst into the shield.
                    // Move sideways during the telegraph using the normal, bounded movement binding.
                    const float Dodge = Enemy->State == EOBMachineState::PulseWarning
                        ? (FMath::Fmod(Now-FightStarted,8.f)<4.f ? 1.f : -1.f) : 0.f;
                    PC.InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftX,IE_Axis,Dodge,1));
                    if (Now-LastFightSample >= 5.f)
                    {
                        UE_LOG(LogTemp,Display,TEXT("OB_LOADER_COMBAT warden=%.0f armor=%.0f heat=%.0f"),Enemy->Health,Loader->Armor,Loader->Heat);
                        LastFightSample=Now;
                    }
                    const bool bAttackWindow = Enemy->IsCoreExposed() || Loader->Heat < 8.f;
                    if (!bPulsePressed && Now>=NextPulseTime && bAttackWindow &&
                        Loader->Heat + Loader->HeatPerShot < 99.f && !Loader->bOverheated)
                    {
                        PC.InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightTrigger,IE_Pressed,1.f));
                        bPulsePressed=true; NextPulseTime=Now+.56f;
                    }
                    break;
                }
            }
            else if (D.CurrentZone==6 && D.bWardenDefeated)
            {
                if (bLoaderFight)
                {
                    if (PC.GetPawn()!=&P) return false;
                    PC.InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftX,IE_Axis,0.f,1));
                    PC.InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightTrigger,IE_Released,0.f));
                    Test->TestTrue(TEXT("Live Warden resolved with cockpit cannon and safe dismount"),
                        P.Health==HealthAtBoard && P.MagazineAmmo==AmmoAtBoard && !P.OccupiedMech.IsValid());
                    Test->AddInfo(TEXT("Warden defeated using simulated R2 and normal cockpit cannon damage"));
                }
                Zone(7,FVector(19250,9500,102));
            }
            else if (D.CurrentZone==7)
            {
                if (D.IsUplinkActive() && !D.IsUplinkPaused())
                    UplinkFrameMilliseconds.Add(1000.f * FApp::GetDeltaTime());
                if (!D.bStabilizerA || D.IsUplinkPaused()) Use(EOBLevel2Action::StabilizerA);
                else if (!D.bStabilizerB) Use(EOBLevel2Action::StabilizerB);
                else if (D.IsActionAvailable(EOBLevel2Action::Transmitter)) Use(EOBLevel2Action::Transmitter);
                else
                {
                    // Turn away from marked security entries so their real visibility guards can release the wave.
                    PC.SetControlRotation(FRotator(0,FMath::Fmod(D.GetWorld()->GetTimeSeconds()*70.f,360.f),0));
                }
            }
            return false;
        }

        void CheckContent(AOBMissionDirector* D)
        {
            FString Errors;
            const bool bValid = D->ValidateContent(Errors);
            Test->TestTrue(FString(TEXT("Authored content is valid: ")) + Errors, bValid);
            if (!Errors.IsEmpty()) Test->AddError(Errors);
            Test->TestEqual(TEXT("Mission has fifteen guards and one solo boss"), D->Enemies.Num(), 16);
            Test->TestEqual(TEXT("Mission has seven gates"), D->Doors.Num(), 7);
            const int32 Expected[] = {0, 3, 3, 2, 3, 1, 4};
            int32 HeavyCount = 0;
            for (int32 Id = 1; Id <= 6; ++Id)
            {
                int32 Count = 0;
                for (AOBEnemyCharacter* E : D->Enemies)
                    if (IsValid(E) && E->EncounterId == Id) { ++Count; if (E->bHeavy) ++HeavyCount; }
                Test->TestEqual(FString::Printf(TEXT("Encounter %d has its authored roster"), Id), Count, Expected[Id]);
            }
            Test->TestEqual(TEXT("Roster contains exactly one sentinel"), HeavyCount, 1);
        }

        bool PositionAtInteraction(AOBMissionDirector* D, AOBPlayerCharacter* P, AOBPlayerController* PC, EOBInteractionKind Kind)
        {
            for (AOBInteractable* I : D->Interactions)
                if (IsValid(I) && I->Kind == Kind)
                {
                    Interaction = I;
                    const FVector Position = I->GetActorLocation() + FVector(-180.f, 0.f, 100.f);
                    P->GetCharacterMovement()->StopMovementImmediately();
                    P->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
                    PC->SetControlRotation(FRotator::ZeroRotator);
                    return true;
                }
            Test->AddError(TEXT("Required authored interaction is missing"));
            return false;
        }

        bool EnterTrigger(UWorld* World, AOBPlayerCharacter* P, int32 Zone, bool bCheckpoint)
        {
            for (TActorIterator<AOBZoneTrigger> It(World); It; ++It)
                if (It->Zone == Zone && It->bCheckpoint == bCheckpoint)
                {
                    FVector Position = It->GetActorLocation();
                    Position.Z = 100.f; // Authored floor is Z=0; let movement settle naturally.
                    P->GetCharacterMovement()->StopMovementImmediately();
                    P->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
                    return true;
                }
            Test->AddError(FString::Printf(TEXT("Missing zone %d checkpoint=%d trigger"), Zone, bCheckpoint));
            return false;
        }

        void ReleaseCombatInput()
        {
            if (!bCombatInputHeld) return;
            if (AOBPlayerController* PC = InputController.Get())
            {
                PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.f));
                PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::RightMouseButton, IE_Released, 0.f));
            }
            bCombatInputHeld = false;
        }

        bool CheckCombatFixture(AOBPlayerCharacter* P, double Now)
        {
            if (!ShotTarget.IsValid() || P->bDead || Now > CombatDeadline)
            {
                Test->AddError(FString::Printf(TEXT("Input shooting failed within five seconds (target=%s health=%.1f magazine=%d aiming=%d playerDead=%d). No direct-damage fallback."),
                    *GetNameSafe(ShotTarget.Get()), ShotTarget.IsValid() ? ShotTarget->Health : -1.f,
                    P->MagazineAmmo, P->bAiming, P->bDead));
                return false;
            }
            return true;
        }

        void AimAtShotTarget(AOBPlayerCharacter* P, AOBPlayerController* PC)
        {
            if (P->FollowCamera && ShotTarget.IsValid())
                PC->SetControlRotation((ShotTarget->GetActorLocation() - P->FollowCamera->GetComponentLocation()).Rotation());
        }

        bool WeaponTraceReachesTarget(UWorld* World, AOBPlayerCharacter* P) const
        {
            if (!P->FollowCamera || !ShotTarget.IsValid()) return false;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(OBMissionInputFixture), true, P);
            FHitResult CameraHit, ShoulderHit, MuzzleHit;
            const FVector Camera = P->FollowCamera->GetComponentLocation();
            World->LineTraceSingleByChannel(CameraHit, Camera, Camera + P->GetAimDirection() * P->WeaponRange, ECC_Visibility, Query);
            if (CameraHit.GetActor() != ShotTarget.Get()) return false;
            const FVector Muzzle = P->GetMuzzleLocation();
            if (World->LineTraceSingleByChannel(ShoulderHit, P->GetActorLocation() + FVector(0, 0, 40.f), Muzzle, ECC_Visibility, Query)) return false;
            const FVector Direction = (CameraHit.ImpactPoint - Muzzle).GetSafeNormal();
            const FVector End = Muzzle + Direction * FMath::Min(P->WeaponRange, FVector::Distance(Muzzle, CameraHit.ImpactPoint) + 2.f);
            World->LineTraceSingleByChannel(MuzzleHit, Muzzle, End, ECC_Visibility, Query);
            return MuzzleHit.GetActor() == ShotTarget.Get();
        }

        bool DefeatEncounter(AOBMissionDirector* D, AOBPlayerController* PC, AOBPlayerCharacter* P, int32 Id, int32 Expected, int32 ExpectedAlreadyDead = 0)
        {
            if (!Test->TestEqual(TEXT("Correct encounter is active before damage"), D->ActiveEncounter, Id)) return false;
            int32 AlreadyDead = 0;
            for (AOBEnemyCharacter* E : D->Enemies)
                if (IsValid(E) && E->EncounterId == Id && E->IsDead()) ++AlreadyDead;
            if (!Test->TestEqual(TEXT("Previously defeated guards match the expected input-test result"), AlreadyDead, ExpectedAlreadyDead)) return false;
            int32 Count = 0;
            for (AOBEnemyCharacter* E : D->Enemies)
            {
                if (!IsValid(E) || E->EncounterId != Id) continue;
                ++Count;
                if (E->IsDead()) continue;
                const float InitialHealth = E->Health;
                Test->TestEqual(TEXT("Guards reject damage without a player instigator"), E->TakeDamage(P->ShotDamage, FDamageEvent(), nullptr, P), 0.f);
                Test->TestEqual(TEXT("Rejected damage preserves guard health"), E->Health, InitialHealth);
                for (int32 Hit = 0; Hit < 100 && !E->IsDead(); ++Hit)
                    if (E->TakeDamage(P->ShotDamage, FDamageEvent(), PC, P) <= 0.f) break;
                if (!Test->TestTrue(FString::Printf(TEXT("%s reports death through TakeDamage"), *E->StableId.ToString()), E->IsDead())) return false;
                Test->TestEqual(TEXT("Dead guard ignores duplicate damage"), E->TakeDamage(999.f, FDamageEvent(), PC, P), 0.f);
                if (D->GetRemainingEnemies() > 0)
                    Test->TestFalse(TEXT("Partial roster cannot clear an encounter"), D->ClearedEncounters.Contains(Id));
            }
            return Test->TestEqual(TEXT("All expected enemies are accounted for"), Count, Expected) &&
                Test->TestTrue(TEXT("Final death clears the encounter"), D->ClearedEncounters.Contains(Id));
        }

        void CheckEncounterRestored(AOBMissionDirector* D, int32 Id, int32 Expected)
        {
            int32 Alive = 0;
            for (AOBEnemyCharacter* E : D->Enemies)
                if (IsValid(E) && E->EncounterId == Id && !E->IsDead())
                {
                    ++Alive;
                    Test->TestFalse(TEXT("Unentered encounter is dormant after Retry"), E->bActive);
                    Test->TestEqual(TEXT("Retry restores full guard health"), E->Health, E->bHeavy ? E->SentinelHealth : E->TrooperHealth);
                }
            Test->TestEqual(FString::Printf(TEXT("Retry restores all %d guards in encounter %d"), Expected, Id), Alive, Expected);
        }

        void DepleteLoadout(AOBPlayerCharacter* P)
        {
            P->RestoreLoadout(true, 3, 7);
            P->EndSpawnGrace();
            P->TakeDamage(27.f, FDamageEvent(), nullptr, P);
            Test->TestTrue(TEXT("Recovery fixture starts with depleted health"), P->Health < P->MaxHealth);
        }

        void CheckRecovery(AOBPlayerCharacter* P)
        {
            Test->TestFalse(TEXT("Checkpoint player is alive"), P->bDead);
            Test->TestEqual(TEXT("Checkpoint restores full health"), P->Health, P->MaxHealth);
            Test->TestTrue(TEXT("Checkpoint restores weapon ownership"), P->bWeaponOwned);
            Test->TestEqual(TEXT("Checkpoint restores full magazine"), P->MagazineAmmo, P->MagazineCapacity);
            Test->TestTrue(TEXT("Checkpoint restores recovery reserve"), P->ReserveAmmo >= 96);
            Test->TestFalse(TEXT("Checkpoint clears interaction state"), P->bInteractionActive);
            Test->TestFalse(TEXT("Checkpoint clears reload state"), P->bReloading);
        }

        bool CheckTerminalHold(AOBMissionDirector* D, AOBPlayerController* PC, AOBPlayerCharacter* P)
        {
            AOBInteractable* Terminal = Interaction.Get();
            if (!Test->TestTrue(TEXT("Terminal accepts valid interaction"), D->TryInteract(P, Terminal))) return false;
            P->bInteractHeld = true;
            D->UpdateHold(P, Terminal, .5f);
            Test->TestEqual(TEXT("Half-second terminal hold reaches one quarter"), P->InteractionProgress, .25f);
            P->bInteractHeld = false;
            D->UpdateHold(P, Terminal, .1f);
            Test->TestNull(TEXT("Releasing interaction cancels terminal hold"), D->HoldTarget.Get());
            Test->TestEqual(TEXT("Cancelled hold resets progress"), P->InteractionProgress, 0.f);
            if (!Test->TestTrue(TEXT("Released terminal hold can restart"), D->TryInteract(P, Terminal))) return false;
            P->bInteractHeld = true;
            D->UpdateHold(P, Terminal, .5f);
            PC->ShowScreen(TEXT("Pause"));
            D->UpdateHold(P, Terminal, 10.f);
            Test->TestEqual(TEXT("Paused hold does not accumulate supplied time"), P->InteractionProgress, .25f);
            PC->PerformAction(TEXT("Resume"));
            Test->TestNull(TEXT("Resume without a physical held key cancels hold"), D->HoldTarget.Get());
            if (!Test->TestTrue(TEXT("Resumed terminal hold can restart"), D->TryInteract(P, Terminal))) return false;
            P->bInteractHeld = true;
            D->UpdateHold(P, Terminal, 1.f);
            Test->TestFalse(TEXT("One second cannot complete two-second terminal"), D->bTerminalComplete);
            D->UpdateHold(P, Terminal, 1.f);
            Test->TestTrue(TEXT("Full terminal hold accepts override"), D->bTerminalComplete);
            Test->TestTrue(TEXT("Completed terminal becomes used"), Terminal->bUsed);
            Test->TestEqual(TEXT("Terminal completion starts security response"), D->ActiveEncounter, 4);
            return D->bTerminalComplete && D->ActiveEncounter == 4;
        }

        void CheckDisk(int32 Checkpoint, bool bCompleted)
        {
            UOBMissionSave* Saved = Cast<UOBMissionSave>(UGameplayStatics::LoadGameFromSlot(MissionSlot, 0));
            if (!Test->TestNotNull(TEXT("Test-owned mission slot loads through Unreal SaveGame"), Saved)) return;
            Test->TestTrue(TEXT("Saved mission passes validation"), Saved->IsValidSave());
            Test->TestEqual(TEXT("Disk checkpoint matches progression"), Saved->CheckpointId, Checkpoint);
            Test->TestEqual(TEXT("Disk completion matches progression"), Saved->bMissionCompleted, bCompleted);
        }

        void RestoreSession()
        {
            if (bRestored || !bOwnsMissionSlot || !Session.IsValid()) return;
            Session->MissionSave = OriginalSave.Get();
            Session->bSaveValid = bOriginalSaveValid;
            Session->SaveWarning = OriginalWarning;
            Session->bFrontEnd = true;
            bRestored = true;
        }

        void Cleanup(UWorld* World)
        {
            ReleaseCombatInput();
            if (AOBMissionDirector* D = World ? AOBMissionDirector::Get(World) : nullptr)
                D->SetActorTickEnabled(false);
            TArray<uint8> SettingsNow, SettingsDiskNow;
            if (Session.IsValid() && Session->Settings)
                UGameplayStatics::SaveGameToMemory(Session->Settings, SettingsNow);
            Test->TestTrue(TEXT("Menu navigation preserves in-memory preferences"), SettingsNow == OriginalSettings);
            const bool bSettingsExistNow = UGameplayStatics::DoesSaveGameExist(SettingsSlot, 0);
            Test->TestEqual(TEXT("Menu navigation preserves settings-file existence"), bSettingsExistNow, bSettingsExisted);
            if (bSettingsExisted && bSettingsExistNow)
            {
                UGameplayStatics::LoadDataFromSlot(SettingsDiskNow, SettingsSlot, 0);
                Test->TestTrue(TEXT("Menu navigation leaves settings bytes unchanged"), SettingsDiskNow == OriginalSettingsDisk);
            }
            RestoreSession();
            if (bOwnsMissionSlot && UGameplayStatics::DoesSaveGameExist(MissionSlot, 0))
            {
                const bool bDeleted = UGameplayStatics::DeleteGameInSlot(MissionSlot, 0);
                Test->TestTrue(TEXT("Cleanup removes only the test-created mission slot"), bDeleted);
                if (bDeleted) bOwnsMissionSlot = false;
            }
            else bOwnsMissionSlot = false;
            TravelWorld = World;
            if (Session.IsValid()) Session->ReturnToMenu();
            Advance(EStage::Cleanup);
        }
    };

    // Automation may end an existing PIE session before RunTest is invoked.
    // Own the complete PIE lifecycle instead of depending on a user's session.
#if WITH_EDITOR
    class FOwnedPIECommand final : public IAutomationLatentCommand
    {
    public:
        explicit FOwnedPIECommand(FAutomationTestBase* InTest)
            : Test(InTest), StageStarted(FPlatformTime::Seconds()) {}

        virtual ~FOwnedPIECommand() override
        {
            // Progression performs save cleanup before its game instance ends.
            Progression.Reset();
            if (bOwnsPIE && GEditor)
            {
                if (!HasPIEWorld() && !GEditor->PlayWorld && GEditor->GetPlaySessionRequest().IsSet())
                    GEditor->CancelRequestPlaySession();
                GEditor->RequestEndPlayMap();
            }
        }

        virtual bool Update() override
        {
            if (!GEditor)
            {
                Test->AddError(TEXT("Unreal editor is unavailable for the PIE integration test"));
                return true;
            }
            const double Elapsed = FPlatformTime::Seconds() - StageStarted;
            switch (Stage)
            {
            case EStage::WaitForIdle:
            {
                // Let automation finish any teardown it initiated. Do not stop
                // an unrelated user session if one is still running.
                if (HasPIEWorld() || GEditor->PlayWorld || GEditor->GetPlaySessionRequest().IsSet())
                {
                    if (Elapsed > 30.0)
                    {
                        Test->AddError(TEXT("Editor did not become idle for the owned PIE session"));
                        return true;
                    }
                    return false;
                }
                if (UGameplayStatics::DoesSaveGameExist(MissionSlot, 0))
                {
                    Test->AddError(TEXT("Mission progress appeared before PIE startup; refusing to replace it"));
                    return true;
                }
                PlaySettings.Reset(DuplicateObject<ULevelEditorPlaySettings>(
                    GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
                PlaySettings->SetPlayNetMode(PIE_Standalone);
                PlaySettings->SetPlayNumberOfClients(1);
                PlaySettings->SetRunUnderOneProcess(true);
                PlaySettings->bLaunchSeparateServer = false;
                PlaySettings->NetworkEmulationSettings.bIsNetworkEmulationEnabled = false;
                FRequestPlaySessionParams Params;
                Params.SessionDestination = EPlaySessionDestinationType::InProcess;
                Params.WorldType = EPlaySessionWorldType::PlayInEditor;
                Params.EditorPlaySettings = PlaySettings.Get();
                Params.GlobalMapOverride = MissionMap;
                Params.bAllowOnlineSubsystem = false;
                // GlobalMapOverride loads the mission into PIE without changing
                // the editor map or discarding any unsaved editor-world changes.
                bOwnsPIE = true;
                Advance(EStage::WaitForMain);
                GEditor->RequestPlaySession(Params);
                GEditor->StartQueuedPlaySessionRequest();
                return false;
            }
            case EStage::WaitForMain:
            {
                UWorld* World = FindCampaignWorld();
                UOBGameInstance* Session = World ? Cast<UOBGameInstance>(World->GetGameInstance()) : nullptr;
                AOBPlayerController* PC = World ? Cast<AOBPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
                AOBMissionDirector* D = World ? AOBMissionDirector::Get(World) : nullptr;
                if (Session && PC && D && D->Player && Session->bFrontEnd && PC->MenuScreen == TEXT("Main"))
                {
                    if (UGameplayStatics::DoesSaveGameExist(MissionSlot, 0) || Session->HasValidSave())
                    {
                        Test->AddError(TEXT("Owned PIE found mission progress; refusing to run NewMission"));
                        StopPIE();
                        return false;
                    }
                    Test->AddInfo(TEXT("Owned single-client PIE reached Main; beginning mission state checks."));
                    Progression = MakeUnique<FProgressionCommand>(Test, Session);
                    Advance(EStage::RunMission);
                    return false;
                }
                if (Elapsed > 60.0)
                {
                    Test->AddError(TEXT("Owned PIE failed to reach the mission Main screen within 60 seconds"));
                    StopPIE();
                }
                return false;
            }
            case EStage::RunMission:
                if (Progression->Update()) StopPIE();
                return false;
            case EStage::StopPIE:
                if (!HasPIEWorld() && !GEditor->PlayWorld && !GEditor->GetPlaySessionRequest().IsSet())
                {
                    bOwnsPIE = false;
                    Test->AddInfo(TEXT("Owned PIE session ended; editor map and play preferences were not replaced."));
                    return true;
                }
                if (Elapsed > 30.0)
                {
                    Test->AddError(TEXT("Owned PIE did not finish shutting down within 30 seconds"));
                    return true;
                }
                return false;
            }
            return false;
        }

    private:
        enum class EStage { WaitForIdle, WaitForMain, RunMission, StopPIE };
        FAutomationTestBase* Test;
        TStrongObjectPtr<ULevelEditorPlaySettings> PlaySettings;
        TUniquePtr<FProgressionCommand> Progression;
        EStage Stage = EStage::WaitForIdle;
        double StageStarted;
        bool bOwnsPIE = false;

        static bool HasPIEWorld()
        {
            if (GEngine)
                for (const FWorldContext& Context : GEngine->GetWorldContexts())
                    if (Context.World() && Context.World()->WorldType == EWorldType::PIE) return true;
            return false;
        }

        void Advance(EStage Next) { Stage = Next; StageStarted = FPlatformTime::Seconds(); }

        void StopPIE()
        {
            Progression.Reset();
            if (!HasPIEWorld() && !GEditor->PlayWorld && GEditor->GetPlaySessionRequest().IsSet())
                GEditor->CancelRequestPlaySession();
            GEditor->RequestEndPlayMap();
            Advance(EStage::StopPIE);
        }
    };
#endif
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBMissionPIETest,
    "OrbitBreakout.Mission.PIE.ProgressionAndRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOBMissionPIETest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("PIE integration requires the Unreal editor"), GEditor)) return false;
    if (UGameplayStatics::DoesSaveGameExist(OBMissionTests::MissionSlot, 0))
    {
        AddError(TEXT("Refusing to run: mission progress exists. Use an isolated fresh test project/save directory; this test never replaces user progress."));
        return false;
    }
    AddInfo(TEXT("PIE state test uses targeted pawn positioning, simulated aim/fire for one guard, player-instigated damage for remaining guards, and Director hold APIs; timers, triggers, world reloads and ending tick naturally."));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<OBMissionTests::FOwnedPIECommand>(this));
    return true;
}

#else
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBPackagedCampaignTest,
    "OrbitBreakout.Package.Campaign",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FOBPackagedCampaignTest::RunTest(const FString& Parameters)
{
    FString UserDir;
    if (!FParse::Param(FCommandLine::Get(), TEXT("OBIsolatedCampaignTest")) ||
        !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || UserDir.IsEmpty() ||
        !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()),
                                  FPaths::ConvertRelativePathToFull(UserDir)))
    {
        AddError(TEXT("The package campaign test needs an explicit isolated -UserDir and -OBIsolatedCampaignTest."));
        return false;
    }
    UWorld* World = OBMissionTests::FindCampaignWorld();
    auto* Session = World ? Cast<UOBGameInstance>(World->GetGameInstance()) : nullptr;
    if (!Session || !Session->bFrontEnd || Session->HasValidSave() ||
        UGameplayStatics::DoesSaveGameExist(OBMissionTests::MissionSlot, 0))
    {
        AddError(TEXT("The package campaign test requires the main menu and a fresh mission slot."));
        return false;
    }
    FAutomationTestFramework::Get().EnqueueLatentCommand(
        MakeShared<OBMissionTests::FProgressionCommand>(this, Session));
    return true;
}

namespace OBMissionTests
{
    bool StageNextCheckpoint(FAutomationTestBase& Test)
    {
        FString Next;
        if (!FParse::Value(FCommandLine::Get(), TEXT("OBNextCheckpoint="), Next)) return true;
        TArray<uint8> Bytes;
        if (!Test.TestTrue(TEXT("Recorded gameplay snapshot exists"), UGameplayStatics::LoadDataFromSlot(Bytes, TEXT("Review_") + Next, 0)))
            return false;
        const auto* Disk = Cast<UOBMissionSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
        if (!Test.TestTrue(TEXT("Recorded checkpoint matches requested identity"), Disk && Disk->IsValidSave() && Disk->CheckpointKey.ToString() == Next))
            return false;
        return Test.TestTrue(TEXT("Next process receives the archived save"), UGameplayStatics::SaveDataToSlot(Bytes, MissionSlot, 0));
    }

    class FColdResume final : public IAutomationLatentCommand
    {
    public:
        FColdResume(FAutomationTestBase& InTest, UOBGameInstance& InSession)
            : Test(InTest), Session(&InSession), Deadline(FPlatformTime::Seconds()+65.0) {}

        bool Update() override
        {
            if (!Session.IsValid() || FPlatformTime::Seconds() > Deadline)
            {
                Test.AddError(TEXT("Cold Continue did not restore playable control in time"));
                return true;
            }
            UWorld* W = FindCampaignWorld(Session.Get());
            auto* D = W ? AOBMissionDirector::Get(W) : nullptr;
            auto* PC = W ? Cast<AOBPlayerController>(UGameplayStatics::GetPlayerController(W,0)) : nullptr;
            if (!D || !PC || !D->Player || !D->IsGameplayActive() || PC->IsMenuOpen()) return false;
            auto* P = D->Player.Get();
            const auto* S = Session->MissionSave.Get();
            const int32 Id = S->CheckpointId;
            FVector Spawn = D->StartLocation;
            if (S->ChapterId == 2)
            {
                auto* L = Cast<AOBLevel2Director>(D);
                if (!Test.TestNotNull(TEXT("Tunnel director restored"),L)) return true;
                const FVector Entries[] = {Spawn,L->JunctionCheckpoint,L->PowerCheckpoint,L->FinaleCheckpoint};
                Spawn = Entries[Id];
                Test.TestEqual(TEXT("Tunnel checkpoint"),L->Checkpoint,Id);
                Test.TestEqual(TEXT("Saved rifle magazine"),P->GetArcMagazineAmmo(),S->ArcMagazine);
                Test.TestEqual(TEXT("Saved HELIX magazine"),P->GetHelixMagazineAmmo(),S->HelixMagazine);
                Test.TestEqual(TEXT("Auxiliary power"),L->bAuxPowerRestored,S->bAuxPowerRestored);
                Test.TestEqual(TEXT("Warden gate"),L->bWardenDefeated,S->bWardenDefeated);
            }
            else
            {
                const FVector Entries[] = {Spawn,D->CargoCheckpointLocation,D->ReactorCheckpointLocation};
                Spawn = Entries[Id];
                Test.TestEqual(TEXT("Chapter 1 weapon availability"),P->bWeaponOwned,Id>0);
                Test.TestTrue(TEXT("Chapter 1 phase"),D->Phase == (Id==2 ? EOBPhase::Reactor : Id==1 ? EOBPhase::Cargo : EOBPhase::Cell));
            }
            Test.TestTrue(TEXT("Checkpoint places the capsule at the authored spawn"),
                FVector::Dist2D(Spawn,P->GetActorLocation())<60 && FMath::Abs(Spawn.Z-P->GetActorLocation().Z)<115);
            Test.TestTrue(TEXT("Wraith owns the view and controls"),PC->GetPawn()==P && PC->GetViewTarget()==P);
            Test.TestTrue(TEXT("Aim, traversal and cockpit released"),!P->bAiming && P->PilotView->Alpha==0.f &&
                !P->OccupiedMech.IsValid() && !P->Parkour()->IsBusy());
            Test.AddInfo(TEXT("Cold Continue restored ") + S->CheckpointKey.ToString());
            StageNextCheckpoint(Test);
            return true;
        }
    private:
        FAutomationTestBase& Test;
        TWeakObjectPtr<UOBGameInstance> Session;
        double Deadline;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBColdBootTest,"OrbitBreakout.Package.ColdResume",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FOBColdBootTest::RunTest(const FString&)
{
    FString Root;
    if (!FParse::Param(FCommandLine::Get(),TEXT("OBIsolatedCampaignTest")) ||
        !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),Root) || !FPaths::GetCleanFilename(Root).StartsWith(TEXT("OrbitPilotReview-")) ||
        !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()),FPaths::ConvertRelativePathToFull(Root)))
    {
        AddError(TEXT("Cold-boot checks require an isolated review profile"));
        return false;
    }
    auto* W = OBMissionTests::FindCampaignWorld();
    auto* GI = W ? Cast<UOBGameInstance>(W->GetGameInstance()) : nullptr;
    auto* PC = W ? Cast<AOBPlayerController>(UGameplayStatics::GetPlayerController(W,0)) : nullptr;
    if (!TestTrue(TEXT("Fresh process reached Main"),GI && PC && GI->bFrontEnd && PC->MenuScreen==TEXT("Main"))) return false;
    FString Expected;
    if (!FParse::Value(FCommandLine::Get(),TEXT("OBExpectedCheckpoint="),Expected))
        return OBMissionTests::StageNextCheckpoint(*this);
    if (!TestTrue(TEXT("Fresh process read the expected checkpoint from disk"),
        GI->HasValidSave() && GI->MissionSave->CheckpointKey.ToString()==Expected)) return false;
    PC->PerformAction(TEXT("Continue"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<OBMissionTests::FColdResume>(*this,*GI));
    return true;
}

// Send events at the same Slate entry point as the Apple controller backend.
// Calling PlayerController::InputKey directly would skip the focus regression.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBGamepadFocusTest,"OrbitBreakout.Package.GamepadFocus",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FOBGamepadFocusTest::RunTest(const FString&)
{
    FString Root;
    if (!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),Root) ||
        !FPaths::GetCleanFilename(Root).StartsWith(TEXT("OrbitPilotReview-")) ||
        !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()),FPaths::ConvertRelativePathToFull(Root)))
    {
        AddError(TEXT("Gamepad focus test requires a temporary review profile"));
        return false;
    }
    auto* W=OBMissionTests::FindCampaignWorld();
    auto* PC=W ? Cast<AOBPlayerController>(W->GetFirstPlayerController()) : nullptr;
    if (!TestTrue(TEXT("Main menu ready"),PC && PC->MenuScreen==TEXT("Main") && PC->GetLocalPlayer())) return false;
    const FPlatformUserId User=PC->GetLocalPlayer()->GetPlatformUserId();
    const FInputDeviceId Device=IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(User);
    const auto Tap=[User,Device](FKey Key)
    {
        auto& Slate=FSlateApplication::Get();
        Slate.OnControllerButtonPressed(Key.GetFName(),User,Device,false);
        Slate.OnControllerButtonReleased(Key.GetFName(),User,Device,false);
    };
    const TWeakObjectPtr<AOBPlayerController> Player=PC;
    auto View=W->GetGameViewport()->GetGameViewportWidget();
    if (!TestTrue(TEXT("Main has viewport focus before a mouse click"), View.IsValid() && View->HasAnyUserFocus().IsSet())) return false;
    PC->SetMouseLocation(2,2);
    FSlateApplication::Get().ClearAllUserFocus(EFocusCause::Cleared);
    PC->ShowScreen(TEXT("Controls"));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(.25f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this,Player,View,Tap]()
    {
        if (!TestTrue(TEXT("Menu transition restores viewport focus"),Player.IsValid() && View->HasAnyUserFocus().IsSet())) return true;
        Tap(EKeys::Gamepad_FaceButton_Right);
        TestEqual(TEXT("Circle returns to Main through Slate"),Player->MenuScreen,FName(TEXT("Main")));
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(.25f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this,Player,Tap]()
    {
        if (!Player.IsValid() || Player->MenuScreen!=TEXT("Main")) return true;
        Tap(EKeys::Gamepad_DPad_Up);
        Tap(EKeys::Gamepad_LeftStick_Up);
        Tap(EKeys::Gamepad_FaceButton_Bottom);
        TestEqual(TEXT("D-pad, left stick and Cross select Controls"),Player->MenuScreen,FName(TEXT("Controls")));
        return true;
    }));
    return true;
}
#endif // WITH_EDITOR
#endif // WITH_DEV_AUTOMATION_TESTS
