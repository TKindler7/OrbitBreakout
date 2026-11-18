#include "../OBParkourMovement.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "../OBLevel2.h"
#include "../OBMech.h"
#include "../OBPlayerCharacter.h"
#include "../OBPilotView.h"
#include "../OBUI.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBPilotUpgradeTest,"OrbitBreakout.Pilot.CollisionCameraAndPossession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOBPilotUpgradeTest::RunTest(const FString&)
{
    auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    UWorld* W = UWorld::CreateWorld(EWorldType::Game,false,
        MakeUniqueObjectName(GetTransientPackage(),UWorld::StaticClass(),TEXT("OBParkourTest")),GetTransientPackage());
    Context.SetCurrentWorld(W);
    W->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { GEngine->ShutdownWorldNetDriver(W); W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
    auto Box = [W](FVector Center,FVector Half)
    {
        auto* A = W->SpawnActor<AActor>();
        auto* B = NewObject<UBoxComponent>(A);
        A->SetRootComponent(B); B->SetBoxExtent(Half); B->SetCollisionProfileName(TEXT("BlockAll"));
        B->RegisterComponent(); A->SetActorLocation(Center);
        return B;
    };
    Box(FVector(0,0,-20),FVector(2500,2500,20));
    auto* D = W->SpawnActor<AOBLevel2Director>();
    auto* P = W->SpawnActor<AOBPlayerCharacter>(FVector(0,0,98),FRotator::ZeroRotator);
    auto* PC = W->SpawnActorDeferred<AOBPlayerController>(AOBPlayerController::StaticClass(),FTransform::Identity);
    if (!TestNotNull(TEXT("Director"),D) || !TestNotNull(TEXT("Pilot"),P) || !TestNotNull(TEXT("Controller"),PC)) return false;
    PC->SetAsLocalPlayerController(); PC->FinishSpawning(FTransform::Identity); PC->Possess(P);
    D->Player = P; D->bReady = true; D->MissionState = EOBLevel2State::DefeatWarden;
    D->CurrentZone = 6; P->Director = D;
    P->RestoreCampaignLoadout(true,11,52,true,9,22,EOBWeaponType::Arc9);
    auto* M = P->Parkour();
    const float Dt = 1.f / 60.f;
    M->SetMovementMode(MOVE_Walking); M->Velocity = FVector(610,0,0);
    P->StartSprint(); P->StartSlide();
    TestTrue(TEXT("Sprint slide crouches the collision capsule"),M->Traversal==EOBTraversal::Slide && P->bIsCrouched);
    auto* Roof = Box(P->GetActorLocation()+FVector(0,0,78),FVector(100,100,10));
    P->StopSprint(); M->UpdateCharacterStateBeforeMovement(Dt);
    TestTrue(TEXT("Slide exit retains the short capsule under cover"),P->bIsCrouched);
    Roof->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    M->UpdateCharacterStateBeforeMovement(Dt);
    TestFalse(TEXT("Capsule stands after clearance"),P->bIsCrouched);
    M->Velocity = FVector(100,0,0); M->SetMovementMode(MOVE_Falling);
    M->PressJump(); M->UpdateCharacterStateBeforeMovement(Dt);
    TestEqual(TEXT("Coyote jump supplies the configured impulse"),M->Velocity.Z,double(M->JumpZVelocity));
    M->ReleaseJump();
    TestTrue(TEXT("Early release shortens rising jump"),M->Velocity.Z<=200.);
    M->PressJump(); M->UpdateCharacterStateBeforeMovement(Dt);
    TestTrue(TEXT("Second air press cannot boost the same fall"),M->Velocity.Z<=200.);
    M->SetMovementMode(MOVE_Walking); M->UpdateCharacterStateBeforeMovement(Dt);
    TestTrue(TEXT("A buffered press is consumed on landing"),M->IsFalling() && M->Velocity.Z>=M->JumpZVelocity);
    M->ResetTraversal(); M->SetMovementMode(MOVE_Walking); P->SetActorLocation(FVector(0,0,98));
    M->Acceleration = FVector(2000,0,0);
    auto* Ledge = Box(FVector(143,0,40),FVector(64,120,40));
    TestFalse(TEXT("An unmarked story obstruction cannot be mantled"),M->TryMantle());
    Ledge->ComponentTags.Add(TEXT("OB_Mantle"));
    auto* Ceiling = Box(FVector(110,0,215),FVector(100,150,8));
    TestFalse(TEXT("Mantle refuses insufficient headroom"),M->TryMantle());
    Ceiling->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (TestTrue(TEXT("Tagged low ledge admits a swept mantle"),M->TryMantle()))
    {
        for (int I=0; I<60 && M->Traversal==EOBTraversal::Mantle; ++I) M->PhysCustom(Dt,0);
        TestTrue(TEXT("Capsule reaches the top on the correct side"),P->GetActorLocation().Z>174 && P->GetActorLocation().X>110);
    }
    auto* Wall = Box(FVector(0,62,225),FVector(540,10,225));
    P->SetActorLocation(FVector(-340,0,205)); M->SetMovementMode(MOVE_Walking); M->SetMovementMode(MOVE_Falling);
    M->Velocity = FVector(600,0,0); M->Acceleration = FVector(2000,0,0); P->StartSprint();
    M->UpdateCharacterStateBeforeMovement(Dt);
    TestFalse(TEXT("An unmarked surface cannot start a wall run"),M->Traversal==EOBTraversal::WallRun);
    Wall->ComponentTags.Add(TEXT("OB_WallRun")); M->UpdateCharacterStateBeforeMovement(Dt);
    if (TestTrue(TEXT("Tagged parallel surface accepts the run"),M->Traversal==EOBTraversal::WallRun))
    {
        M->PhysCustom(Dt,0); M->PressJump();
        TestTrue(TEXT("Kick detaches away from the wall"),M->IsFalling() && M->Velocity.Y < -300 && M->Velocity.Z > 400);
        M->Velocity = FVector(600,0,0); M->UpdateCharacterStateBeforeMovement(Dt);
        TestFalse(TEXT("Repeat wall runs require landing"),M->Traversal==EOBTraversal::WallRun);
    }
    P->CancelActions(); P->StartAim(); P->PilotView->UpdateView(*P,.12f,true);
    TestTrue(TEXT("ADS blends through an intermediate frame"),P->PilotView->InTransition());
    P->PilotView->UpdateView(*P,.2f,true);
    TestEqual(TEXT("ARC precision FOV"),P->FollowCamera->FieldOfView,68.f);
    P->EquipWeapon(EOBWeaponType::Helix12); P->PilotView->UpdateView(*P,Dt,true);
    TestEqual(TEXT("HELIX uses its own sight profile"),P->FollowCamera->FieldOfView,62.f);
    PC->SetControlRotation(FRotator::ZeroRotator);
    const FVector Eye=P->GetActorLocation()+FVector(0,0,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-24.f);
    auto* Door=Box(Eye+FVector(43,0,0),FVector(3,38,75));
    for (const auto Weapon : {EOBWeaponType::Arc9,EOBWeaponType::Helix12})
    {
        P->EquipWeapon(Weapon);
        Door->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        P->PilotView->UpdateView(*P,1.f,true);
        TestFalse(TEXT("Close cover hides the weapon sight"),P->SightRifle->IsVisible());
        Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        const FVector SightOffset = Weapon == EOBWeaponType::Arc9 ? P->PilotView->ArcSightOffset : P->PilotView->HelixSightOffset;
        const FBox RifleBounds = P->SightRifle->GetStaticMesh()->GetBoundingBox();
        TestTrue(TEXT("Reticle clears rifle geometry and camera clears stock"),
            SightOffset.Z + RifleBounds.Max.Z < 0.f && SightOffset.X + RifleBounds.Min.Y > 10.f);
        P->bReloading=true; P->ReloadProgress=.5f;
        P->PilotView->UpdateView(*P,Dt,true);
        TestTrue(TEXT("Clear sight dips during reload"),P->SightRifle->IsVisible() && P->SightRifle->GetRelativeLocation().Z < SightOffset.Z - 7.f);
        P->bReloading=false;
        M->SetMovementMode(MOVE_Falling); M->SetMovementMode(MOVE_Walking);
        P->PilotView->UpdateView(*P,Dt,true);
        TestTrue(TEXT("Landing restores sight alignment"),P->SightRifle->GetRelativeLocation().Equals(SightOffset,0.1f));
    }
    auto* GI=NewObject<UOBGameInstance>(GEngine);
    GI->Settings=NewObject<UOBSettings>(GI); GI->bFrontEnd=false;
    W->SetGameInstance(GI);
    M->Traversal=EOBTraversal::WallRun; M->WallNormal=P->GetActorRightVector();
    for (int32 Motion=0;Motion<=2;++Motion)
    {
        GI->Settings->TraversalMotion=Motion; GI->Settings->WallRoll=8.f;
        P->PilotView->UpdateView(*P,1.f,false);
        TestTrue(TEXT("Motion setting scales wall roll"), FMath::IsNearlyEqual(P->FollowCamera->GetRelativeRotation().Roll,double(4*Motion),.01));
    }
    GI->Settings->WallRoll=0.f;
    P->PilotView->UpdateView(*P,1.f,false);
    TestTrue(TEXT("Zero roll suppresses tilt"),FMath::Abs(P->FollowCamera->GetRelativeRotation().Roll)<.01);
    M->ResetTraversal();
    P->CancelActions();
    TestEqual(TEXT("Interruption restores third person"),P->FollowCamera->FieldOfView,85.f);
    P->SetActorLocation(FVector(-800,-500,98)); M->SetMovementMode(MOVE_Walking);
    auto* Loader = W->SpawnActor<AOBMech>(FVector(-640,-500,152),FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Articulated loader spawns"),Loader)) return false;
    for (auto* Part : {Loader->Thighs[0].Get(),Loader->Shins[0].Get(),Loader->Feet[0].Get()})
        TestTrue(TEXT("Load-bearing assemblies use authored meshes"),Part && Part->GetStaticMesh());
    Loader->Chapter = D; Loader->Pad = Loader->GetActorLocation(); D->WardenMech=Loader;
    P->Health=63.f; const int Magazine=P->MagazineAmmo;
    if (TestTrue(TEXT("Chassis admits pilot at the live dock"),Loader->Board(*P)))
    {
        TestTrue(TEXT("One controller possesses the chassis"),PC->GetPawn()==Loader && !P->GetController());
        Loader->Fire(); TestTrue(TEXT("Pulse raises independent heat"),Loader->Heat>0.f && P->MagazineAmmo==Magazine);
        Loader->DamageArmor(500.f);
        TestTrue(TEXT("Armor depletion ejects to preserved pilot"),PC->GetPawn()==P && !P->OccupiedMech.IsValid());
        TestTrue(TEXT("Pilot loadout and health survive"),P->Health==63.f && P->MagazineAmmo==Magazine);
        TestFalse(TEXT("Destroyed chassis cannot be boarded again"),Loader->Board(*P));
    }
    return true;
}
#endif
