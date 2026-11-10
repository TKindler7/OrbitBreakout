#include "OBEnemyCharacter.h"
#include "OBEnemyProjectile.h"
#include "OBPlayerCharacter.h"
#include "OBRifleAnimInstance.h"
#include "OBCore.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    void SetPart(UStaticMeshComponent* Part, const FVector& Position, const FVector& Size)
    {
        Part->SetRelativeLocation(Position);
        Part->SetRelativeScale3D(Size / 100.f);
    }
}

AOBEnemyCharacter::AOBEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bUseControllerRotationYaw = false;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AAIController::StaticClass();
    GetCapsuleComponent()->InitCapsuleSize(36.f, 90.f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    // Enemy bolts occupy this object channel. Both teams' capsules otherwise remain solid.
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
    GetCharacterMovement()->MaxWalkSpeed = 260.f;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->MaxAcceleration = 1200.f;
    GetCharacterMovement()->BrakingDecelerationWalking = 1600.f;
    GetCharacterMovement()->bUseRVOAvoidance = true;
    GetCharacterMovement()->AvoidanceConsiderationRadius = 300.f;
    GetCharacterMovement()->SetAvoidanceGroup(1);
    BodyPivot = CreateDefaultSubobject<USceneComponent>(TEXT("RobotBody"));
    BodyPivot->SetupAttachment(GetCapsuleComponent());
    // Keep the old subobject names loadable for existing maps, but replace the
    // block-model body with the same authored skeletal body used by the player.
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    if (Manny.Succeeded()) TrooperSkeletalMesh = Manny.Object;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Rifle(TEXT("/Game/Weapons/Rifle/Meshes/SM_Rifle.SM_Rifle"));
    if (Rifle.Succeeded()) TrooperRifleMesh = Rifle.Object;
    WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SecurityRifleGrip"));
    WeaponRoot->SetupAttachment(GetMesh(), TEXT("hand_r"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Part = [&](const TCHAR* Name)
    {
        UStaticMeshComponent* P = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        P->SetupAttachment(BodyPivot);
        if (Cube.Succeeded()) P->SetStaticMesh(Cube.Object);
        P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        P->SetCanEverAffectNavigation(false);
        return P;
    };
    Torso = Part(TEXT("ArmoredTorso"));
    Head = Part(TEXT("Helmet"));
    Visor = Part(TEXT("SecurityVisor"));
    Core = Part(TEXT("PowerCore"));
    LeftArm = Part(TEXT("LeftArm"));
    RightArm = Part(TEXT("RightArm"));
    LeftLeg = Part(TEXT("LeftLeg"));
    RightLeg = Part(TEXT("RightLeg"));
    Weapon = Part(TEXT("PulseEmitter"));
    Muzzle = Part(TEXT("ChargeEmitter"));
    WeaponBarrel = Part(TEXT("SecurityRifleBarrel"));
    WeaponStock = Part(TEXT("SecurityRifleStock"));
    WeaponGrip = Part(TEXT("SecurityRifleHandle"));
    WeaponMagazine = Part(TEXT("SecurityRifleMagazine"));
    WeaponRail = Part(TEXT("SecurityRifleRail"));
    WeaponCell = Part(TEXT("SecurityRifleEnergyCell"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Cylinder.Succeeded()) WeaponBarrel->SetStaticMesh(Cylinder.Object);
    if (Sphere.Succeeded()) Muzzle->SetStaticMesh(Sphere.Object);
    Muzzle->SetCastShadow(false);
    Core->SetCastShadow(false);
    Visor->SetCastShadow(false);
}

void AOBEnemyCharacter::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GetCapsuleComponent()->SetCapsuleSize(bHeavy ? 44.f : 36.f, bHeavy ? 112.f : 90.f);
    ConfigureCharacterVisuals();
}

void AOBEnemyCharacter::ConfigureCharacterVisuals()
{
    for (UStaticMeshComponent* Legacy : {Torso.Get(),Head.Get(),Visor.Get(),Core.Get(),LeftArm.Get(),RightArm.Get(),LeftLeg.Get(),RightLeg.Get()})
    {
        Legacy->SetVisibility(false);
        Legacy->SetHiddenInGame(true);
        Legacy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    USkeletalMeshComponent* Mesh = GetMesh();
    const bool bBull = bHeavy && HeavySkeletalMesh;
    Mesh->SetSkeletalMesh(bBull ? HeavySkeletalMesh.Get() : TrooperSkeletalMesh.Get());
    Mesh->SetRelativeLocation(bBull ? HeavyMeshOffset : FVector(0,0,bHeavy ? -112.f : -90.f));
    Mesh->SetRelativeRotation(bBull ? HeavyMeshRotation : FRotator(0,-90,0));
    Mesh->SetRelativeScale3D(bBull ? HeavyMeshScale : FVector(bHeavy ? 1.18f : .96f));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCanEverAffectNavigation(false);
    Mesh->SetVisibility(true);
    Mesh->SetHiddenInGame(false);
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Mesh->EmptyOverrideMaterials();
    UniformMIDs.Reset();
    if (bBull)
    {
        Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        CurrentHeavyAnimation = nullptr;
        UpdateHeavyAnimation();
    }
    else
    {
        Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        Mesh->SetAnimInstanceClass(UOBRifleAnimInstance::StaticClass());
        // Preserve all authored masks, normal/roughness maps and segmentation.
        // The cloned shader adds a full-surface tint after its BaseColor graph.
        for (int32 Index=0; Index<Mesh->GetNumMaterials(); ++Index)
        {
            const FString ArmorPath = FString::Printf(TEXT("/Game/OrbitBreakout/Materials/Characters/MI_Armor_%02d.MI_Armor_%02d"), Index+1, Index+1);
            UMaterialInterface* ArmorMaterial = LoadObject<UMaterialInterface>(nullptr, *ArmorPath);
            if (ArmorMaterial) Mesh->SetMaterial(Index, ArmorMaterial);
            if (UMaterialInstanceDynamic* Uniform = Mesh->CreateDynamicMaterialInstance(Index))
            {
                Uniform->SetVectorParameterValue(ArmorMaterial ? TEXT("ArmorTint") : TEXT("Paint Tint"),
                    Index==0 ? FLinearColor(.13f,.18f,.24f) : FLinearColor(.055f,.075f,.095f));
                Uniform->SetScalarParameterValue(TEXT("MetalPaintMetallic"), .65f);
                Uniform->SetScalarParameterValue(TEXT("MetalPaintRoughness"), .38f);
                UniformMIDs.Add(Uniform);
            }
        }
    }
    const bool bHasWeaponBone = Mesh->GetBoneIndex(TEXT("weapon_r")) != INDEX_NONE;
    const FName GripSocket = bBull ? HeavyWeaponSocket :
        bHasWeaponBone ? FName(TEXT("weapon_r")) : FName(TEXT("hand_r"));
    WeaponRoot->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform, GripSocket);
    FTransform GripTransform = bBull ? FTransform(HeavyEmitterRotation, HeavyEmitterOffset, HeavyEmitterScale) :
        FTransform(WeaponGripRotation, WeaponGripOffset);
    if (!bBull && !bHasWeaponBone)
    {
        // Manny Simple omits weapon_r from its mesh. Reconstruct the authored
        // rifle bone frame from the animated hand rather than reuse its axes.
        GripTransform = GripTransform * UOBRifleAnimInstance::GetWeaponBoneInRightHand();
    }
    WeaponRoot->SetRelativeTransform(GripTransform);
    for (UStaticMeshComponent* Part : {Weapon.Get(),Muzzle.Get(),WeaponBarrel.Get(),WeaponStock.Get(),WeaponGrip.Get(),WeaponMagazine.Get(),WeaponRail.Get(),WeaponCell.Get()})
    {
        Part->AttachToComponent(WeaponRoot, FAttachmentTransformRules::KeepRelativeTransform);
        Part->SetVisibility(true);
        Part->SetHiddenInGame(false);
    }
    // Compact receiver, a separate cylindrical barrel, stock, handle and
    // magazine read as a manufactured firearm rather than a floating block.
    Weapon->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    SetPart(Weapon, FVector(13,0,5), FVector(37,9,11));
    SetPart(WeaponStock, FVector(-13,0,5), FVector(18,7,9));
    SetPart(WeaponGrip, FVector(0,0,-4), FVector(7,6,15));
    WeaponGrip->SetRelativeRotation(FRotator(-14,0,0));
    SetPart(WeaponMagazine, FVector(15,0,-6), FVector(9,7,17));
    WeaponMagazine->SetRelativeRotation(FRotator(-8,0,0));
    SetPart(WeaponRail, FVector(14,0,12), FVector(32,5,3));
    SetPart(WeaponBarrel, FVector(37,0,5), FVector(7,7,28));
    WeaponBarrel->SetRelativeRotation(FRotator(90,0,0));
    SetPart(WeaponCell, FVector(13,-5,6), FVector(21,1.5f,3));
    SetPart(Muzzle, FVector(52,0,5), FVector(5,5,5));

    UMaterialInterface* Metal = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/OrbitBreakout/Materials/M_Metal.M_Metal"));
    if (!Metal) Metal = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/OrbitBreakout/Materials/M_Emissive.M_Emissive"));
    if (!Glow) Glow = Metal;
    if (Metal)
    {
        ArmorMID = UMaterialInstanceDynamic::Create(Metal, this);
        ArmorMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(.11f,.14f,.18f));
        DarkMID = UMaterialInstanceDynamic::Create(Metal, this);
        DarkMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(.035f,.05f,.065f));
        for (UStaticMeshComponent* P : {Weapon.Get(),WeaponStock.Get(),WeaponMagazine.Get()}) P->SetMaterial(0, ArmorMID);
        for (UStaticMeshComponent* P : {WeaponBarrel.Get(),WeaponGrip.Get(),WeaponRail.Get()}) P->SetMaterial(0, DarkMID);
    }
    if (Glow)
    {
        AccentMID = UMaterialInstanceDynamic::Create(Glow, this);
        SetGlow(.7f);
        for (UStaticMeshComponent* P : {WeaponCell.Get(),Muzzle.Get()}) P->SetMaterial(0, AccentMID);
    }
    if (bBull)
    {
        // The Bull keeps its authored creature attack gestures. Its integrated
        // forearm pulse emitter does not require a human rifle-grip pose.
        for (UStaticMeshComponent* RifleOnly : {WeaponStock.Get(),WeaponGrip.Get(),WeaponMagazine.Get()})
            RifleOnly->SetVisibility(false);
    }
    else if (TrooperRifleMesh)
    {
        Weapon->SetStaticMesh(TrooperRifleMesh);
        Weapon->SetRelativeTransform(FTransform::Identity);
        Weapon->EmptyOverrideMaterials();
        for (UStaticMeshComponent* FallbackPart : {WeaponBarrel.Get(),WeaponStock.Get(),WeaponGrip.Get(),WeaponMagazine.Get(),WeaponRail.Get(),WeaponCell.Get()})
            FallbackPart->SetVisibility(false);
        if (Weapon->DoesSocketExist(TEXT("Muzzle")))
        {
            Muzzle->AttachToComponent(Weapon, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Muzzle"));
            Muzzle->SetRelativeLocation(FVector::ZeroVector);
        }
        else if (Weapon->DoesSocketExist(TEXT("muzzle")))
        {
            Muzzle->AttachToComponent(Weapon, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("muzzle"));
            Muzzle->SetRelativeLocation(FVector::ZeroVector);
        }
        else
        {
            // The rifle is authored along +Y. The similarly named skeleton
            // bone is unanimated in these clips, so use the actual barrel tip.
            Muzzle->AttachToComponent(Weapon, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
            Muzzle->SetRelativeLocation(FVector(0,52,3));
        }
    }
}

void AOBEnemyCharacter::UpdateHeavyAnimation()
{
    if (!bHeavy || !HeavySkeletalMesh) return;
    UAnimationAsset* Wanted = IsDead() ? HeavyDeathAnimation.Get() :
        bBossIntroPlaying && HeavyIntroAnimation ? HeavyIntroAnimation.Get() :
        (bCharging || BoltsRemaining>0 || BossAttack==EOBBossAttack::WaveWarning || BossAttack==EOBBossAttack::Shockwave) && HeavyAttackAnimation ? HeavyAttackAnimation.Get() :
        (GetVelocity().SizeSquared2D()>100.f || BossAttack==EOBBossAttack::Rush) && HeavyMoveAnimation ? HeavyMoveAnimation.Get() : HeavyIdleAnimation.Get();
    if (!Wanted || CurrentHeavyAnimation == Wanted) return;
    CurrentHeavyAnimation = Wanted;
    const bool bIntroClip = bBossIntroPlaying && !IsDead() && Wanted==HeavyIntroAnimation;
    GetMesh()->PlayAnimation(Wanted, !IsDead() && !bIntroClip);
    const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Wanted);
    GetMesh()->SetPlayRate(bIntroClip && Sequence ? Sequence->GetPlayLength()/FMath::Clamp(BossIntroSeconds,1.f,6.f) : 1.f);
}

void AOBEnemyCharacter::StartBossIntroAnimation()
{
    bBossIntroPlayed = true;
    bBossIntroPlaying = true;
    CancelAttack();
    bCanSeeTarget = false;
    if (AAIController* AI = Cast<AAIController>(Controller)) AI->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    CurrentHeavyAnimation = nullptr;
    UpdateHeavyAnimation();
    SetGlow(3.f);
    if (HeavyIntroSound) PlayCue(HeavyIntroSound,.8f);
    else if (ChargeSound) PlayCue(ChargeSound,.65f);
    else if (Director.IsValid()) Director->PlaySound(TEXT("Charge"),GetActorLocation(),.65f);
}

void AOBEnemyCharacter::FinishBossIntroAnimation()
{
    if (!bBossIntroPlaying) return;
    bBossIntroPlaying = false;
    CurrentHeavyAnimation = nullptr;
    UpdateHeavyAnimation();
    SetGlow(.7f);
}

void AOBEnemyCharacter::ResumeAfterBossIntro()
{
    FinishBossIntroAnimation();
    if (!bActive || IsDead()) return;
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    State = EOBEnemyState::Acquire;
    NextAttackTime = GetWorld()->GetTimeSeconds()+.6f;
    NextMoveTime = GetWorld()->GetTimeSeconds()+.6f;
    if (Target.IsValid() && !Target->bDead) RememberTarget(Target->GetActorLocation());
}

void AOBEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();
    Director = AOBMissionDirector::Get(this);
    Target = Cast<AOBPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    Health = bHeavy ? SentinelHealth : TrooperHealth;
    bBossIntroPlayed = bBossIntroPlaying = false;
    BossAttackSequence = 0;
    ResetBossAttack();
    GetCharacterMovement()->MaxWalkSpeed = bHeavy ? 190.f : 260.f;
    InitialLocation = GetActorLocation();
    InitialFacing = GetActorRotation();
    State = EOBEnemyState::Dormant;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetActorTickEnabled(false);
    GetCharacterMovement()->DisableMovement();
    // Rebuild runtime MIDs because construction-created MIDs are not durable saved assets.
    OnConstruction(GetActorTransform());
}

void AOBEnemyCharacter::Activate()
{
    if (bActive || IsDead()) return;
    if (!Director.IsValid()) Director = AOBMissionDirector::Get(this);
    bActive = true;
    State = EOBEnemyState::Idle;
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    SetActorTickEnabled(true);
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    if (!Controller) SpawnDefaultController();
    const uint32 Offset = GetTypeHash(StableId) % 4;
    NextAttackTime = GetWorld()->GetTimeSeconds() + InitialAttackDelay + Offset * .3f;
    NextMoveTime = GetWorld()->GetTimeSeconds() + .25f;
    if (bHeavy && Director.IsValid())
        Director->ShowMessage(TEXT("BULL SENTINEL  /  Move out of charge lanes. Jump over shockwaves."), 6.f);
    // The room alarm gives this wave an initial last-known position. It does
    // not activate later waves or permit shots through a wall.
    if (!Target.IsValid()) Target = Cast<AOBPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (Target.IsValid() && !Target->bDead) RememberTarget(Target->GetActorLocation());
    GetWorldTimerManager().SetTimer(PerceptionTimer, this, &AOBEnemyCharacter::Perceive, .2f, true, .1f);
    if (bHeavy && Director.IsValid() && !bBossIntroPlayed) Director->BeginBossIntro(this);
}

FVector AOBEnemyCharacter::GetMuzzleLocation() const
{
    return Muzzle ? Muzzle->GetComponentLocation() : GetActorLocation() + GetActorForwardVector() * 65.f + FVector(0,0,25);
}

float AOBEnemyCharacter::GetVisualAimPitch() const
{
    if (!bCanSeeTarget && !HasRecentAwareness()) return 0.f;
    const FVector AimPoint = (bCharging || BoltsRemaining>0) && Target.IsValid() ?
        Target->GetActorLocation()+FVector(0,0,12.f) : LastSeenLocation+FVector(0,0,12.f);
    return FMath::Clamp((AimPoint-GetActorLocation()-FVector(0,0,35.f)).Rotation().Pitch,-50.f,50.f);
}

float AOBEnemyCharacter::GetBossHealthFraction() const
{
    return FMath::Clamp(Health / FMath::Max(1.f, SentinelHealth), 0.f, 1.f);
}

bool AOBEnemyCharacter::IsBossEnraged() const
{
    return bHeavy && !IsDead() && GetBossHealthFraction() <= .5f;
}

FString AOBEnemyCharacter::GetBossAttackCue() const
{
    if (bBossIntroPlaying) return TEXT("CONTAINMENT BREACH  /  SENTINEL AWAKENING");
    switch (BossAttack)
    {
        case EOBBossAttack::RushWarning: return TEXT("CHARGE INCOMING  /  MOVE SIDEWAYS");
        case EOBBossAttack::Rush: return TEXT("BULL RUSH  /  CLEAR THE LANE");
        case EOBBossAttack::WaveWarning: return TEXT("SHOCKWAVE CHARGING  /  PREPARE TO JUMP");
        case EOBBossAttack::Shockwave: return TEXT("SHOCKWAVE  /  JUMP OR TAKE COVER");
        case EOBBossAttack::Recovery: return TEXT("RECOVERING  /  ATTACK NOW");
        default: return bCharging ? TEXT("PULSE BURST  /  TAKE COVER") :
            IsBossEnraged() ? TEXT("OVERDRIVE  /  FASTER RECOVERY") : TEXT("HEAVY SENTINEL");
    }
}

bool AOBEnemyCharacter::HasRecentAwareness() const
{
    return bActive && !IsDead() && GetWorld() &&
        GetWorld()->GetTimeSeconds() - LastSeenTime <= FMath::Max(1.f, AlertMemorySeconds);
}

void AOBEnemyCharacter::RememberTarget(const FVector& Location)
{
    LastSeenLocation = Location;
    LastSeenTime = GetWorld()->GetTimeSeconds();
    bAlerted = true;
}

bool AOBEnemyCharacter::CanVisuallyDetect(const AOBPlayerCharacter* Player) const
{
    if (!bActive || IsDead() || !Player || Player->bDead) return false;
    const FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
    const float Distance = ToPlayer.Size2D();
    if (Distance > AcquisitionRange || FMath::Abs(ToPlayer.Z) > 350.f) return false;
    const float FacingDot = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ToPlayer.GetSafeNormal2D());
    // Wider peripheral vision, plus close-range awareness from any direction.
    // Every visual acquisition still needs a clear shot path around cover.
    return (FacingDot >= .25f || HasRecentAwareness() || Distance <= NearbyAwarenessRange) && HasLineOfSight(Player);
}

void AOBEnemyCharacter::ReceiveSquadAlert(const AOBEnemyCharacter* Source, const FVector& Location)
{
    if (!IsValid(Source) || Source == this || !bActive || IsDead() ||
        !Source->bActive || Source->IsDead() || Source->EncounterId != EncounterId ||
        FVector::DistSquared(GetActorLocation(), Source->GetActorLocation()) > FMath::Square(SquadAlertRange)) return;
    // Radio reports carry a snapshot, never a live target through cover.
    RememberTarget(Location);
}

void AOBEnemyCharacter::AlertNearbySquad()
{
    if (!Director.IsValid() || !bActive || IsDead()) return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastSquadAlertTime < .75f) return;
    LastSquadAlertTime = Now;
    for (AOBEnemyCharacter* Ally : Director->Enemies)
        if (IsValid(Ally)) Ally->ReceiveSquadAlert(this, LastSeenLocation);
}

bool AOBEnemyCharacter::HasLineOfSight(const AOBPlayerCharacter* Player) const
{
    if (!Player || Player->bDead) return false;
    const FVector Source = GetMuzzleLocation();
    const FVector Destination = Player->GetActorLocation() + FVector(0,0,18);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OBEnemySight), false, this);
    FHitResult Hit;
    // An emitter can protrude beyond the capsule. It must not peek through nearby cover.
    if (GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation() + FVector(0,0,24), Source, ECC_Visibility, Params))
        return false;
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Source, Destination, ECC_Visibility, Params);
    return !bHit || Hit.GetActor() == Player;
}

void AOBEnemyCharacter::Perceive()
{
    if (!bActive || IsDead()) return;
    if (bBossIntroPlaying || (Director.IsValid() && Director->IsBossIntroActive())) return;
    if (!Director.IsValid()) Director = AOBMissionDirector::Get(this);
    if (!Target.IsValid()) Target = Cast<AOBPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    AOBPlayerCharacter* Player = Target.Get();
    if (!Director.IsValid() || !Director->IsCombatAllowed() || !Player || Player->bDead)
    {
        bCanSeeTarget = false;
        CancelAttack();
        if (AAIController* AI = Cast<AAIController>(Controller)) AI->StopMovement();
        return;
    }
    // Specials commit to a marked direction or center. Perception must never
    // steer a rush after the player has dodged the warning lane.
    if (BossAttack != EOBBossAttack::None) return;
    const float Now = GetWorld()->GetTimeSeconds();
    const FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
    const float Distance = ToPlayer.Size2D();
    const bool bSameDeck = FMath::Abs(ToPlayer.Z) <= 250.f;
    const bool bNewGunshot = bSameDeck && Distance <= GunshotHearingRange &&
        Player->LastShotTime > LastHeardShotTime && Now - Player->LastShotTime <= .4f;
    const float FootstepRange = Player->bSprinting ? 900.f : 500.f;
    const bool bHeardFootsteps = bSameDeck && Distance <= FootstepRange && Player->GetVelocity().SizeSquared2D() >= FMath::Square(220.f);
    if (bNewGunshot || bHeardFootsteps)
    {
        if (bNewGunshot) LastHeardShotTime = Player->LastShotTime;
        RememberTarget(Player->GetActorLocation());
        AlertNearbySquad();
    }
    bCanSeeTarget = CanVisuallyDetect(Player);
    if (bCanSeeTarget)
    {
        RememberTarget(Player->GetActorLocation());
        AlertNearbySquad();
        if (bCharging || BoltsRemaining > 0 || SlotReleaseCountdown > 0.f) return;
        // Let a between-burst strafe finish. Perception must not cancel the path
        // at its next 0.2-second update, before the guard has left its old position.
        if (State == EOBEnemyState::Reposition && Now < NextAttackTime)
        {
            if (const AAIController* AI = Cast<AAIController>(Controller))
                if (AI->GetMoveStatus() == EPathFollowingStatus::Moving) return;
        }
        const float UpperRange = bHeavy ? 1400.f : 1200.f;
        if (Distance > UpperRange)
        {
            State = EOBEnemyState::Approach;
            RequestMove(LastSeenLocation, bHeavy ? 950.f : 850.f);
        }
        else
        {
            State = EOBEnemyState::Acquire;
            if (AAIController* AI = Cast<AAIController>(Controller)) AI->StopMovement();
            if (Now >= NextAttackTime) BeginCharge();
        }
    }
    else
    {
        // Losing sight cancels an uncommitted telegraph and releases its attack slot.
        if (bCharging) CancelAttack();
        if (BoltsRemaining > 0 || SlotReleaseCountdown > 0.f) return;
        bAlerted = HasRecentAwareness();
        if (bAlerted)
        {
            State = EOBEnemyState::Approach;
            RequestMove(LastSeenLocation, 100.f);
        }
        else
        {
            State = EOBEnemyState::Idle;
            if (AAIController* AI = Cast<AAIController>(Controller)) AI->StopMovement();
        }
    }
}

void AOBEnemyCharacter::RequestMove(const FVector& Destination, float AcceptanceRadius)
{
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < NextMoveTime) return;
    NextMoveTime = Now + .85f;
    AAIController* AI = Cast<AAIController>(Controller);
    if (!AI) return;
    const EPathFollowingRequestResult::Type Result = AI->MoveToLocation(Destination, AcceptanceRadius, true, true, true, true);
    if (Result == EPathFollowingRequestResult::Failed)
    {
        if (UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld()))
        {
            FNavLocation Alternate;
            if (Nav->GetRandomReachablePointInRadius(GetActorLocation(), 350.f, Alternate))
            {
                AI->MoveToLocation(Alternate.Location, 60.f, true, true, false, true);
                State = EOBEnemyState::Reposition;
            }
        }
    }
}

void AOBEnemyCharacter::BeginCharge()
{
    if (!bCanSeeTarget || !Director.IsValid() || !Director->IsCombatAllowed() || !Target.IsValid()) return;
    if (!Director->TryAcquireAttackSlot(this))
    {
        NextAttackTime = GetWorld()->GetTimeSeconds() + .25f;
        return;
    }
    bHasAttackSlot = true;
    if (bHeavy)
    {
        const float Distance = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
        const int32 Pattern = BossAttackSequence++ % 3;
        if (Pattern == 2 && Distance <= ShockwaveRadius + 100.f)
        {
            BeginBossAttack(EOBBossAttack::WaveWarning);
            return;
        }
        if (Pattern != 0 && Distance > 280.f && Distance <= RushMaxDistance + 150.f)
        {
            BeginBossAttack(EOBBossAttack::RushWarning);
            return;
        }
        if (Pattern != 0 && Distance <= ShockwaveRadius)
        {
            BeginBossAttack(EOBBossAttack::WaveWarning);
            return;
        }
    }
    bCharging = true;
    ChargeElapsed = 0.f;
    ChargeDuration = FMath::Max(.2f, bHeavy ? SentinelChargeSeconds : TrooperChargeSeconds);
    State = EOBEnemyState::Attack;
    if (AAIController* AI = Cast<AAIController>(Controller)) AI->StopMovement();
    if (ChargeSound) PlayCue(ChargeSound, .45f);
    else Director->PlaySound(TEXT("Charge"), GetActorLocation(), .45f);
}

void AOBEnemyCharacter::EmitBolt()
{
    if (IsDead() || !Director.IsValid() || !Director->IsCombatAllowed()) { CancelAttack(); return; }
    const FVector Source = GetMuzzleLocation();
    FHitResult MuzzleBlock;
    FCollisionQueryParams MuzzleParams(SCENE_QUERY_STAT(OBEnemyMuzzle), false, this);
    if (GetWorld()->LineTraceSingleByChannel(MuzzleBlock, GetActorLocation() + FVector(0,0,24), Source, ECC_Visibility, MuzzleParams))
    {
        CancelAttack();
        NextAttackTime = GetWorld()->GetTimeSeconds() + .6f;
        return;
    }
    FVector Direction = (LockedTarget - Source).GetSafeNormal();
    // Aim is sampled once at the end of the charge; bolts never home or retarget.
    Direction = FMath::VRandCone(Direction, FMath::DegreesToRadians(bHeavy ? 1.5f : 2.f));
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.Instigator = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AOBEnemyProjectile* Bolt = GetWorld()->SpawnActor<AOBEnemyProjectile>(Source, Direction.Rotation(), Params);
    if (Bolt) Bolt->Initialize(Direction, ProjectileSpeed, bHeavy ? SentinelDamage : TrooperDamage, Controller);
    LastShotTime = GetWorld()->GetTimeSeconds();
    FlashRemaining = .08f;
    if (ShotSound) PlayCue(ShotSound, .55f);
    else Director->PlaySound(TEXT("EnemyShot"), Source, .55f);
}

void AOBEnemyCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bActive || IsDead()) return;
    if (bBossIntroPlaying || (Director.IsValid() && Director->IsBossIntroActive()))
    {
        UpdateHeavyAnimation();
        return;
    }
    if (GetActorLocation().Z < -1500.f)
    {
        UE_LOG(LogTemp, Error, TEXT("Orbit placement defect: enemy %s fell below safe boundary"), *StableId.ToString());
        Die(true);
        return;
    }
    if (!Director.IsValid() || !Director->IsCombatAllowed())
    {
        CancelAttack();
        return;
    }
    VisualClock += DeltaSeconds;
    FlashRemaining = FMath::Max(0.f, FlashRemaining - DeltaSeconds);
    UpdateHeavyAnimation();

    if (BossAttack != EOBBossAttack::None)
    {
        TickBossAttack(DeltaSeconds);
        return;
    }

    if (bCanSeeTarget || HasRecentAwareness())
    {
        FVector Facing = (bCharging || BoltsRemaining == 0) ? LastSeenLocation : LockedTarget;
        const FRotator Desired(0.f, (Facing - GetActorLocation()).Rotation().Yaw, 0.f);
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), Desired, DeltaSeconds, 7.f));
    }
    else if (State == EOBEnemyState::Idle)
    {
        const FRotator SearchFacing(0.f, InitialFacing.Yaw + FMath::Sin(VisualClock * .65f) * 65.f, 0.f);
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), SearchFacing, DeltaSeconds, 2.f));
    }
    if (bCharging)
    {
        ChargeElapsed += DeltaSeconds;
        const float Progress = FMath::Clamp(ChargeElapsed / ChargeDuration, 0.f, 1.f);
        SetGlow(.8f + Progress * 7.f);
        Muzzle->SetRelativeScale3D(FVector(.09f * (1.f + Progress * 1.8f)));
        if (ChargeElapsed >= ChargeDuration)
        {
            if (!Target.IsValid() || !HasLineOfSight(Target.Get())) { CancelAttack(); return; }
            LockedTarget = Target->GetActorLocation() + FVector(0,0,12.f);
            bCharging = false;
            BoltsRemaining = 3;
            BoltCountdown = 0.f;
        }
    }
    if (!bCharging && BoltsRemaining > 0)
    {
        BoltCountdown -= DeltaSeconds;
        while (BoltCountdown <= 0.f && BoltsRemaining > 0)
        {
            EmitBolt();
            if (IsDead() || !bHasAttackSlot) break;
            --BoltsRemaining;
            BoltCountdown += bHeavy ? .25f : .18f;
            if (BoltsRemaining == 0)
            {
                SlotReleaseCountdown = .25f;
                NextAttackTime = GetWorld()->GetTimeSeconds() + (bHeavy ? (IsBossEnraged() ? 1.45f : 2.2f) : 1.6f);
            }
        }
    }
    else if (!bCharging && SlotReleaseCountdown > 0.f)
    {
        SlotReleaseCountdown -= DeltaSeconds;
        if (SlotReleaseCountdown <= 0.f)
        {
            ReleaseAttackSlot();
            State = EOBEnemyState::Reposition;
            // Small authored-space strafe between attacks, with navigation projection.
            if (UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld()))
            {
                const float Side = ((GetTypeHash(StableId) + FMath::FloorToInt(VisualClock)) % 2) ? 1.f : -1.f;
                FNavLocation Point;
                const FVector Destination = GetActorLocation() + GetActorRightVector() * Side * 160.f;
                if (Nav->ProjectPointToNavigation(Destination, Point, FVector(100,100,180)))
                    RequestMove(Point.Location, 30.f);
            }
        }
    }
    if (!bCharging)
    {
        SetGlow(FlashRemaining > 0.f ? 7.f : .7f);
        Muzzle->SetRelativeScale3D(FVector(FlashRemaining > 0.f ? .19f : .09f));
    }
}

void AOBEnemyCharacter::BeginBossAttack(EOBBossAttack Warning)
{
    if (!bHeavy || !Target.IsValid()) return;
    bCharging = false;
    BoltsRemaining = 0;
    BossAttack = Warning;
    BossAttackElapsed = 0.f;
    WaveRadius = 0.f;
    bBossAttackHitPlayer = false;
    BossAttackOrigin = GetActorLocation();
    BossAttackOrigin.Z -= GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.f;
    RushDirection = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    if (RushDirection.IsNearlyZero()) RushDirection = GetActorForwardVector();
    RushRemainingDistance = FMath::Clamp(FVector::Dist2D(GetActorLocation(), Target->GetActorLocation()) + 140.f,
        250.f, FMath::Max(250.f, RushMaxDistance));
    State = EOBEnemyState::Attack;
    if (AAIController* AI = Cast<AAIController>(Controller)) AI->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    SetActorRotation(FRotator(0, RushDirection.Rotation().Yaw, 0));
    CreateBossTelegraph();
    UpdateBossTelegraph();
    if (ChargeSound) PlayCue(ChargeSound, .6f);
    else if (Director.IsValid()) Director->PlaySound(TEXT("Charge"), GetActorLocation(), .6f);
}

void AOBEnemyCharacter::TickBossAttack(float DeltaSeconds)
{
    BossAttackElapsed += DeltaSeconds;
    if (BossAttack == EOBBossAttack::RushWarning)
    {
        const float Duration = FMath::Max(.8f, RushWarningSeconds);
        SetGlow(1.f + 4.f * FMath::Clamp(BossAttackElapsed / Duration, 0.f, 1.f));
        if (BossAttackElapsed >= Duration)
        {
            BossAttack = EOBBossAttack::Rush;
            BossAttackElapsed = 0.f;
        }
    }
    else if (BossAttack == EOBBossAttack::Rush)
    {
        // Sweeping the entire capsule prevents tunnelling through the player,
        // reactor equipment or doors even on a slow frame. The rush never homes.
        const float Step = FMath::Min(RushRemainingDistance, FMath::Max(100.f, RushSpeed) * DeltaSeconds);
        FHitResult Hit;
        AddActorWorldOffset(RushDirection * Step, true, &Hit);
        RushRemainingDistance = FMath::Max(0.f, RushRemainingDistance - Step);
        if (Hit.bBlockingHit)
        {
            if (AOBPlayerCharacter* Player = Cast<AOBPlayerCharacter>(Hit.GetActor())) ApplyBossHit(Player, RushDamage);
            FinishBossAttack();
        }
        else if (RushRemainingDistance <= 0.f || BossAttackElapsed >= 2.f)
            FinishBossAttack();
    }
    else if (BossAttack == EOBBossAttack::WaveWarning)
    {
        const float Duration = FMath::Max(1.f, WaveWarningSeconds);
        SetGlow(1.f + 4.f * FMath::Clamp(BossAttackElapsed / Duration, 0.f, 1.f));
        if (BossAttackElapsed >= Duration)
        {
            BossAttack = EOBBossAttack::Shockwave;
            BossAttackElapsed = 0.f;
            WaveRadius = 0.f;
            if (Director.IsValid()) Director->PlaySound(TEXT("Impact"), GetActorLocation(), .7f);
        }
    }
    else if (BossAttack == EOBBossAttack::Shockwave)
    {
        const float OldRadius = WaveRadius;
        WaveRadius = FMath::Min(ShockwaveRadius, WaveRadius + FMath::Max(100.f, ShockwaveSpeed) * DeltaSeconds);
        if (CanShockwaveHit(Target.Get(), OldRadius, WaveRadius)) ApplyBossHit(Target.Get(), ShockwaveDamage);
        if (WaveRadius >= ShockwaveRadius) FinishBossAttack();
    }
    else if (BossAttack == EOBBossAttack::Recovery && BossAttackElapsed >= RecoveryDuration)
    {
        ResetBossAttack();
        State = EOBEnemyState::Acquire;
        // Perceive will reacquire normally; no immediate attack from this tick.
        NextAttackTime = GetWorld()->GetTimeSeconds() + .25f;
    }
    UpdateBossTelegraph();
}

bool AOBEnemyCharacter::CanShockwaveHit(const AOBPlayerCharacter* Player, float PreviousRadius, float NewRadius) const
{
    if (!Player || Player->bDead || bBossAttackHitPlayer || !GetWorld()) return false;
    const UCapsuleComponent* PlayerCapsule = Player->GetCapsuleComponent();
    const float FeetZ = Player->GetActorLocation().Z - PlayerCapsule->GetScaledCapsuleHalfHeight();
    // This is a low floor shockwave. A real jump or raised cover avoids it;
    // actors on another deck cannot be struck by a radial distance check.
    if (FeetZ > BossAttackOrigin.Z + 65.f || FeetZ < BossAttackOrigin.Z - 65.f) return false;
    const float Distance = FVector::Dist2D(BossAttackOrigin, Player->GetActorLocation());
    const float HalfWidth = 18.f + PlayerCapsule->GetScaledCapsuleRadius();
    if (Distance + HalfWidth < PreviousRadius || Distance - HalfWidth > NewRadius) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OBBossWave), false, this);
    FHitResult Hit;
    const FVector Source = BossAttackOrigin + FVector(0,0,35.f);
    const FVector Destination(Player->GetActorLocation().X, Player->GetActorLocation().Y, Source.Z);
    const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Source, Destination, ECC_Visibility, Params);
    return !bBlocked || Hit.GetActor() == Player;
}

void AOBEnemyCharacter::ApplyBossHit(AOBPlayerCharacter* Player, float Damage)
{
    if (bBossAttackHitPlayer || !Player || Player->bDead || !Director.IsValid() || !Director->IsCombatAllowed()) return;
    bBossAttackHitPlayer = true;
    Player->DamagePlayer(Damage, GetActorLocation());
}

void AOBEnemyCharacter::FinishBossAttack()
{
    BossAttack = EOBBossAttack::Recovery;
    BossAttackElapsed = 0.f;
    RecoveryDuration = IsBossEnraged() ? 1.35f : 2.15f;
    ReleaseAttackSlot();
    HideBossTelegraph();
    SetGlow(.7f);
    // Hold the recovery pose in place, giving the player a clear punish window.
    GetCharacterMovement()->StopMovementImmediately();
}

void AOBEnemyCharacter::ResetBossAttack()
{
    const bool bHadSpecial = BossAttack != EOBBossAttack::None;
    BossAttack = EOBBossAttack::None;
    BossAttackElapsed = 0.f;
    WaveRadius = 0.f;
    RushRemainingDistance = 0.f;
    bBossAttackHitPlayer = false;
    HideBossTelegraph();
    if (bHadSpecial && bActive && !IsDead()) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void AOBEnemyCharacter::CreateBossTelegraph()
{
    if (!BossRingSegments.IsEmpty()) return;
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/OrbitBreakout/Materials/M_Emissive.M_Emissive"));
    if (Glow) BossTelegraphMID = UMaterialInstanceDynamic::Create(Glow, this);
    auto Segment = [&]()
    {
        UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
        Part->SetupAttachment(GetCapsuleComponent());
        Part->SetAbsolute(true, true, true);
        Part->SetMobility(EComponentMobility::Movable);
        Part->SetStaticMesh(Cube);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCanEverAffectNavigation(false);
        Part->SetCastShadow(false);
        Part->SetHiddenInGame(true);
        if (BossTelegraphMID) Part->SetMaterial(0, BossTelegraphMID);
        Part->RegisterComponent();
        return Part;
    };
    for (int32 Index=0; Index<32; ++Index) BossRingSegments.Add(Segment());
    for (int32 Index=0; Index<2; ++Index) BossLaneSegments.Add(Segment());
}

void AOBEnemyCharacter::HideBossTelegraph()
{
    for (UStaticMeshComponent* Part : BossRingSegments) if (Part) Part->SetHiddenInGame(true);
    for (UStaticMeshComponent* Part : BossLaneSegments) if (Part) Part->SetHiddenInGame(true);
}

void AOBEnemyCharacter::UpdateBossTelegraph()
{
    const bool bLane = BossAttack==EOBBossAttack::RushWarning || BossAttack==EOBBossAttack::Rush;
    const bool bRing = BossAttack==EOBBossAttack::WaveWarning || BossAttack==EOBBossAttack::Shockwave;
    if (BossTelegraphMID)
    {
        BossTelegraphMID->SetVectorParameterValue(TEXT("Color"), BossAttack==EOBBossAttack::Shockwave ?
            FLinearColor(.1f,.85f,1.f) : FLinearColor(1.f,.2f,.025f));
        const UOBGameInstance* GI = Cast<UOBGameInstance>(GetGameInstance());
        BossTelegraphMID->SetScalarParameterValue(TEXT("EmissiveStrength"), GI && GI->Settings && GI->Settings->bReducedFlashes ? 1.5f : 3.f);
    }
    const float Radius = BossAttack==EOBBossAttack::Shockwave ? FMath::Max(30.f, WaveRadius) : ShockwaveRadius;
    for (int32 Index=0; Index<BossRingSegments.Num(); ++Index)
    {
        UStaticMeshComponent* Part = BossRingSegments[Index];
        Part->SetHiddenInGame(!bRing);
        if (!bRing) continue;
        const float Angle = 2.f * PI * Index / BossRingSegments.Num();
        Part->SetWorldLocation(BossAttackOrigin + FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0));
        Part->SetWorldRotation(FRotator(0,FMath::RadiansToDegrees(Angle)+90.f,0));
        Part->SetWorldScale3D(FVector((2.f*PI*Radius/BossRingSegments.Num()+2.f)/100.f,.12f,.035f));
    }
    const FVector Right(-RushDirection.Y, RushDirection.X, 0);
    const float LaneHalfWidth = GetCapsuleComponent()->GetScaledCapsuleRadius() +
        (Target.IsValid() ? Target->GetCapsuleComponent()->GetScaledCapsuleRadius() : 42.f) + 12.f;
    for (int32 Index=0; Index<BossLaneSegments.Num(); ++Index)
    {
        UStaticMeshComponent* Part = BossLaneSegments[Index];
        Part->SetHiddenInGame(!bLane);
        if (!bLane) continue;
        // Retain the original marked lane while the bull crosses it.
        const float Length = FMath::Clamp(FVector::Dist2D(GetActorLocation(), BossAttackOrigin)+RushRemainingDistance,250.f,FMath::Max(250.f,RushMaxDistance));
        Part->SetWorldLocation(BossAttackOrigin + RushDirection*(Length*.5f) + Right*(Index==0 ? -LaneHalfWidth : LaneHalfWidth));
        Part->SetWorldRotation(FRotator(0,RushDirection.Rotation().Yaw,0));
        Part->SetWorldScale3D(FVector(Length/100.f,.09f,.035f));
    }
}

void AOBEnemyCharacter::SetGlow(float Brightness)
{
    if (!AccentMID) return;
    AccentMID->SetVectorParameterValue(TEXT("Color"), bHeavy ? FLinearColor(1.f,.48f,.055f) : FLinearColor(1.f,.22f,.035f));
    AccentMID->SetScalarParameterValue(TEXT("EmissiveStrength"), Brightness);
}

void AOBEnemyCharacter::ReleaseAttackSlot()
{
    if (bHasAttackSlot && Director.IsValid()) Director->ReleaseAttackSlot(this);
    bHasAttackSlot = false;
}

void AOBEnemyCharacter::CancelAttack()
{
    ResetBossAttack();
    bCharging = false;
    BoltsRemaining = 0;
    SlotReleaseCountdown = 0.f;
    ChargeElapsed = 0.f;
    ReleaseAttackSlot();
    SetGlow(.7f);
    if (Muzzle) Muzzle->SetRelativeScale3D(FVector(.09f));
    if (!IsDead() && bActive) State = EOBEnemyState::Acquire;
}

float AOBEnemyCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!bActive || IsDead() || DamageAmount <= 0.f || !Director.IsValid() || !Director->IsCombatAllowed()) return 0.f;
    // Only the player weapon may damage a guard; enemy projectiles cannot friendly-fire.
    if (!EventInstigator || !EventInstigator->IsPlayerController()) return 0.f;
    const float Applied = FMath::Min(Health, DamageAmount);
    const bool bWasEnraged = IsBossEnraged();
    Health = FMath::Max(0.f, Health - Applied);
    if (!bWasEnraged && IsBossEnraged() && Health > 0.f)
        Director->ShowMessage(TEXT("SENTINEL OVERDRIVE  /  Recovery windows are shorter. Keep moving."), 4.f);
    // A guard hit from outside its forward cone investigates the attack once.
    // Future position updates and attacks still require its own line of sight.
    if (const APawn* Attacker = EventInstigator->GetPawn())
    {
        RememberTarget(Attacker->GetActorLocation());
        AlertNearbySquad();
    }
    FlashRemaining = .12f;
    if (HitSound) PlayCue(HitSound, .65f);
    else Director->PlaySound(TEXT("EnemyDamage"), GetActorLocation(), .65f);
    if (Health <= 0.f) Die(true);
    return Applied;
}

void AOBEnemyCharacter::Die(bool bReportEncounter)
{
    if (IsDead()) return;
    if (bHeavy && Director.IsValid() && Director->IsBossIntroActive()) Director->CancelBossIntro();
    FinishBossIntroAnimation();
    CancelAttack();
    State = EOBEnemyState::Dead;
    bActive = false;
    bAlerted = false;
    bCanSeeTarget = false;
    Health = 0.f;
    GetWorldTimerManager().ClearTimer(PerceptionTimer);
    if (AAIController* AI = Cast<AAIController>(Controller)) AI->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    SetActorEnableCollision(false);
    SetActorTickEnabled(false);
    SetGlow(.05f);
    UpdateHeavyAnimation();
    if (bReportEncounter && !bDeathReported)
    {
        bDeathReported = true;
        if (Director.IsValid())
        {
            if (DefeatSound) PlayCue(DefeatSound, .8f);
            else Director->PlaySound(TEXT("EnemyDeath"), GetActorLocation(), .8f);
            Director->ResolveEnemy(EncounterId, StableId);
        }
    }
    // Keep the actor/StableId briefly for encounter bookkeeping, but remove
    // the body and weapon after a short death beat instead of littering rooms.
    GetWorldTimerManager().SetTimer(CorpseHideTimer, this, &AOBEnemyCharacter::HideDefeatedBody,
        FMath::Clamp(CorpseVisibleSeconds, .1f, 5.f), false);
    SetLifeSpan(8.f);
}

void AOBEnemyCharacter::HideDefeatedBody()
{
    if (!IsDead()) return;
    SetActorHiddenInGame(true);
    GetMesh()->SetComponentTickEnabled(false);
}

void AOBEnemyCharacter::SetResolved()
{
    // Checkpoint restoration clears earlier actors without generating a kill event.
    Die(false);
    GetWorldTimerManager().ClearTimer(CorpseHideTimer);
    HideDefeatedBody();
}

void AOBEnemyCharacter::PlayCue(USoundBase* Cue, float Volume)
{
    if (!Cue) return;
    if (const UOBGameInstance* GI = Cast<UOBGameInstance>(GetGameInstance()))
        if (GI->Settings) Volume *= GI->Settings->MasterVolume * GI->Settings->EffectsVolume;
    UGameplayStatics::PlaySoundAtLocation(this, Cue, GetActorLocation(), Volume);
}

void AOBEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bHeavy && Director.IsValid() && Director->IsBossIntroActive()) Director->CancelBossIntro();
    bBossIntroPlaying = false;
    GetWorldTimerManager().ClearTimer(PerceptionTimer);
    GetWorldTimerManager().ClearTimer(CorpseHideTimer);
    HideBossTelegraph();
    ReleaseAttackSlot();
    // Map reload, arbitrary destruction and end play are not enemy kills.
    Super::EndPlay(EndPlayReason);
}
