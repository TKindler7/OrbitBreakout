#include "OBMech.h"
#include "OBPlayerCharacter.h"
#include "OBPilotTuning.h"
#include "OBParkourMovement.h"
#include "OBLevel2.h"
#include "OBMachineEnemy.h"
#include "OBUI.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AOBMech::AOBMech()
{
    Tuning = FSoftObjectPath(TEXT("/Game/OrbitBreakout/Tuning/DA_OBMechTuning.DA_OBMechTuning"));
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessAI = EAutoPossessAI::Disabled;
    GetCapsuleComponent()->InitCapsuleSize(70,150);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    GetCharacterMovement()->MaxWalkSpeed = 355.f;
    GetCharacterMovement()->MaxAcceleration = 780.f;
    GetCharacterMovement()->BrakingDecelerationWalking = 950.f;
    GetCharacterMovement()->MaxStepHeight = 32.f;
    bUseControllerRotationYaw = true;
    Cockpit = CreateDefaultSubobject<UCameraComponent>(TEXT("Cockpit"));
    Cockpit->SetupAttachment(GetRootComponent());
    Cockpit->SetRelativeLocation(FVector(12,0,85));
    Cockpit->bUsePawnControlRotation = true;
    Cockpit->FieldOfView = 83.f;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cab(TEXT("/Game/OrbitBreakout/Loader/SM_LoaderHull"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Thigh(TEXT("/Game/OrbitBreakout/Loader/SM_LoaderThigh"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shin(TEXT("/Game/OrbitBreakout/Loader/SM_LoaderShin"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Foot(TEXT("/Game/OrbitBreakout/Loader/SM_LoaderFoot"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Interior(TEXT("/Game/OrbitBreakout/Loader/SM_LoaderCockpit"));
    auto AttachPart = [this](FName Name, USceneComponent* Parent, UStaticMesh* Asset, FVector Pivot, bool bInterior = false)
    {
        auto* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(Parent);
        Part->SetStaticMesh(Asset);
        Part->SetRelativeLocation(Pivot);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCanEverAffectNavigation(false);
        Part->SetOnlyOwnerSee(bInterior);
        Part->SetOwnerNoSee(!bInterior);
        return Part;
    };
    AttachPart(TEXT("Cab"),GetRootComponent(),Cab.Object,FVector::ZeroVector);
    AttachPart(TEXT("CockpitTrim"),Cockpit,Interior.Object,FVector::ZeroVector,true);
    for (int32 Leg = 0; Leg < 2; ++Leg)
    {
        const FString Side = Leg == 0 ? TEXT("Port") : TEXT("Starboard");
        Thighs[Leg] = AttachPart(FName(Side + TEXT("Thigh")),GetRootComponent(),Thigh.Object,FVector(0, Leg == 0 ? -34 : 34, -13));
        Shins[Leg] = AttachPart(FName(Side + TEXT("Shin")),Thighs[Leg],Shin.Object,FVector(15,0,-51));
        Feet[Leg] = AttachPart(FName(Side + TEXT("Foot")),Shins[Leg],Foot.Object,FVector(-20,0,-56));
    }
}
void AOBMech::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Profile = Tuning.LoadSynchronous())
    {
        Armor = Profile->Armor; CannonDamage = Profile->Damage; HeatPerShot = Profile->HeatPerShot;
        GetCharacterMovement()->MaxWalkSpeed = Profile->WalkSpeed;
    }
    Pad = GetActorLocation();
    Chapter = Cast<AOBLevel2Director>(AOBMissionDirector::Get(this));
    if (Chapter.IsValid()) Chapter->WardenMech = this;
}
bool AOBMech::CanBoard(const AOBPlayerCharacter& P) const
{
    if (!Chapter.IsValid() || Chapter->MissionState != EOBLevel2State::DefeatWarden || Chapter->CurrentZone != 6 ||
        !P.IsGameplayActive() || P.bDead || P.bReloading || P.bInteractionActive || P.Parkour()->IsBusy() ||
        !P.GetCharacterMovement()->IsMovingOnGround() || P.bIsCrouched || !P.GetController() || Pilot.IsValid() || Armor <= 0 ||
        FVector::DistSquared(P.GetActorLocation(),GetActorLocation()) > FMath::Square(245.f)) return false;
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,P.GetActorLocation(),GetActorLocation(),ECC_Visibility,
        FCollisionQueryParams(SCENE_QUERY_STAT(OBBoardMech),false,&P)) || Hit.GetActor() == this;
}
bool AOBMech::Board(AOBPlayerCharacter& P)
{
    auto* PC = Cast<APlayerController>(P.GetController());
    if (!PC || !CanBoard(P)) return false;
    P.CancelActions();
    ReturnView = PC->GetControlRotation();
    PC->Possess(this);
    if (PC->GetPawn() != this) return false;
    Pilot = &P;
    P.OccupiedMech = this;
    // Leave the pilot capsule on the verified walking floor, reserving the exit.
    P.SetActorHiddenInGame(true);
    P.GetCharacterMovement()->DisableMovement();
    PC->SetControlRotation(GetActorRotation());
    PC->SetViewTarget(this);
    return true;
}
void AOBMech::Dismount()
{
    auto* PC = Cast<APlayerController>(GetController());
    auto* P = Pilot.Get();
    if (!P || !PC) return;
    PC->Possess(P);
    if (PC->GetPawn() != P) return;
    P->OccupiedMech.Reset();
    Pilot.Reset();
    P->SetActorHiddenInGame(false);
    P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    P->CancelActions();
    PC->SetControlRotation(ReturnView);
    PC->SetViewTarget(P);
    GetCharacterMovement()->StopMovementImmediately();
}
void AOBMech::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Reason == EEndPlayReason::Destroyed) Dismount();
    Super::EndPlay(Reason);
}
bool AOBMech::ControlsLive() const
{
    return Pilot.IsValid() && Chapter.IsValid() && Chapter->IsGameplayActive() && Controller && !GetWorld()->IsPaused();
}
void AOBMech::Tick(float Dt)
{
    Super::Tick(Dt);
    const float Speed = GetVelocity().Size2D();
    const float Load = FMath::Clamp(Speed / GetCharacterMovement()->MaxWalkSpeed,0.f,1.f);
    GaitWeight = FMath::FInterpTo(GaitWeight,Load,Dt,8.f);
    const float ForwardSign = FVector::DotProduct(GetVelocity(),GetActorForwardVector()) < 0.f ? -1.f : 1.f;
    Stride = FMath::Fmod(Stride + ForwardSign * Speed * Dt * (2.f * PI / 190.f),2.f * PI);
    for (int32 Leg = 0; Leg < 2; ++Leg)
    {
        const float Phase = Stride + Leg * PI;
        const float Hip = FMath::Sin(Phase) * 19.f * GaitWeight;
        const float Knee = FMath::Max(0.f,FMath::Cos(Phase)) * 24.f * GaitWeight;
        Thighs[Leg]->SetRelativeRotation(FRotator(Hip,0,0));
        Shins[Leg]->SetRelativeRotation(FRotator(-Knee,0,0));
        Feet[Leg]->SetRelativeRotation(FRotator(Knee-Hip,0,0));
    }
    Heat = FMath::Max(0.f,Heat - 18.f * Dt);
    if (Heat < 30.f) bOverheated = false;
    if (Pilot.IsValid() && (!Chapter.IsValid() || Chapter->MissionState != EOBLevel2State::DefeatWarden || Pilot->bDead)) Dismount();
}
void AOBMech::Forward(float V)
{
    if (!ControlsLive()) return;
    FVector Dir = GetActorForwardVector() * V;
    const FVector FromPad = GetActorLocation() - Pad;
    if (FromPad.Size2D() < 920.f || FVector::DotProduct(FromPad,Dir) < 0.f) AddMovementInput(Dir.GetSafeNormal(),FMath::Abs(V));
}
void AOBMech::Strafe(float V)
{
    if (!ControlsLive()) return;
    FVector Dir = GetActorRightVector() * V;
    const FVector FromPad = GetActorLocation() - Pad;
    if (FromPad.Size2D() < 920.f || FVector::DotProduct(FromPad,Dir) < 0.f) AddMovementInput(Dir.GetSafeNormal(),FMath::Abs(V));
}
void AOBMech::Turn(float V)
{
    const auto* GI = GetGameInstance<UOBGameInstance>();
    if (ControlsLive()) AddControllerYawInput(V * (GI && GI->Settings ? GI->Settings->MouseSensitivity : 1.f) * .72f);
}
void AOBMech::Look(float V)
{
    const auto* GI = GetGameInstance<UOBGameInstance>();
    if (ControlsLive()) AddControllerPitchInput(V * (GI && GI->Settings ? GI->Settings->MouseSensitivity : 1.f) * .72f *
        (GI && GI->Settings && GI->Settings->bInvertVerticalLook ? -1.f : 1.f));
}
void AOBMech::TurnStick(float V) { Turn(V * 90.f * GetWorld()->GetDeltaSeconds()); }
void AOBMech::LookStick(float V) { Look(V * 90.f * GetWorld()->GetDeltaSeconds()); }
void AOBMech::Fire()
{
    if (!ControlsLive() || bOverheated || GetWorld()->GetTimeSeconds() < NextShot) return;
    NextShot = GetWorld()->GetTimeSeconds() + .48f;
    Heat = FMath::Min(100.f,Heat + HeatPerShot);
    bOverheated = Heat >= 99.f;
    FHitResult Hit;
    const FVector Dir = GetControlRotation().Vector(), Eye = Cockpit->GetComponentLocation();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OBMechPulse),false,this);
    Params.AddIgnoredActor(Pilot.Get());
    GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye + Dir * 5000.f,ECC_Visibility,Params);
    const FVector AimPoint = Hit.bBlockingHit ? Hit.ImpactPoint + Dir * 2.f : Eye + Dir * 5000.f;
    const FVector Muzzle = GetActorLocation() + GetActorForwardVector()*70.f + GetActorRightVector()*47.f + FVector::UpVector*52.f;
    if (!GetWorld()->LineTraceSingleByChannel(Hit,GetActorLocation(),Muzzle,ECC_Visibility,Params))
        GetWorld()->LineTraceSingleByChannel(Hit,Muzzle,AimPoint,ECC_Visibility,Params);
    if (auto* Enemy = Cast<AOBMachineEnemy>(Hit.GetActor()))
        UGameplayStatics::ApplyPointDamage(Enemy,CannonDamage,Dir,Hit,Controller,this,nullptr);
    Chapter->PlaySound(TEXT("Shot"),Muzzle,.8f);
}
void AOBMech::DamageArmor(float Amount)
{
    if (!ControlsLive() || Amount <= 0.f) return;
    Armor = FMath::Max(0.f,Armor - Amount);
    if (Armor == 0.f) Dismount();
}
float AOBMech::TakeDamage(float Amount,const FDamageEvent&,AController*,AActor*)
{
    const float Before = Armor;
    DamageArmor(Amount);
    return Before - Armor;
}
void AOBMech::SetupPlayerInputComponent(UInputComponent* I)
{
    Super::SetupPlayerInputComponent(I);
    I->BindAxis(TEXT("MoveForward"),this,&AOBMech::Forward);
    I->BindAxis(TEXT("MoveRight"),this,&AOBMech::Strafe);
    I->BindAxis(TEXT("Turn"),this,&AOBMech::Turn);
    I->BindAxis(TEXT("LookUp"),this,&AOBMech::Look);
    I->BindAxis(TEXT("TurnRate"),this,&AOBMech::TurnStick);
    I->BindAxis(TEXT("LookUpRate"),this,&AOBMech::LookStick);
    I->BindAction(TEXT("Fire"),IE_Pressed,this,&AOBMech::Fire);
    I->BindAction(TEXT("Interact"),IE_Pressed,this,&AOBMech::Dismount);
}
