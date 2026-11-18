#include "OBParkourMovement.h"
#include "OBPlayerCharacter.h"
#include "OBPilotTuning.h"
#include "OBCore.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> ParkourDebug(TEXT("ob.ParkourDebug"), 0, TEXT("Draw active traversal paths"));

UOBParkourMovement::UOBParkourMovement()
{
    Tuning = FSoftObjectPath(TEXT("/Game/OrbitBreakout/Tuning/DA_OBParkourTuning.DA_OBParkourTuning"));
}
void UOBParkourMovement::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Profile = Tuning.LoadSynchronous())
    {
        CoyoteSeconds = Profile->CoyoteSeconds; BufferSeconds = Profile->BufferSeconds;
        SlideSeconds = Profile->SlideSeconds; WallSeconds = Profile->WallSeconds; WallSpeed = Profile->WallSpeed;
    }
}

bool UOBParkourMovement::CanTraverse() const
{
    const auto* P = Cast<AOBPlayerCharacter>(CharacterOwner);
    return P && P->IsParkourEnabled() && P->IsGameplayActive() && !P->bInteractionActive;
}

void UOBParkourMovement::OnMovementModeChanged(EMovementMode Previous, uint8 PreviousCustom)
{
    Super::OnMovementModeChanged(Previous, PreviousCustom);
    if (IsMovingOnGround())
    {
        bUsedAirJump = false;
        bUsedWall = false;
        bCanCutJump = false;
    }
    if (MovementMode == MOVE_Falling && Previous == MOVE_Walking && !bUsedAirJump && Velocity.Z <= 0.f)
        CoyoteUntil = GetWorld()->GetTimeSeconds() + CoyoteSeconds;
    if (MovementMode != MOVE_Custom && (Traversal == EOBTraversal::Mantle || Traversal == EOBTraversal::WallRun))
        Traversal = EOBTraversal::Free;
}

void UOBParkourMovement::PressJump()
{
    if (!CanTraverse()) return;
    bJumpHeld = true;
    if (Traversal == EOBTraversal::WallRun)
    {
        const FVector Kick = WallNormal * 430.f + TravelDirection * 280.f + FVector::UpVector * JumpZVelocity;
        Detach();
        Velocity = Kick;
        bUsedAirJump = true;
        return;
    }
    if (Traversal == EOBTraversal::Mantle) return;
    EndSlide();
    if (CharacterOwner->bIsCrouched) { UnCrouch(false); if (CharacterOwner->bIsCrouched) return; }
    if (TryMantle()) return;
    BufferUntil = GetWorld()->GetTimeSeconds() + BufferSeconds;
}

void UOBParkourMovement::ReleaseJump()
{
    bJumpHeld = false;
    if (bCanCutJump && IsFalling() && Velocity.Z > 200.f) Velocity.Z = 200.f;
    bCanCutJump = false;
    CharacterOwner->StopJumping();
}

void UOBParkourMovement::StartSlide()
{
    auto* P = Cast<AOBPlayerCharacter>(CharacterOwner);
    if (!CanTraverse() || IsBusy() || !IsMovingOnGround() || !P->bSprinting || Velocity.Size2D() < 500.f ||
        FVector::DotProduct(Velocity.GetSafeNormal2D(), P->GetControlRotation().Vector().GetSafeNormal2D()) < .65f) return;
    P->Crouch();
    Crouch(false);
    if (!P->bIsCrouched) { P->UnCrouch(); return; }
    Traversal = EOBTraversal::Slide;
    TravelDirection = Velocity.GetSafeNormal2D();
    Remaining = SlideSeconds;
}

void UOBParkourMovement::EndSlide()
{
    if (Traversal != EOBTraversal::Slide) return;
    Traversal = EOBTraversal::Free;
    CharacterOwner->UnCrouch();
}

float UOBParkourMovement::GetMaxSpeed() const
{
    return Traversal == EOBTraversal::Slide ? FMath::Lerp(360.f,760.f,FMath::Clamp(Remaining / FMath::Max(.01f,SlideSeconds),0.f,1.f)) : Super::GetMaxSpeed();
}

void UOBParkourMovement::Detach()
{
    Traversal = EOBTraversal::Free;
    Surface.Reset();
    if (MovementMode == MOVE_Custom) SetMovementMode(MOVE_Falling);
}

void UOBParkourMovement::ResetTraversal()
{
    EndSlide();
    Detach();
    BufferUntil = CoyoteUntil = -1.;
    bJumpHeld = false;
    bCanCutJump = false;
}

void UOBParkourMovement::StartCustom(EOBTraversal Mode)
{
    BufferUntil = CoyoteUntil = -1.;
    bUsedAirJump = true;
    Traversal = Mode;
    SetMovementMode(MOVE_Custom, static_cast<uint8>(Mode));
}

bool UOBParkourMovement::TryMantle()
{
    if (!(IsMovingOnGround() || IsFalling()) || Acceleration.IsNearlyZero() || Velocity.Z < -500.f) return false;
    const FVector Pos = UpdatedComponent->GetComponentLocation();
    const auto* Capsule = CharacterOwner->GetCapsuleComponent();
    const float Half = Capsule->GetScaledCapsuleHalfHeight(), Radius = Capsule->GetScaledCapsuleRadius();
    const FVector Forward = Acceleration.GetSafeNormal2D(), Feet = Pos - FVector::UpVector * Half;
    const FVector Start = Feet + FVector::UpVector * 44.f;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OBLedge), false, CharacterOwner);
    FHitResult Face, Top;
    UWorld* World = GetWorld();
    if (!World->SweepSingleByChannel(Face, Start, Start + Forward * 105.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(9.f), Params)
        || !Face.GetActor() || !(Face.GetActor()->ActorHasTag(TEXT("OB_Mantle")) || Face.GetComponent()->ComponentHasTag(TEXT("OB_Mantle"))) || FMath::Abs(Face.ImpactNormal.Z) > .25f) return false;
    const FVector Probe = Face.ImpactPoint - Face.ImpactNormal * (Radius + 8.f);
    const FVector High(Probe.X, Probe.Y, Feet.Z + 135.f);
    if (!World->LineTraceSingleByChannel(Top, High, High - FVector::UpVector * 100.f, ECC_Pawn, Params)
        || Top.GetComponent() != Face.GetComponent() || !IsWalkable(Top)) return false;
    MantleTop = Top.ImpactPoint + FVector::UpVector * (Half + 2.5f);
    MantleLift = FVector(Pos.X, Pos.Y, MantleTop.Z);
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, Half);
    FHitResult Block;
    if (MantleTop.Z <= Pos.Z || World->SweepSingleByChannel(Block, Pos, MantleLift, FQuat::Identity, ECC_Pawn, Shape, Params)
        || World->SweepSingleByChannel(Block, MantleLift, MantleTop, FQuat::Identity, ECC_Pawn, Shape, Params)) return false;
    bLifting = true;
    Surface = Top.GetActor();
    Remaining = .75f;
    StartCustom(EOBTraversal::Mantle);
    return true;
}

bool UOBParkourMovement::FindWall(FHitResult& Hit) const
{
    const FVector Start = UpdatedComponent->GetComponentLocation();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Velocity.GetSafeNormal2D());
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OBWallRun), false, CharacterOwner);
    for (float Side : {-1.f, 1.f})
        if (GetWorld()->SweepSingleByChannel(Hit, Start, Start + Right * Side * 70.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(10.f), Params)
            && Hit.GetActor() && (Hit.GetActor()->ActorHasTag(TEXT("OB_WallRun")) || Hit.GetComponent()->ComponentHasTag(TEXT("OB_WallRun"))) && FMath::Abs(Hit.ImpactNormal.Z) < .18f
            && FMath::Abs(FVector::DotProduct(Velocity.GetSafeNormal2D(), Hit.ImpactNormal)) < .45f) return true;
    return false;
}

void UOBParkourMovement::UpdateCharacterStateBeforeMovement(float Dt)
{
    Super::UpdateCharacterStateBeforeMovement(Dt);
    if (!CanTraverse()) { if (IsBusy() || BufferUntil >= 0.) ResetTraversal(); return; }
    if (ParkourDebug.GetValueOnGameThread() && IsBusy())
    {
        const FVector Pos = UpdatedComponent->GetComponentLocation();
        DrawDebugString(GetWorld(),Pos + FVector::UpVector*120.f,StaticEnum<EOBTraversal>()->GetNameStringByValue(static_cast<int64>(Traversal)));
        if (Traversal == EOBTraversal::WallRun) DrawDebugLine(GetWorld(),Pos,Pos+WallNormal*85.f,FColor::Cyan);
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (BufferUntil >= Now && (IsMovingOnGround() || (IsFalling() && !bUsedAirJump && Now <= CoyoteUntil)))
    {
        BufferUntil = CoyoteUntil = -1.;
        bUsedAirJump = true;
        SetMovementMode(MOVE_Falling);
        bCanCutJump = bJumpHeld;
        Velocity.Z = JumpZVelocity * (bJumpHeld ? 1.f : .75f);
    }
    const auto* P = CastChecked<AOBPlayerCharacter>(CharacterOwner);
    if (Traversal == EOBTraversal::Slide)
    {
        Remaining -= Dt;
        if (Remaining <= 0.f || !IsMovingOnGround() || !P->bSprinting || Velocity.Size2D() < 250.f) EndSlide();
        else Acceleration = TravelDirection * MaxAcceleration * FMath::Clamp(Remaining / SlideSeconds, .2f, 1.f);
    }
    if (IsFalling() && !bUsedWall && P->bSprinting && !Acceleration.IsNearlyZero() && Velocity.Size2D() >= 490.f && Velocity.Z > -400.f)
    {
        FHitResult Hit;
        if (FindWall(Hit))
        {
            WallNormal = Hit.ImpactNormal;
            TravelDirection = FVector::VectorPlaneProject(Velocity, WallNormal).GetSafeNormal2D();
            Surface = Hit.GetActor();
            Remaining = WallSeconds;
            bUsedWall = true;
            StartCustom(EOBTraversal::WallRun);
        }
    }
}

void UOBParkourMovement::PhysCustom(float Dt, int32 Iterations)
{
    if (!CanTraverse() || !Surface.IsValid() || Remaining <= 0.f) { Detach(); return; }
    Remaining -= Dt;
    const FVector Pos = UpdatedComponent->GetComponentLocation();
    FVector Delta;
    if (Traversal == EOBTraversal::Mantle)
    {
        const FVector Target = bLifting ? MantleLift : MantleTop;
        Delta = (Target - Pos).GetClampedToMaxSize(550.f * Dt);
        if (ParkourDebug.GetValueOnGameThread()) DrawDebugLine(GetWorld(), Pos, Target, FColor::Yellow);
    }
    else if (Traversal == EOBTraversal::WallRun)
    {
        FHitResult Wall;
        if (!CastChecked<AOBPlayerCharacter>(CharacterOwner)->bSprinting || Acceleration.IsNearlyZero() || !FindWall(Wall) || Wall.GetActor() != Surface.Get()) { Detach(); return; }
        Delta = (TravelDirection * WallSpeed - FVector::UpVector * FMath::Lerp(10.f, 110.f, 1.f - FMath::Max(0.f,Remaining) / WallSeconds)) * Dt;
    }
    else { Super::PhysCustom(Dt, Iterations); return; }
    FHitResult Hit;
    SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
    Velocity = (UpdatedComponent->GetComponentLocation() - Pos) / FMath::Max(Dt, SMALL_NUMBER);
    if (Hit.bBlockingHit) { Detach(); return; }
    if (Traversal == EOBTraversal::Mantle && UpdatedComponent->GetComponentLocation().Equals(bLifting ? MantleLift : MantleTop, 1.f))
    {
        if (bLifting) bLifting = false;
        else { Velocity = FVector::ZeroVector; Detach(); }
    }
}
