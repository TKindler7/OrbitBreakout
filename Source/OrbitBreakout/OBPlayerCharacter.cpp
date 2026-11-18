#include "OBPlayerCharacter.h"
#include "OBLevel2.h"
#include "OBMech.h"
#include "OBCore.h"
#include "OBParkourMovement.h"
#include "OBPilotView.h"
#include "OBEnemyCharacter.h"
#include "OBMachineEnemy.h"
#include "OBUI.h"
#include "OBRifleAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "InputCoreTypes.h"

AOBPlayerCharacter::AOBPlayerCharacter(const FObjectInitializer& Initializer)
    : Super(Initializer.SetDefaultSubobjectClass<UOBParkourMovement>(ACharacter::CharacterMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
    auto* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = true;
    Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
    Movement->JumpZVelocity = 500.f;
    Movement->AirControl = .36f;
    Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
    Movement->SetCrouchedHalfHeight(48.f);
    PilotView = CreateDefaultSubobject<UOBPilotView>(TEXT("PilotView"));
    Movement->MaxWalkSpeed = WalkSpeed;
    Movement->MaxAcceleration = 2000.f;
    Movement->BrakingDecelerationWalking = 2000.f;
    Movement->SetWalkableFloorAngle(45.f);

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 320.f;
    CameraBoom->SocketOffset = FVector(0, 35, 65);
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->bDoCollisionTest = true;
    CameraBoom->ProbeSize = 16.f;
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;
    FollowCamera->FieldOfView = 85.f;

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Meshes/Wraith.Wraith"));
    if (Manny.Succeeded()) GetMesh()->SetSkeletalMesh(Manny.Object);
    GetMesh()->SetAnimInstanceClass(UOBWraithAnimInstance::StaticClass());
    GetMesh()->SetRelativeLocation(FVector(0, 0, -96));
    GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Rifle(TEXT("/Game/Weapons/Rifle/Meshes/SM_Rifle.SM_Rifle"));
    EffectMesh = Cube.Object;
    WeaponPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ARC9Pivot"));
    // weapon_r is the mannequin's authored weapon bone under the right hand.
    // The rifle animations pose both hands around this grip, including reloads.
    WeaponPivot->SetupAttachment(GetMesh(), WeaponSocketName);
    auto Part = [&](const TCHAR* Name, USceneComponent* Parent, const FVector& Position, const FVector& Scale, UStaticMesh* Mesh)
    {
        auto* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Component->SetupAttachment(Parent);
        Component->SetStaticMesh(Mesh);
        Component->SetRelativeLocation(Position);
        Component->SetRelativeScale3D(Scale);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCanEverAffectNavigation(false);
        Component->SetCastShadow(true);
        return Component;
    };
    SightRifle = Part(TEXT("PrecisionRifle"), FollowCamera, FVector::ZeroVector, FVector::OneVector, Rifle.Object);
    SightCell = Part(TEXT("PrecisionCapacitor"), SightRifle, FVector(4,22,5), FVector(.06,.16,.06), Sphere.Object);
    for (auto* Sight : {SightRifle.Get(),SightCell.Get()})
    {
        Sight->SetOnlyOwnerSee(true);
        Sight->SetVisibility(false);
        Sight->SetCastShadow(false);
    }
    WeaponBody = Part(TEXT("ARC9Body"), WeaponPivot, FVector::ZeroVector, FVector(1.f), Rifle.Succeeded() ? Rifle.Object : Cube.Object);
    // Keep the legacy component name for existing level instances, but the rifle
    // already includes a modelled barrel. Its long axis is local +Y.
    WeaponBarrel = Part(TEXT("ARC9Barrel"), WeaponPivot, FVector::ZeroVector, FVector(1.f), nullptr);
    WeaponStrip = Part(TEXT("ARC9EnergyStrip"), WeaponPivot, FVector(2.9f,23.f,5.f), FVector(.012,.19,.018), Cube.Object);
    MuzzleGlow = Part(TEXT("ARC9Muzzle"), WeaponPivot, WeaponMuzzleOffset, FVector(.045), Sphere.Object);
    HelixShroud = Part(TEXT("HelixBarrelShroud"), WeaponPivot, FVector(0,40,3), FVector(.10,.30,.10), Cube.Object);
    HelixLeftRail = Part(TEXT("HelixLeftRail"), WeaponPivot, FVector(-6,36,5), FVector(.025,.42,.025), Cube.Object);
    HelixRightRail = Part(TEXT("HelixRightRail"), WeaponPivot, FVector(6,36,5), FVector(.025,.42,.025), Cube.Object);
    HelixCell = Part(TEXT("HelixCapacitor"), WeaponPivot, FVector(0,15,12), FVector(.095,.16,.085), Sphere.Object);
    AgentLight = Part(TEXT("RookIdentificationLight"), GetMesh(), FVector(0,0,0), FVector(.035,.10,.025), Cube.Object);
    AgentLight->SetupAttachment(GetMesh(),TEXT("spine_05"));
    AgentLight->SetRelativeLocation(FVector(0,-14,2));
    WeaponPivot->SetVisibility(false, true);
}

void AOBPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    Director = AOBMissionDirector::Get(this);
    SpawnGraceUntil = GetWorld()->GetTimeSeconds() + 2.f;
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (PC->PlayerCameraManager)
        {
            PC->PlayerCameraManager->ViewPitchMin = -65.f;
            PC->PlayerCameraManager->ViewPitchMax = 65.f;
        }
    }
    SetupVisualMaterials();
    RefreshWeaponAttachment();
    WeaponPivot->SetVisibility(bWeaponOwned, true);
    MuzzleGlow->SetVisibility(false);
    UpdateWeaponVisuals();
}

void AOBPlayerCharacter::RefreshWeaponAttachment()
{
    WeaponPivot->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Muzzle_01"));
    WeaponPivot->SetRelativeTransform(FTransform::Identity);
    MuzzleGlow->SetRelativeLocation(FVector::ZeroVector);
}

void AOBPlayerCharacter::SetupVisualMaterials()
{
    // Preserve Wraith's authored materials, including his integrated rifle.
    UMaterialInterface* Metal = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/OrbitBreakout/Materials/M_Metal.M_Metal"));
    EffectMaterial = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/OrbitBreakout/Materials/M_Emissive.M_Emissive"));
    auto Tint = [&](UStaticMeshComponent* Part, UMaterialInterface* Material, const FLinearColor& Color, float Glow)
    {
        if (!Material) return;
        auto* MID = UMaterialInstanceDynamic::Create(Material, this);
        MID->SetVectorParameterValue(TEXT("Color"), Color);
        MID->SetScalarParameterValue(TEXT("EmissiveStrength"), Glow);
        Part->SetMaterial(0, MID);
    };
    // Preserve the rifle's normal/colour textures, rather than replacing it with a flat shader.
    ArcRifleMaterial = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/OrbitBreakout/Materials/M_Arc9.M_Arc9"));
    if (ArcRifleMaterial)
    {
        WeaponBody->SetMaterial(0, ArcRifleMaterial);
        SightRifle->SetMaterial(0, ArcRifleMaterial);
    }
    Tint(HelixShroud, Metal, FLinearColor(.08,.16,.20), 0);
    Tint(HelixLeftRail, EffectMaterial, FLinearColor(.08,1,.65), 2.4f);
    Tint(HelixRightRail, EffectMaterial, FLinearColor(.08,1,.65), 2.4f);
    Tint(SightCell, EffectMaterial, FLinearColor(.15,.95,1), 1.4f);
    Tint(HelixCell, EffectMaterial, FLinearColor(.15,.95,1), 2.4f);
    Tint(WeaponBarrel, Metal, FLinearColor(.14,.20,.24), 0);
    Tint(WeaponStrip, EffectMaterial, FLinearColor(.06,.85,.78), 1.6f);
    Tint(MuzzleGlow, EffectMaterial, FLinearColor(.3,1,1), 8);
    Tint(AgentLight, EffectMaterial, FLinearColor(.1,.8,.75), 1.2f);
}

void AOBPlayerCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis(TEXT("MoveForward"), this, &AOBPlayerCharacter::MoveForward);
    Input->BindAxis(TEXT("MoveRight"), this, &AOBPlayerCharacter::MoveRight);
    Input->BindAxis(TEXT("TurnRate"), this, &AOBPlayerCharacter::TurnRate);
    Input->BindAxis(TEXT("LookUpRate"), this, &AOBPlayerCharacter::LookUpRate);
    Input->BindAxis(TEXT("Turn"), this, &AOBPlayerCharacter::Turn);
    Input->BindAxis(TEXT("LookUp"), this, &AOBPlayerCharacter::LookUp);
    Input->BindAction(TEXT("Jump"), IE_Pressed, this, &AOBPlayerCharacter::StartJump);
    Input->BindAction(TEXT("Jump"), IE_Released, this, &AOBPlayerCharacter::ReleaseJump);
    Input->BindAction(TEXT("Slide"), IE_Pressed, this, &AOBPlayerCharacter::StartSlide);
    Input->BindAction(TEXT("Sprint"), IE_Pressed, this, &AOBPlayerCharacter::StartSprint);
    Input->BindAction(TEXT("Sprint"), IE_Released, this, &AOBPlayerCharacter::StopSprint);
    Input->BindAction(TEXT("Aim"), IE_Pressed, this, &AOBPlayerCharacter::StartAim);
    Input->BindAction(TEXT("Aim"), IE_Released, this, &AOBPlayerCharacter::StopAim);
    Input->BindAction(TEXT("Fire"), IE_Pressed, this, &AOBPlayerCharacter::StartFire);
    Input->BindAction(TEXT("Fire"), IE_Released, this, &AOBPlayerCharacter::StopFire);
    Input->BindAction(TEXT("Reload"), IE_Pressed, this, &AOBPlayerCharacter::StartReload);
    Input->BindAction(TEXT("SelectArc9"), IE_Pressed, this, &AOBPlayerCharacter::SelectArc9);
    Input->BindAction(TEXT("SelectHelix12"), IE_Pressed, this, &AOBPlayerCharacter::SelectHelix12);
    Input->BindAction(TEXT("CycleWeapon"), IE_Pressed, this, &AOBPlayerCharacter::CycleWeapon);
    Input->BindAction(TEXT("Interact"), IE_Pressed, this, &AOBPlayerCharacter::StartInteract);
    Input->BindAction(TEXT("Interact"), IE_Released, this, &AOBPlayerCharacter::ReleaseInteract);
}

bool AOBPlayerCharacter::IsGameplayActive() const
{
    if (bDead || !GetWorld() || GetWorld()->IsPaused()) return false;
    const auto* PC = Cast<AOBPlayerController>(Controller);
    if (PC && PC->IsMenuOpen()) return false;
    return Director.IsValid() && Director->IsGameplayActive();
}

void AOBPlayerCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Director.IsValid()) Director = AOBMissionDirector::Get(this);
    if (bDead || OccupiedMech.IsValid()) return;
    if (IsGameplayActive())
    {
        if (GetActorLocation().Z < -1000.f)
        {
            EndSpawnGrace();
            DamagePlayer(MaxHealth + 1.f, GetActorLocation());
            return;
        }
        const APlayerController* PC = Cast<APlayerController>(Controller);
        if (PC)
        {
            if (bFireHeld && !(PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::Gamepad_RightTrigger))) StopFire();
            if (bSprintHeld && !(PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::Gamepad_LeftThumbstick))) StopSprint();
            if (bAiming && !(PC->IsInputKeyDown(EKeys::RightMouseButton) || PC->IsInputKeyDown(EKeys::Gamepad_LeftTrigger))) StopAim();
            // Pause-menu key flushes generate synthetic releases. The controller
            // retains the physical E state separately until release/focus loss.
            const AOBPlayerController* OBPC = Cast<AOBPlayerController>(PC);
            if (bInteractHeld && !(OBPC ? OBPC->IsInteractPhysicallyHeld() : PC->IsInputKeyDown(EKeys::E))) StopInteract();
        }
        UpdateInteraction(DeltaSeconds);
        UpdateSpeed();
    }
    const float Time = GetWorld()->GetTimeSeconds();
    ReloadProgress = bReloading ? FMath::Clamp((Time - ReloadStartTime) / ReloadDuration, 0.f, 1.f) : 0.f;
    PilotView->UpdateView(*this, DeltaSeconds, IsGameplayActive());
    const bool bFacingAim = bWeaponOwned && !bSprinting;
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bUseControllerDesiredRotation = bFacingAim;
    GetCharacterMovement()->bOrientRotationToMovement = !bFacingAim;
    VisualRecoil = FMath::FInterpTo(VisualRecoil, 0.f, DeltaSeconds, 18.f);
    // Aim and recoil are animated through the upper body. The grip stays fixed
    // to the right hand, so the left hand follows the authored support grip.
    // Wraith already carries an animated rifle. Do not draw a second gun over it.
    UpdateWeaponVisuals();

    // A small positional recoil avoids cumulative camera drift and respects the comfort setting.
    const auto* GI = GetGameInstance<UOBGameInstance>();
    MuzzleGlow->SetVisibility(bWeaponOwned && Time - LastShotTime < .045f && !(GI && GI->Settings && GI->Settings->bReducedFlashes));
    const float Shake = GI && GI->Settings ? GI->Settings->CameraShakeScale : .35f;
    FollowCamera->SetRelativeLocation(FVector(-VisualRecoil * 2.f * Shake, 0, VisualRecoil * Shake));
}

void AOBPlayerCharacter::MoveForward(float Value)
{
    if (!IsGameplayActive() || bInteractionActive || !Controller || FMath::IsNearlyZero(Value)) return;
    AddMovementInput(FRotationMatrix(FRotator(0,Controller->GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::X), Value);
}
void AOBPlayerCharacter::MoveRight(float Value)
{
    if (!IsGameplayActive() || bInteractionActive || !Controller || FMath::IsNearlyZero(Value)) return;
    AddMovementInput(FRotationMatrix(FRotator(0,Controller->GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::Y), Value);
}
void AOBPlayerCharacter::Turn(float Value)
{
    if (!IsGameplayActive()) return;
    const auto* GI = GetGameInstance<UOBGameInstance>();
    const float Sensitivity = GI && GI->Settings ? GI->Settings->MouseSensitivity : 1.f;
    AddControllerYawInput(Value * Sensitivity * FMath::Lerp(1.f, GI && GI->Settings ? GI->Settings->ADSSensitivity : .8f, PilotView->Alpha));
}
void AOBPlayerCharacter::LookUp(float Value)
{
    if (!IsGameplayActive()) return;
    const auto* GI = GetGameInstance<UOBGameInstance>();
    const float Sensitivity = GI && GI->Settings ? GI->Settings->MouseSensitivity : 1.f;
    const bool bInvert = GI && GI->Settings && GI->Settings->bInvertVerticalLook;
    AddControllerPitchInput(Value * Sensitivity * FMath::Lerp(1.f, GI && GI->Settings ? GI->Settings->ADSSensitivity : .8f, PilotView->Alpha) * (bInvert ? -1.f : 1.f));
}
void AOBPlayerCharacter::TurnRate(float Value)
{
    Turn(Value * 90.f * GetWorld()->GetDeltaSeconds());
}
void AOBPlayerCharacter::LookUpRate(float Value)
{
    LookUp(Value * 90.f * GetWorld()->GetDeltaSeconds());
}
UOBParkourMovement* AOBPlayerCharacter::Parkour() const { return CastChecked<UOBParkourMovement>(GetCharacterMovement()); }
bool AOBPlayerCharacter::IsParkourEnabled() const { return Director.IsValid() && Director->bEnableParkour; }
const AActor* AOBPlayerCharacter::CombatBody() const { return OccupiedMech.IsValid() ? static_cast<const AActor*>(OccupiedMech.Get()) : this; }
AOBMech* AOBPlayerCharacter::NearbyMech() const
{
    const auto* L2 = Cast<AOBLevel2Director>(Director.Get());
    return L2 && L2->WardenMech && L2->WardenMech->CanBoard(*this) ? L2->WardenMech.Get() : nullptr;
}
void AOBPlayerCharacter::StartSlide() { Parkour()->StartSlide(); }
void AOBPlayerCharacter::ReleaseJump() { Parkour()->ReleaseJump(); }
void AOBPlayerCharacter::StartJump()
{
    if (!IsGameplayActive() || bInteractionActive) return;
    if (IsParkourEnabled()) Parkour()->PressJump();
    else Jump();
}
void AOBPlayerCharacter::StartSprint()
{
    if (!IsGameplayActive() || bInteractionActive || bAiming || bFireHeld) return;
    CancelReload(); bSprintHeld = true; UpdateSpeed();
}
void AOBPlayerCharacter::StopSprint() { Parkour()->EndSlide(); bSprintHeld = false; bSprinting = false; UpdateSpeed(); }
void AOBPlayerCharacter::StartAim()
{
    if (!IsGameplayActive() || !bWeaponOwned || bInteractionActive) return;
    if (Parkour()->IsBusy()) return;
    bAiming = true; StopSprint();
}
void AOBPlayerCharacter::StopAim() { bAiming = false; UpdateSpeed(); }
void AOBPlayerCharacter::UpdateSpeed()
{
    bSprinting = bSprintHeld && !bAiming && !bFireHeld && !bReloading && !bInteractionActive;
    GetCharacterMovement()->MaxWalkSpeed = bInteractionActive ? 0.f : (bAiming ? AimSpeed : (bSprinting ? SprintSpeed : WalkSpeed));
}

void AOBPlayerCharacter::StartFire()
{
    if (Parkour()->IsBusy()) return;
    if (!IsGameplayActive() || !bWeaponOwned || bInteractionActive) return;
    bFireHeld = true; StopSprint(); FireTick();
    GetWorldTimerManager().SetTimer(FireTimer, this, &AOBPlayerCharacter::FireTick, FMath::Max(.05f, ShotInterval), true);
}
void AOBPlayerCharacter::StopFire()
{
    bFireHeld = false;
    GetWorldTimerManager().ClearTimer(FireTimer);
    UpdateSpeed();
}
void AOBPlayerCharacter::FireTick()
{
    if (!bFireHeld || !IsGameplayActive() || bInteractionActive) return;
    // Timers can run before this character's next Tick, including the first
    // frame after reloading. Never emit a stale held-trigger shot after release.
    if (const APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (!(PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::Gamepad_RightTrigger))) { StopFire(); return; }
    }
    if (bReloading || PilotView->InTransition() || (bAiming && PilotView->Alpha < .99f) || Parkour()->IsBusy()) return;
    const float Time = GetWorld()->GetTimeSeconds();
    if (Time + .001f < NextShotTime) return;
    if (MagazineAmmo <= 0)
    {
        if (ReserveAmmo > 0) StartReload();
        else if (Time >= NextEmptyCueTime)
        {
            PlayCue(TEXT("DryFire"), .5f); NextEmptyCueTime = Time + .45f;
            if (Director.IsValid()) Director->ShowMessage(TEXT("Find ammo — use a SECURITY AMMUNITION station"), 2.f);
        }
        return;
    }
    FireShot();
}

FVector AOBPlayerCharacter::GetAimDirection() const
{
    return FollowCamera ? FollowCamera->GetForwardVector() : GetActorForwardVector();
}
FVector AOBPlayerCharacter::GetMuzzleLocation() const
{
    return MuzzleGlow ? MuzzleGlow->GetComponentLocation() : GetActorLocation() + GetActorForwardVector() * 65.f + FVector(0,0,40);
}

void AOBPlayerCharacter::FireShot()
{
    if (!bWeaponOwned || bReloading || MagazineAmmo <= 0 || !IsGameplayActive()) return;
    const float Now = GetWorld()->GetTimeSeconds();
    NextShotTime = Now + ShotInterval;
    LastShotTime = Now;
    --MagazineAmmo;
    EndSpawnGrace();
    StopSprint();
    // CharacterMovement smoothly turns toward aim; never snap the mesh on a shot.

    const FVector CameraStart = FollowCamera->GetComponentLocation();
    const FVector AimDirection = FMath::VRandCone(GetAimDirection(), FMath::DegreesToRadians(bAiming ? AimSpreadDegrees : HipSpreadDegrees));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(OBPlayerBlaster), true, this);
    FHitResult CameraHit;
    const FVector MaxTarget = CameraStart + AimDirection * WeaponRange;
    GetWorld()->LineTraceSingleByChannel(CameraHit, CameraStart, MaxTarget, ECC_Visibility, Query);
    const FVector DesiredTarget = CameraHit.bBlockingHit ? CameraHit.ImpactPoint : MaxTarget;
    const FVector Muzzle = GetMuzzleLocation();
    // Trace out from the shoulder too: a muzzle pushed through thin cover cannot shoot from its far side.
    FHitResult ShoulderHit;
    const FVector Shoulder = GetActorLocation() + FVector(0,0,40.f);
    GetWorld()->LineTraceSingleByChannel(ShoulderHit, Shoulder, Muzzle, ECC_Visibility, Query);
    FHitResult ActualHit;
    FVector End;
    if (ShoulderHit.bBlockingHit)
    {
        ActualHit = ShoulderHit;
        End = ShoulderHit.ImpactPoint;
    }
    else
    {
        const FVector MuzzleDirection = (DesiredTarget - Muzzle).GetSafeNormal();
        End = Muzzle + MuzzleDirection * FMath::Min(WeaponRange, FVector::Distance(Muzzle, DesiredTarget) + 2.f);
        GetWorld()->LineTraceSingleByChannel(ActualHit, Muzzle, End, ECC_Visibility, Query);
        if (ActualHit.bBlockingHit) End = ActualHit.ImpactPoint;
    }
    bool bEnemyHit = false;
    if (AOBEnemyCharacter* Enemy = Cast<AOBEnemyCharacter>(ActualHit.GetActor()))
    {
        if (!Enemy->IsDead())
        {
            const float Applied = UGameplayStatics::ApplyPointDamage(Enemy, ShotDamage, (End-Muzzle).GetSafeNormal(), ActualHit, Controller, this, nullptr);
            bEnemyHit = Applied > 0.f;
        }
    }
    else if (AOBMachineEnemy* Machine = Cast<AOBMachineEnemy>(ActualHit.GetActor()))
    {
        if (!Machine->IsDead())
            bEnemyHit = UGameplayStatics::ApplyPointDamage(Machine, ShotDamage, (End-Muzzle).GetSafeNormal(), ActualHit, Controller, this, nullptr) > 0.f;
    }
    if (bEnemyHit) LastHitTime = Now;
    DrawShotEffect(ShoulderHit.bBlockingHit ? Shoulder : Muzzle, End, ActualHit.bBlockingHit);
    VisualRecoil = EquippedWeapon == EOBWeaponType::Helix12 ? 1.25f : 1.f;
    PlayCue(TEXT("Shot"), EquippedWeapon == EOBWeaponType::Helix12 ? .85f : .75f);
    if (MagazineAmmo == 0 && ReserveAmmo > 0)
        GetWorldTimerManager().SetTimer(AutoReloadTimer, this, &AOBPlayerCharacter::StartReload, ShotInterval, false);
}

void AOBPlayerCharacter::DrawShotEffect(const FVector& Start, const FVector& End, bool bHit)
{
    if (!EffectMesh) return;
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Tracer = GetWorld()->SpawnActor<AStaticMeshActor>((Start+End)*.5f, (End-Start).Rotation(), Params);
    if (Tracer)
    {
        auto* Mesh = Tracer->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(EffectMesh);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetCastShadow(false);
        Tracer->SetActorScale3D(FVector(FMath::Max(1.f,FVector::Distance(Start,End))/100.f,EquippedWeapon==EOBWeaponType::Helix12?.026f:.014f,EquippedWeapon==EOBWeaponType::Helix12?.026f:.014f));
        if (EffectMaterial)
        {
            auto* MID = UMaterialInstanceDynamic::Create(EffectMaterial, Tracer);
            MID->SetVectorParameterValue(TEXT("Color"), EquippedWeapon==EOBWeaponType::Helix12?FLinearColor(.12,1,.55):FLinearColor(.12,.9,1));
            MID->SetScalarParameterValue(TEXT("EmissiveStrength"), 3.f);
            Mesh->SetMaterial(0, MID);
        }
        Tracer->SetLifeSpan(.07f);
    }
    if (bHit)
    {
        auto* Spark = GetWorld()->SpawnActor<AStaticMeshActor>(End, FRotator::ZeroRotator, Params);
        if (Spark)
        {
            auto* Mesh = Spark->GetStaticMeshComponent();
            Mesh->SetMobility(EComponentMobility::Movable);
            Mesh->SetStaticMesh(EffectMesh);
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Mesh->SetCanEverAffectNavigation(false);
            Mesh->SetCastShadow(false);
            Spark->SetActorScale3D(FVector(.055));
            if (EffectMaterial) Mesh->SetMaterial(0, EffectMaterial);
            Spark->SetLifeSpan(.09f);
        }
    }
}

void AOBPlayerCharacter::StartReload()
{
    if (!IsGameplayActive() || !bWeaponOwned || bReloading || bInteractionActive || MagazineAmmo >= MagazineCapacity) return;
    if (ReserveAmmo <= 0)
    {
        if (Director.IsValid()) Director->ShowMessage(TEXT("No reserve ammo — find a supply station"), 2.f);
        return;
    }
    StopSprint();
    bReloading = true;
    ReloadStartTime = GetWorld()->GetTimeSeconds();
    ReloadProgress = 0.f;
    GetWorldTimerManager().ClearTimer(AutoReloadTimer);
    GetWorldTimerManager().SetTimer(ReloadTimer,this,&AOBPlayerCharacter::CompleteReload,FMath::Max(.1f,ReloadDuration),false);
    PlayCue(TEXT("Reload"), .6f);
}
void AOBPlayerCharacter::CompleteReload()
{
    if (!bReloading || bDead || bInteractionActive) { CancelReload(); return; }
    const int32 Transfer = FMath::Min(FMath::Max(0,MagazineCapacity-MagazineAmmo), ReserveAmmo);
    MagazineAmmo += Transfer;
    ReserveAmmo -= Transfer;
    bReloading = false;
    ReloadProgress = 0.f;
    // Existing cadence timer resumes a held trigger. A menu click cannot create a new trigger state.
    PlayCue(TEXT("Reload"), .35f);
}
void AOBPlayerCharacter::CancelReload()
{
    bReloading = false;
    ReloadProgress = 0.f;
    GetWorldTimerManager().ClearTimer(ReloadTimer);
    GetWorldTimerManager().ClearTimer(AutoReloadTimer);
}

void AOBPlayerCharacter::UpdateInteraction(float DeltaSeconds)
{
    if (!Director.IsValid()) return;
    PromptAccumulator += DeltaSeconds;
    if (PromptAccumulator >= .08f)
    {
        PromptAccumulator = 0.f;
        FocusedInteraction = Director->FindInteraction(this);
        InteractionPrompt = FocusedInteraction.IsValid() ? FocusedInteraction->GetPrompt() :
            NearbyMech() ? TEXT("E  Enter loader cockpit") : FString();
    }
    if (bInteractHeld && HeldInteraction.IsValid())
    {
        Director->UpdateHold(this,HeldInteraction.Get(),DeltaSeconds);
        bInteractionActive = Director->HoldTarget != nullptr;
        if (bInteractionActive) GetCharacterMovement()->StopMovementImmediately();
    }
    else if (bInteractionActive)
    {
        Director->CancelHold();
        bInteractionActive = false;
    }
}
void AOBPlayerCharacter::StartInteract()
{
    if (Parkour()->IsBusy() || PilotView->InTransition()) return;
    if (!IsGameplayActive() || bReloading || !Director.IsValid()) return;
    FocusedInteraction = Director->FindInteraction(this);
    if (!FocusedInteraction.IsValid()) { if (auto* Mech = NearbyMech()) Mech->Board(*this); return; }
    AOBInteractable* Target = FocusedInteraction.Get();
    // Directors own interaction rules and hold duration, including chapter-specific controls.
    if (!GetCharacterMovement()->IsMovingOnGround()) return;
    bInteractHeld = true;
    if (Director->TryInteract(this,Target) && Director->HoldTarget == Target)
    {
        HeldInteraction = Target;
        bInteractionActive = Director->HoldTarget != nullptr;
        StopFire(); StopSprint();
        GetCharacterMovement()->StopMovementImmediately();
    }
}
void AOBPlayerCharacter::GamepadContextPressed()
{
    if (!IsGameplayActive() || !Director.IsValid()) return;
    // Check fresh focus at press time. Never reload after a denied/locked interaction.
    if (Director->FindInteraction(this) || NearbyMech()) StartInteract();
    else StartReload();
}
void AOBPlayerCharacter::GamepadContextReleased() { ReleaseInteract(); }
void AOBPlayerCharacter::ReleaseInteract()
{
    if (const AOBPlayerController* PC = Cast<AOBPlayerController>(Controller))
        if (PC->IsInteractPhysicallyHeld()) return;
    StopInteract();
}
void AOBPlayerCharacter::StopInteract()
{
    bInteractHeld = false;
    bInteractionActive = false;
    HeldInteraction.Reset();
    InteractionProgress = 0.f;
    if (Director.IsValid()) Director->CancelHold();
    UpdateSpeed();
}

void AOBPlayerCharacter::ClearHeldInputPreservingInteraction()
{
    Parkour()->ResetTraversal();
    StopFire(); StopSprint(); StopAim(); StopJumping();
    PilotView->ResetView(*this);
    GetCharacterMovement()->StopMovementImmediately();
}
void AOBPlayerCharacter::CancelActions()
{
    ClearHeldInputPreservingInteraction();
    CancelReload(); StopInteract();
    InteractionPrompt.Empty();
}
void AOBPlayerCharacter::SetCrashPresentation(bool bActive, float RecoveryElapsed)
{
    bCrashPresentationActive = bActive;
    // Keep the last sample for the animation layer's blend out at handoff.
    if (bActive) CrashRecoveryElapsed = FMath::Max(0.f, RecoveryElapsed);
}
void AOBPlayerCharacter::RestoreLoadout(bool bOwned,int32 Magazine,int32 Reserve)
{
    CancelActions();
    SetCrashPresentation(false);
    bDead = false;
    Health = MaxHealth;
    CacheArcTuning();
    bHelixOwned = false;
    ArcMagazine = ArcReserve = HelixMagazine = HelixReserve = 0;
    ApplyWeaponProfile(EOBWeaponType::Arc9);
    bWeaponOwned = bOwned;
    MagazineAmmo = bOwned ? FMath::Clamp(Magazine,0,MagazineCapacity) : 0;
    ReserveAmmo = bOwned ? FMath::Clamp(Reserve,0,MaxReserveAmmo) : 0;
    SpawnGraceUntil = GetWorld()->GetTimeSeconds() + 2.f;
    WeaponPivot->SetVisibility(bOwned,true);
    MuzzleGlow->SetVisibility(false);
    UpdateWeaponVisuals();
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    UpdateSpeed();
}
void AOBPlayerCharacter::AcquireWeapon()
{
    if (bWeaponOwned || bDead) return;
    CacheArcTuning(); ApplyWeaponProfile(EOBWeaponType::Arc9);
    bWeaponOwned = true; MagazineAmmo = MagazineCapacity; ReserveAmmo = FMath::Min(96,MaxReserveAmmo);
    WeaponPivot->SetVisibility(true,true); MuzzleGlow->SetVisibility(false); UpdateWeaponVisuals();
    PlayCue(TEXT("Pickup"), .75f);
    if (Director.IsValid()) Director->WeaponAcquired();
}
void AOBPlayerCharacter::CacheArcTuning()
{
    if (bArcTuningCached) return;
    bArcTuningCached = true;
    ArcDamage = ShotDamage; ArcInterval = ShotInterval; ArcReload = ReloadDuration;
    ArcCapacity = MagazineCapacity; ArcMaxReserve = MaxReserveAmmo;
    ArcAimSpread = AimSpreadDegrees; ArcHipSpread = HipSpreadDegrees;
}
void AOBPlayerCharacter::StoreEquippedAmmo()
{
    if (EquippedWeapon == EOBWeaponType::Helix12) { HelixMagazine=MagazineAmmo; HelixReserve=ReserveAmmo; }
    else { ArcMagazine=MagazineAmmo; ArcReserve=ReserveAmmo; }
}
void AOBPlayerCharacter::ApplyWeaponProfile(EOBWeaponType Weapon)
{
    EquippedWeapon = Weapon;
    const bool bHelix = Weapon == EOBWeaponType::Helix12;
    ShotDamage = bHelix ? 36.f : ArcDamage;
    ShotInterval = bHelix ? .13f : ArcInterval;
    ReloadDuration = bHelix ? 1.7f : ArcReload;
    MagazineCapacity = bHelix ? 36 : ArcCapacity;
    MaxReserveAmmo = bHelix ? 180 : ArcMaxReserve;
    AimSpreadDegrees = bHelix ? .3f : ArcAimSpread;
    HipSpreadDegrees = bHelix ? 1.5f : ArcHipSpread;
    UpdateWeaponVisuals();
}
void AOBPlayerCharacter::UpdateWeaponVisuals()
{
    const bool bHelix = bWeaponOwned && EquippedWeapon == EOBWeaponType::Helix12;
    for (UStaticMeshComponent* Part : {WeaponBody.Get(),WeaponBarrel.Get(),WeaponStrip.Get(),AgentLight.Get()})
        if (Part) Part->SetVisibility(false);
    // Fit the HELIX upgrade around Wraith's authored muzzle, whose +X axis is forward.
    for (UStaticMeshComponent* Part : {HelixShroud.Get(),HelixLeftRail.Get(),HelixRightRail.Get(),HelixCell.Get()})
        if (Part) Part->SetVisibility(bHelix);
    if (HelixShroud) { HelixShroud->SetRelativeLocation(FVector(-7,0,0)); HelixShroud->SetRelativeScale3D(FVector(.16,.14,.14)); }
    if (HelixLeftRail) { HelixLeftRail->SetRelativeLocation(FVector(-12,-7,3)); HelixLeftRail->SetRelativeScale3D(FVector(.26,.018,.025)); }
    if (HelixRightRail) { HelixRightRail->SetRelativeLocation(FVector(-12,7,3)); HelixRightRail->SetRelativeScale3D(FVector(.26,.018,.025)); }
    if (HelixCell) { HelixCell->SetRelativeLocation(FVector(-22,0,9)); HelixCell->SetRelativeScale3D(FVector(.08,.10,.07)); }
    if (MuzzleGlow) MuzzleGlow->SetRelativeLocation(FVector::ZeroVector);

}
bool AOBPlayerCharacter::EquipWeapon(EOBWeaponType Weapon)
{
    if (!bWeaponOwned || bDead || bInteractionActive || (Weapon!=EOBWeaponType::Arc9 && Weapon!=EOBWeaponType::Helix12) ||
        (Weapon==EOBWeaponType::Helix12 && !bHelixOwned)) return false;
    if (Weapon==EquippedWeapon) return true;
    CacheArcTuning();
    StopFire(); CancelReload(); StopSprint();
    StoreEquippedAmmo();
    ApplyWeaponProfile(Weapon);
    MagazineAmmo=Weapon==EOBWeaponType::Helix12?HelixMagazine:ArcMagazine;
    ReserveAmmo=Weapon==EOBWeaponType::Helix12?HelixReserve:ArcReserve;
    // Switching cannot bypass the outgoing weapon's fire cadence.
    NextShotTime=FMath::Max(NextShotTime,GetWorld()->GetTimeSeconds()+.25f);
    return true;
}
void AOBPlayerCharacter::SelectArc9() { if(IsGameplayActive()) EquipWeapon(EOBWeaponType::Arc9); }
void AOBPlayerCharacter::SelectHelix12() { if(IsGameplayActive()) EquipWeapon(EOBWeaponType::Helix12); }
void AOBPlayerCharacter::CycleWeapon() { if(IsGameplayActive() && bHelixOwned) EquipWeapon(EquippedWeapon==EOBWeaponType::Arc9?EOBWeaponType::Helix12:EOBWeaponType::Arc9); }
FString AOBPlayerCharacter::GetWeaponName() const { return EquippedWeapon==EOBWeaponType::Helix12?TEXT("HELIX-12 / PLASMA"):TEXT("ARC-9 / BLASTER"); }
int32 AOBPlayerCharacter::GetArcReserveAmmo() const { return EquippedWeapon==EOBWeaponType::Arc9?ReserveAmmo:ArcReserve; }
int32 AOBPlayerCharacter::GetHelixReserveAmmo() const { return EquippedWeapon==EOBWeaponType::Helix12?ReserveAmmo:HelixReserve; }
int32 AOBPlayerCharacter::GetArcMagazineAmmo() const { return EquippedWeapon==EOBWeaponType::Arc9?MagazineAmmo:ArcMagazine; }
int32 AOBPlayerCharacter::GetHelixMagazineAmmo() const { return EquippedWeapon==EOBWeaponType::Helix12?MagazineAmmo:HelixMagazine; }
void AOBPlayerCharacter::RestoreCampaignLoadout(bool bArc,int32 ArcMag,int32 InArcReserve,bool bHelix,int32 HelixMag,int32 InHelixReserve,EOBWeaponType Selected)
{
    RestoreLoadout(bArc,ArcMag,InArcReserve);
    // RestoreLoadout deliberately clears inventory; set stored magazines afterwards.
    ArcMagazine=MagazineAmmo; ArcReserve=ReserveAmmo;
    bHelixOwned=bArc && bHelix;
    HelixMagazine=bHelixOwned?FMath::Clamp(HelixMag,0,36):0;
    HelixReserve=bHelixOwned?FMath::Clamp(InHelixReserve,0,180):0;
    if (bHelixOwned && Selected==EOBWeaponType::Helix12) EquipWeapon(Selected);
    UpdateWeaponVisuals();
}
void AOBPlayerCharacter::RestoreHeavyWeapon(bool bOwned,int32 Reserve,bool bEquip)
{
    if (!bOwned || !bWeaponOwned) return;
    bHelixOwned=true; HelixMagazine=36; HelixReserve=FMath::Clamp(Reserve,0,180);
    if (bEquip) EquipWeapon(EOBWeaponType::Helix12);
    UpdateWeaponVisuals();
}
void AOBPlayerCharacter::AcquireHeavyWeapon()
{
    if (!bWeaponOwned || bHelixOwned || bDead) return;
    RestoreHeavyWeapon(true,144,true);
    PlayCue(TEXT("Pickup"),.85f);
    if (Director.IsValid()) Director->ShowMessage(TEXT("HELIX-12 acquired. Q or 1 / 2 switches weapons."),6.f);
}
bool AOBPlayerCharacter::AddHealth(float Amount)
{
    if (bDead || Amount <= 0 || Health >= MaxHealth) return false;
    Health = FMath::Clamp(Health + Amount,0.f,MaxHealth); return true;
}
bool AOBPlayerCharacter::AddAmmo(int32 Amount)
{
    if (bDead || !bWeaponOwned || Amount <= 0 || ReserveAmmo >= MaxReserveAmmo) return false;
    ReserveAmmo = FMath::Clamp(ReserveAmmo + Amount,0,MaxReserveAmmo); return true;
}
float AOBPlayerCharacter::TakeDamage(float Amount,const FDamageEvent& Event,AController* Instigator,AActor* Causer)
{
    const float Before = Health;
    DamagePlayer(Amount,Causer ? Causer->GetActorLocation() : GetActorLocation());
    return FMath::Max(0.f,Before-Health);
}
void AOBPlayerCharacter::DamagePlayer(float Amount,FVector SourceLocation)
{
    if (OccupiedMech.IsValid()) { OccupiedMech->DamageArmor(Amount); return; }
    if (Amount <= 0 || bDead || !IsGameplayActive() || GetWorld()->GetTimeSeconds() < SpawnGraceUntil) return;
    Health = FMath::Clamp(Health - Amount,0.f,MaxHealth);
    LastDamageTime = GetWorld()->GetTimeSeconds();
    DamageDirection = (SourceLocation-GetActorLocation()).GetSafeNormal();
    PlayCue(TEXT("Damage"), .65f);
    if (Parkour()->IsBusy()) Parkour()->ResetTraversal();
    if (bInteractionActive) StopInteract();
    if (Health <= 0.f)
    {
        bDead = true;
        CancelActions();
        GetCharacterMovement()->DisableMovement();
        if (Director.IsValid()) Director->PlayerDied();
    }
}
void AOBPlayerCharacter::PlayCue(FName Cue,float Volume)
{
    if (Director.IsValid()) Director->PlaySound(Cue,GetActorLocation(),Volume);
}
void AOBPlayerCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(FireTimer);
    GetWorldTimerManager().ClearTimer(ReloadTimer);
    GetWorldTimerManager().ClearTimer(AutoReloadTimer);
    Super::EndPlay(Reason);
}
