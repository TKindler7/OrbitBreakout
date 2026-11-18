#include "OBPilotView.h"
#include "OBPlayerCharacter.h"
#include "OBPilotTuning.h"
#include "OBParkourMovement.h"
#include "OBCore.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"

UOBPilotView::UOBPilotView()
{
    ArcProfile = FSoftObjectPath(TEXT("/Game/OrbitBreakout/Tuning/DA_Arc9Sight.DA_Arc9Sight"));
    HelixProfile = FSoftObjectPath(TEXT("/Game/OrbitBreakout/Tuning/DA_Helix12Sight.DA_Helix12Sight"));
}
void UOBPilotView::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Arc = ArcProfile.LoadSynchronous()) { ArcSightOffset = Arc->Offset; ArcFOV = Arc->FieldOfView; }
    if (auto* Helix = HelixProfile.LoadSynchronous()) { HelixSightOffset = Helix->Offset; HelixFOV = Helix->FieldOfView; }
}
void UOBPilotView::ResetView(AOBPlayerCharacter& Pilot)
{
    Alpha = 0.f;
    UpdateView(Pilot, 0.f, false);
}

void UOBPilotView::UpdateView(AOBPlayerCharacter& P, float Dt, bool bAllowADS)
{
    const bool ADS = bAllowADS && P.bAiming && !P.Parkour()->IsBusy();
    Alpha = FMath::FInterpConstantTo(Alpha, ADS ? 1.f : 0.f, Dt, 1.f / FMath::Max(.01f,BlendSeconds));
    const float T = FMath::SmoothStep(0.f,1.f,Alpha);
    const bool Heavy = P.EquippedWeapon == EOBWeaponType::Helix12;
    P.CameraBoom->TargetArmLength = 320.f * (1.f - T);
    P.CameraBoom->TargetOffset = FVector(0,0,T * (P.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 24.f));
    P.CameraBoom->SocketOffset = FVector(0,35,65) * (1.f - T);
    P.FollowCamera->SetFieldOfView(FMath::Lerp(85.f,Heavy ? HelixFOV : ArcFOV,T));
    const auto* GI = P.GetGameInstance<UOBGameInstance>();
    const auto* S = GI ? GI->Settings.Get() : nullptr;
    float Roll = 0.f;
    if (P.Parkour()->Traversal == EOBTraversal::WallRun && S && S->TraversalMotion > 0)
        Roll = FVector::DotProduct(P.GetActorRightVector(), P.Parkour()->WallNormal) * S->WallRoll * (S->TraversalMotion == 1 ? .5f : 1.f);
    const float CurrentRoll = P.FollowCamera->GetRelativeRotation().Roll;
    P.FollowCamera->SetRelativeRotation(FRotator(0,0,Dt > 0.f ? FMath::FInterpTo(CurrentRoll,Roll,Dt,12.f) : 0.f));
    // The integrated third-person rifle continues to drive the real muzzle.
    // The local sight uses the project's existing +Y-forward rifle mesh.
    const bool FirstPerson = T > .8f;
    P.GetMesh()->SetOwnerNoSee(FirstPerson);
    for (auto* Part : {P.HelixShroud.Get(),P.HelixLeftRail.Get(),P.HelixRightRail.Get(),P.HelixCell.Get(),P.MuzzleGlow.Get()})
        Part->SetOwnerNoSee(FirstPerson);
    const FVector SightOffset = Heavy ? HelixSightOffset : ArcSightOffset;
    bool bClear = true;
    if (FirstPerson)
    {
        const FVector Eye = P.GetActorLocation() + P.CameraBoom->TargetOffset;
        // The rifle points along local +Y. Check to the tip of this sight profile.
        const UStaticMesh* Rifle = P.SightRifle->GetStaticMesh();
        const float Reach = Rifle ? SightOffset.X + Rifle->GetBoundingBox().Max.Y : 75.f;
        FHitResult Hit;
        bClear = !P.GetWorld()->SweepSingleByChannel(Hit, Eye, Eye + P.GetControlRotation().Vector() * Reach,
            FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(5.f), FCollisionQueryParams(SCENE_QUERY_STAT(OBSightClearance),false,&P));
    }
    P.SightRifle->SetVisibility(FirstPerson && bClear);
    P.SightCell->SetVisibility(FirstPerson && Heavy && bClear);
    const float Dip = P.bReloading ? FMath::Sin(PI * P.ReloadProgress) : 0.f;
    P.SightRifle->SetRelativeLocation(SightOffset + FVector(-2.f * P.VisualRecoil,0,-8.f * Dip));
    P.SightRifle->SetRelativeRotation(FRotator(0,-90.f,12.f * Dip));
}
