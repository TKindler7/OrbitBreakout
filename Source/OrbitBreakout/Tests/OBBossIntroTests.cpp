#include "../OBEnemyCharacter.h"
#include "../OBPlayerCharacter.h"
#include "../OBCore.h"
#include "../OBUI.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include <type_traits>

namespace OBBossIntroTests
{
    struct FWorldFixture
    {
        UWorld* World=nullptr;
        FWorldFixture()
        {
            if(!GEngine)return;
            FWorldContext& Context=GEngine->CreateNewWorldContext(EWorldType::Game);
            World=UWorld::CreateWorld(EWorldType::Game,false,
                MakeUniqueObjectName(GetTransientPackage(),UWorld::StaticClass(),TEXT("OBBossIntroTest")),GetTransientPackage());
            Context.SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
        }
        ~FWorldFixture()
        {
            if(!World)return;
            GEngine->ShutdownWorldNetDriver(World);
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
        template<typename T> T* Pawn(FVector Location,bool bHeavy=false)
        {
            const FTransform Pose(FRotator(0,180,0),Location);
            T* Actor=World->SpawnActorDeferred<T>(T::StaticClass(),Pose,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if(!Actor)return nullptr;
            Actor->AutoPossessAI=EAutoPossessAI::Disabled;
            Actor->AutoPossessPlayer=EAutoReceiveInput::Disabled;
            if constexpr(std::is_same_v<T,AOBEnemyCharacter>)Actor->bHeavy=bHeavy;
            Actor->FinishSpawning(Pose);
            return Actor;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBBossIntroTest,"OrbitBreakout.Boss.IntroLockCameraAndCleanup",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)

bool FOBBossIntroTest::RunTest(const FString& Parameters)
{
    // No GameInstance, BeginPlay, mission travel or disk save access. This
    // fixture exercises actual controller/camera and character action gates.
    OBBossIntroTests::FWorldFixture Fixture;
    if(!TestNotNull(TEXT("Isolated entrance world exists"),Fixture.World))return false;
    AOBMissionDirector* Director=Fixture.World->SpawnActor<AOBMissionDirector>();
    AOBPlayerCharacter* Player=Fixture.Pawn<AOBPlayerCharacter>(FVector(-700,0,96));
    AOBPlayerController* PC=Fixture.World->SpawnActorDeferred<AOBPlayerController>(AOBPlayerController::StaticClass(),FTransform::Identity);
    if (PC) { PC->SetAsLocalPlayerController(); PC->FinishSpawning(FTransform::Identity); }
    AOBEnemyCharacter* Boss=Fixture.Pawn<AOBEnemyCharacter>(FVector(0,0,112),true);
    if(!TestNotNull(TEXT("Director exists"),Director)||!TestNotNull(TEXT("Player exists"),Player)||
       !TestNotNull(TEXT("Controller exists"),PC)||!TestNotNull(TEXT("Boss exists"),Boss))return false;
    Director->bInitialized=true;Director->Player=Player;Director->Phase=EOBPhase::Reactor;
    Player->Director=Director;Player->bWeaponOwned=true;Player->MagazineAmmo=24;Player->ReserveAmmo=96;
    Player->EndSpawnGrace();Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    PC->Possess(Player);PC->SetViewTarget(Player);
    if(!TestNotNull(TEXT("Controller has a camera manager"),PC->PlayerCameraManager.Get()))return false;
    // The isolated controller has no viewport client to drive the camera update.
    PC->PlayerCameraManager->bUseClientSideCameraUpdates=false;
    Boss->Director=Director;Boss->Target=Player;Boss->Health=Boss->SentinelHealth;Boss->EncounterId=5;Boss->StableId=TEXT("TEST_INTRO_BULL");
    Boss->HeavySkeletalMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/SciFi_Beast06/Mesh/SK_SciFi_Beast06_Skin1.SK_SciFi_Beast06_Skin1"));
    Boss->HeavyIntroAnimation=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/SciFi_Beast06/Animations/Anim_SciFi_Beast06_Rage.Anim_SciFi_Beast06_Rage"));
    Boss->HeavyIdleAnimation=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/SciFi_Beast06/Animations/Anim_SciFi_Beast06_Idle1.Anim_SciFi_Beast06_Idle1"));
    if(!TestNotNull(TEXT("Authored Bull mesh loads"),Boss->HeavySkeletalMesh.Get())||
       !TestNotNull(TEXT("Authored Rage animation loads"),Boss->HeavyIntroAnimation.Get())||
       !TestNotNull(TEXT("Authored idle animation loads"),Boss->HeavyIdleAnimation.Get()))return false;
    Boss->ConfigureCharacterVisuals();
    Boss->Activate();
    TestTrue(TEXT("Activating the Bull starts its entrance once"),Director->IsBossIntroActive()&&Boss->HasPlayedBossIntro());
    TestTrue(TEXT("The Bull selects its authored entrance animation"),Boss->IsBossIntroPlaying()&&Boss->CurrentHeavyAnimation==Boss->HeavyIntroAnimation);
    UAnimSingleNodeInstance* Instance=Boss->GetMesh()->GetSingleNodeInstance();
    if(!TestNotNull(TEXT("Intro animation has a single-node player"),Instance))return false;
    {
        TestFalse(TEXT("The entrance animation never loops"),Instance->IsLooping());
        const UAnimSequence* Clip=Cast<UAnimSequence>(Boss->HeavyIntroAnimation);
        if(Clip)TestEqual(TEXT("The full clip is normalized to the entrance duration"),Instance->GetPlayRate(),Clip->GetPlayLength()/Boss->BossIntroSeconds);
    }
    TestFalse(TEXT("Player gameplay is locked during the entrance"),Player->IsGameplayActive());
    TestFalse(TEXT("Damage and enemy attacks are locked"),Director->IsCombatAllowed());
    TestTrue(TEXT("Player physical movement is frozen"),Player->GetCharacterMovement()->MovementMode==MOVE_None);
    TestTrue(TEXT("Bull physical movement is frozen"),Boss->GetCharacterMovement()->MovementMode==MOVE_None);
    TestNotNull(TEXT("A close-up camera was created"),Director->BossIntroCamera.Get());
    TestTrue(TEXT("The camera blends toward the Bull close-up"),PC->GetViewTarget()==Director->BossIntroCamera);
    Player->MoveForward(1.f);Player->StartFire();Player->StartInteract();
    TestTrue(TEXT("Movement input cannot accumulate during the entrance"),Player->GetPendingMovementInputVector().IsNearlyZero());
    TestEqual(TEXT("Attempted fire consumes no ammunition"),Player->MagazineAmmo,24);
    TestFalse(TEXT("Attempted interaction cannot begin"),Player->bInteractHeld);
    Player->DamagePlayer(50.f,Boss->GetActorLocation());
    TestEqual(TEXT("The player cannot take damage during the close-up"),Player->Health,100.f);
    TestEqual(TEXT("The Bull cannot be damaged during its entrance"),Boss->TakeDamage(500.f,FDamageEvent(),PC,Player),0.f);
    Boss->Perceive();Boss->Tick(.2f);
    TestFalse(TEXT("The boss cannot acquire an attack during the entrance"),Boss->bHasAttackSlot);
    Boss->Activate();
    TestEqual(TEXT("Repeated activation does not restart the timeline"),Director->GetBossIntroElapsed(),0.f);

    PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::P,IE_Pressed,1.f));
    TestEqual(TEXT("Pause is reachable while gameplay is locked"),PC->MenuScreen,FName(TEXT("Pause")));
    Director->TickBossIntro(5.f);
    TestEqual(TEXT("The pause menu freezes the entrance timeline"),Director->GetBossIntroElapsed(),0.f);
    PC->HideScreen();
    TestTrue(TEXT("Resuming the menu preserves the entrance movement lock"),Player->GetCharacterMovement()->MovementMode==MOVE_None);
    APlayerState* PauseOwner=Fixture.World->SpawnActor<APlayerState>();
    Fixture.World->GetWorldSettings()->SetPauserPlayerState(PauseOwner);
    Director->TickBossIntro(5.f);
    TestEqual(TEXT("World pause also freezes the entrance timeline"),Director->GetBossIntroElapsed(),0.f);
    Fixture.World->GetWorldSettings()->SetPauserPlayerState(nullptr);

    PC->PlayerCameraManager->UpdateCamera(.5f);
    Director->TickBossIntro(Boss->BossIntroSeconds);
    TestTrue(TEXT("Roar completion starts camera return"),Director->IsBossIntroReturning());
    TestFalse(TEXT("The roar does not loop through camera return"),Boss->IsBossIntroPlaying());
    TestFalse(TEXT("Combat stays locked throughout camera return"),Director->IsCombatAllowed());
    Player->DamagePlayer(50.f,Boss->GetActorLocation());
    TestEqual(TEXT("Camera return remains invulnerable"),Player->Health,100.f);
    Director->TickBossIntro(.7f);
    TestTrue(TEXT("An unfinished actual camera blend cannot unlock gameplay"),Director->IsBossIntroActive());
    PC->PlayerCameraManager->UpdateCamera(.7f);
    Director->TickBossIntro(.01f);
    TestFalse(TEXT("Completed camera return unlocks the game"),Director->IsBossIntroActive());
    TestTrue(TEXT("The previous player view is restored"),PC->GetViewTarget()==Player);
    TestTrue(TEXT("Player movement mode is restored"),Player->GetCharacterMovement()->MovementMode==MOVE_Walking);
    TestTrue(TEXT("The player can act after the introduction"),Player->IsGameplayActive());
    TestTrue(TEXT("The boss grants at least 0.6 seconds before attacking"),Boss->NextAttackTime>=Fixture.World->GetTimeSeconds()+.59f);
    TestFalse(TEXT("A completed entrance cannot play again on the same boss"),Director->BeginBossIntro(Boss));

    int32 FreshIndex=0;
    auto FreshBoss=[&]()
    {
        AOBEnemyCharacter* Fresh=Fixture.Pawn<AOBEnemyCharacter>(FVector(0,600.f*++FreshIndex,112),true);
        Fresh->Director=Director;Fresh->Target=Player;Fresh->Health=Fresh->SentinelHealth;
        Fresh->Activate();
        return Fresh;
    };
    AOBEnemyCharacter* Cancelled=FreshBoss();
    if(!TestNotNull(TEXT("Return-cancellation scenario owns a real camera"),Director->BossIntroCamera.Get()))return false;
    PC->PlayerCameraManager->UpdateCamera(.5f);
    Director->TickBossIntro(Cancelled->BossIntroSeconds);
    TestTrue(TEXT("Fresh checkpoint-life actors can play their entrance"),Director->IsBossIntroReturning());
    Director->CancelBossIntro();
    TestFalse(TEXT("Cancellation during return releases the lock"),Director->IsBossIntroActive());
    TestTrue(TEXT("Cancellation during return restores the view immediately"),PC->GetViewTarget()==Player);
    TestTrue(TEXT("Cancellation clears any pending owned camera blend"),PC->PlayerCameraManager->PendingViewTarget.Target==nullptr&&PC->PlayerCameraManager->BlendTimeToGo==0.f);

    AOBEnemyCharacter* Interrupted=FreshBoss();
    if(!TestNotNull(TEXT("Death-cleanup scenario owns a real camera"),Director->BossIntroCamera.Get()))return false;
    Interrupted->Die(false);
    TestFalse(TEXT("Boss destruction interrupts its entrance safely"),Director->IsBossIntroActive());
    TestTrue(TEXT("Interrupted entrance restores player movement"),Player->GetCharacterMovement()->MovementMode==MOVE_Walking);
    FreshBoss();
    if(!TestNotNull(TEXT("Later-camera scenario owns a real close-up camera"),Director->BossIntroCamera.Get()))return false;
    ACameraActor* LaterCamera=Fixture.World->SpawnActor<ACameraActor>();
    PC->SetViewTargetWithBlend(LaterCamera,.5f);
    Director->CancelBossIntro();
    TestTrue(TEXT("Cleanup does not overwrite a later camera transition"),PC->GetViewTarget()==LaterCamera);
    PC->PlayerCameraManager->UpdateCamera(.6f);
    PC->SetViewTarget(Player);
    FreshBoss();
    if(!TestNotNull(TEXT("Session-end scenario owns a real close-up camera"),Director->BossIntroCamera.Get()))return false;
    Director->EndPlay(EEndPlayReason::EndPlayInEditor);
    TestFalse(TEXT("Ending the session releases the entrance lock"),Director->IsBossIntroActive());
    TestTrue(TEXT("Ending the session restores the previous camera"),PC->GetViewTarget()==Player);
    return true;
}

#endif
