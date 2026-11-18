#include "OBMachineEnemy.h"
#include "OBParkourMovement.h"
#include "OBEnemyProjectile.h"
#include "OBPlayerCharacter.h"
#include "OBRifleAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "OBCore.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float PulseRadius = 255.f;
    // Equipment uses the authored rifle pose to derive a stable bone-local mount.
    FTransform ReferenceBone(UAnimSequence* Pose, FName Bone)
    {
        if (!Pose || !Pose->GetSkeleton()) return FTransform::Identity;
        const FReferenceSkeleton& Ref = Pose->GetSkeleton()->GetReferenceSkeleton();
        FTransform Result = FTransform::Identity;
        for (int32 Index = Ref.FindBoneIndex(Bone); Index != INDEX_NONE; Index = Ref.GetParentIndex(Index))
        {
            FTransform Local;
            Pose->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Index), FAnimExtractContext(0.0, false), false);
            Result = Result * Local;
        }
        return Result;
    }

    void FitEquipment(UStaticMeshComponent* Component, FVector Size)
    {
        if (!Component->GetStaticMesh()) return;
        const FBox Bounds = Component->GetStaticMesh()->GetBoundingBox();
        const FVector Actual = Bounds.GetSize();
        const FVector Scale(Size.X/FMath::Max(Actual.X,.01f), Size.Y/FMath::Max(Actual.Y,.01f), Size.Z/FMath::Max(Actual.Z,.01f));
        Component->SetRelativeScale3D(Scale);
        Component->SetRelativeLocation(-Bounds.GetCenter()*Scale);
    }

}

AOBMachineEnemy::AOBMachineEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    bUseControllerRotationYaw = false;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AAIController::StaticClass();
    Tags.Add(TEXT("OB_LEVEL2_MACHINE"));
    GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
    GetMesh()->SetVisibility(true);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->MaxAcceleration = 1400.f;
    GetCharacterMovement()->BrakingDecelerationWalking = 1800.f;
    GetCharacterMovement()->bUseRVOAvoidance = true;
    GetCharacterMovement()->AvoidanceConsiderationRadius = 200.f;
    GetCharacterMovement()->SetAvoidanceGroup(2);
    GetCharacterMovement()->MaxStepHeight = 40.f;
    Body = CreateDefaultSubobject<USceneComponent>(TEXT("MachineAssembly"));
    Body->SetupAttachment(GetCapsuleComponent());
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Trooper(TEXT("/Game/SciFITrooper_Man_03/SkeletalMesh/SK_SciFITrooper_Man_03.SK_SciFITrooper_Man_03"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> AuthoredRifle(TEXT("/Game/Weapons/Rifle/Meshes/SM_Rifle.SM_Rifle"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ArmorShell(TEXT("/Game/SciFITrooper_Man_03/Modulars/SM/SM_Armor_full.SM_Armor_full"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ShieldPanel(TEXT("/Game/SciFITrooper_Man_03/Modulars/SM/SM_Armor_full_2.SM_Armor_full_2"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Top(TEXT("/Game/SciFITrooper_Man_03/Materials/MI/MI_Sci-FITropperMan-03_Top_3.MI_Sci-FITropperMan-03_Top_3"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Bottom(TEXT("/Game/SciFITrooper_Man_03/Materials/MI/MI_Sci-FITropperMan-03_Bottom_3.MI_Sci-FITropperMan-03_Bottom_3"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Dash(TEXT("/Game/OrbitBreakout/Animations/Trooper/MM_Dash.MM_Dash"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> RiflePose(TEXT("/Game/OrbitBreakout/Animations/Trooper/MF_Rifle_Idle_ADS.MF_Rifle_Idle_ADS"));
    TrooperMesh=Trooper.Object; RifleMesh=AuthoredRifle.Object; ReinforcedArmorMesh=ArmorShell.Object; ShieldMesh=ShieldPanel.Object;
    TrooperTopMaterial=Top.Object; TrooperBottomMaterial=Bottom.Object; LungeAnimation=Dash.Object; EquipmentReferencePose=RiflePose.Object;
    GetMesh()->SetSkeletalMesh(TrooperMesh);
    RifleGrip=CreateDefaultSubobject<USceneComponent>(TEXT("TrooperRifleGrip"));
    RifleGrip->SetupAttachment(GetMesh(),TEXT("hand_r"));
    ShieldMount=CreateDefaultSubobject<USceneComponent>(TEXT("TrooperShieldMount"));
    ShieldMount->SetupAttachment(GetMesh(),TEXT("lowerarm_l"));
    auto Equipment = [&](const TCHAR* Name, USceneComponent* Parent)
    {
        UStaticMeshComponent* Part=CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(Parent);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCanEverAffectNavigation(false);
        return Part;
    };
    Rifle=Equipment(TEXT("TrooperAuthoredRifle"),RifleGrip);
    Shield=Equipment(TEXT("TrooperBallisticShield"),ShieldMount);
    ReinforcedArmor=Equipment(TEXT("TrooperReinforcedArmor"),GetMesh());
    MuzzleFlash=Equipment(TEXT("TrooperMuzzleFlash"),Rifle);
    StatusLight=Equipment(TEXT("TrooperStatusLight"),GetMesh());
    MuzzleFlash->SetCastShadow(false); StatusLight->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    for (int32 I = 0; I < 24; ++I)
    {
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("MachinePart%02d"), I));
        Part->SetupAttachment(Body);
        Part->SetStaticMesh((I >= 6 && I <= 9) || I == 15 || I == 18 ? Cylinder.Object : I == 5 ? Sphere.Object : Cube.Object);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCanEverAffectNavigation(false);
        Parts.Add(Part);
        UStaticMeshComponent* Segment = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("WarningSegment%02d"), I));
        Segment->SetupAttachment(GetCapsuleComponent());
        Segment->SetStaticMesh(Cube.Object);
        Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Segment->SetCanEverAffectNavigation(false);
        Segment->SetCastShadow(false);
        Segment->SetVisibility(false);
        WarningSegments.Add(Segment);
    }
}

void AOBMachineEnemy::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ConfigureVisuals();
}

void AOBMachineEnemy::ConfigureVisuals()
{
    const bool Cutter=Kind==EOBMachineKind::Cutter;
    const bool Watcher=Kind==EOBMachineKind::Watcher;
    const bool Warden=Kind==EOBMachineKind::Warden;
    const bool Heavy=!Cutter && !Watcher;
    const float Half=Cutter ? 90.f : Watcher ? 94.f : Warden ? 112.f : 104.f;
    const float Scale=Cutter ? .96f : Watcher ? 1.f : Warden ? 1.17f : 1.08f;
    GetCapsuleComponent()->SetCapsuleSize(Cutter ? 34.f : Watcher ? 36.f : Warden ? 44.f : 42.f,Half);
    GetCharacterMovement()->MaxWalkSpeed=Cutter ? 430.f : Watcher ? 260.f : Warden ? 190.f : 170.f;
    MaxHealth=Cutter ? 70.f : Watcher ? 130.f : Warden ? 650.f : 260.f;
    if (!HasActorBegunPlay()) Health=MaxHealth;
    // Preserve serialized subobject names in the existing level, never their block bodies.
    Body->SetRelativeTransform(FTransform::Identity);
    for (UStaticMeshComponent* Part : Parts)
    {
        Part->SetVisibility(false); Part->SetHiddenInGame(true);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCanEverAffectNavigation(false);
    }
    USkeletalMeshComponent* Mesh=GetMesh();
    Mesh->SetSkeletalMesh(TrooperMesh);
    Mesh->SetRelativeLocation(FVector(0,0,-Half));
    Mesh->SetRelativeRotation(FRotator(0,-90,0));
    Mesh->SetRelativeScale3D(FVector(Scale));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCanEverAffectNavigation(false);
    Mesh->SetVisibility(true); Mesh->SetHiddenInGame(false);
    Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    Mesh->SetAnimInstanceClass(UOBRifleAnimInstance::StaticClass());
    Mesh->EmptyOverrideMaterials();
    UniformMIDs.Reset();
    // These paintable vendor instances retain the authored normal, ORM and wear maps.
    const FLinearColor Base=Cutter ? FLinearColor(.045f,.05f,.055f) : Watcher ? FLinearColor(.085f,.105f,.12f) : Warden ? FLinearColor(.035f,.04f,.048f) : FLinearColor(.11f,.12f,.12f);
    const FLinearColor Armor=Cutter ? FLinearColor(.14f,.15f,.14f) : Watcher ? FLinearColor(.20f,.22f,.24f) : Warden ? FLinearColor(.32f,.35f,.38f) : FLinearColor(.25f,.26f,.25f);
    const FLinearColor Mark=Cutter ? FLinearColor(.34f,.23f,.075f) : Watcher ? FLinearColor(.26f,.045f,.035f) : Warden ? FLinearColor(.32f,.055f,.045f) : FLinearColor(.36f,.30f,.15f);
    for (int32 Index=0;Index<Mesh->GetNumMaterials();++Index)
    {
        UMaterialInterface* Parent=Index==0 ? TrooperTopMaterial.Get() : TrooperBottomMaterial.Get();
        if (auto* Material=Mesh->CreateDynamicMaterialInstance(Index,Parent))
        {
            Material->SetVectorParameterValue(TEXT("Color1"),Base);
            Material->SetVectorParameterValue(TEXT("Color2"),Armor);
            Material->SetVectorParameterValue(TEXT("Color3"),Mark);
            Material->SetScalarParameterValue(TEXT("emissivePower"),0.f);
            Material->SetScalarParameterValue(TEXT("EmissiveMultiplyer"),0.f);
            UniformMIDs.Add(Material);
        }
    }
    const bool WeaponBone=Mesh->GetBoneIndex(TEXT("weapon_r"))!=INDEX_NONE;
    RifleGrip->AttachToComponent(Mesh,FAttachmentTransformRules::KeepRelativeTransform,WeaponBone ? TEXT("weapon_r") : TEXT("hand_r"));
    FTransform Grip(FRotator(0,-90,0),FVector::ZeroVector);
    if (!WeaponBone) Grip=Grip*UOBRifleAnimInstance::GetWeaponBoneInRightHand();
    RifleGrip->SetRelativeTransform(Grip);
    Rifle->SetStaticMesh(RifleMesh); Rifle->SetRelativeTransform(FTransform::Identity);
    Rifle->EmptyOverrideMaterials(); Rifle->SetVisibility(true); Rifle->SetHiddenInGame(false);
    // Authored rifle barrel coordinates are +Y. Socket fallback is the actual tip.
    const FName MuzzleSocket=Rifle->DoesSocketExist(TEXT("Muzzle")) ? FName(TEXT("Muzzle")) : Rifle->DoesSocketExist(TEXT("muzzle")) ? FName(TEXT("muzzle")) : FName(NAME_None);
    MuzzleFlash->AttachToComponent(Rifle,FAttachmentTransformRules::SnapToTargetNotIncludingScale,MuzzleSocket);
    MuzzleFlash->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
    MuzzleFlash->SetRelativeLocation(MuzzleSocket.IsNone() ? FVector(0,52,3) : FVector::ZeroVector);
    MuzzleFlash->SetRelativeScale3D(FVector(.025f));
    MuzzleFlash->SetVisibility(false); MuzzleFlash->SetHiddenInGame(false);
    // Imported chest shell adds real modeled armor to the two heavy variants.
    ReinforcedArmor->SetStaticMesh(ReinforcedArmorMesh);
    ReinforcedArmor->AttachToComponent(Mesh,FAttachmentTransformRules::KeepRelativeTransform,TEXT("spine_03"));
    if (ReinforcedArmorMesh)
    {
        const FVector ShellScale(Warden ? 1.16f : 1.09f,Warden ? 1.16f : 1.10f,1.04f);
        const FVector ShellCenter(0,-9.6f,120.8f);
        const FTransform ShellInMesh(FQuat::Identity,ShellCenter-ReinforcedArmorMesh->GetBoundingBox().GetCenter()*ShellScale,ShellScale);
        ReinforcedArmor->SetRelativeTransform(ShellInMesh.GetRelativeTransform(ReferenceBone(EquipmentReferencePose,TEXT("spine_03"))));
    }
    if (UniformMIDs.Num()) ReinforcedArmor->SetMaterial(0,UniformMIDs[0]);
    ReinforcedArmor->SetVisibility(Heavy); ReinforcedArmor->SetHiddenInGame(!Heavy);
    Shield->SetStaticMesh(ShieldMesh); Shield->EmptyOverrideMaterials();
    if (UniformMIDs.Num()) Shield->SetMaterial(0,UniformMIDs[0]);
    ShieldMount->AttachToComponent(Mesh,FAttachmentTransformRules::KeepRelativeTransform,TEXT("lowerarm_l"));
    // In the rifle idle pose this is a forearm-supported shield in front of the left torso.
    const FTransform ShieldInMesh(FRotator::ZeroRotator,FVector(34,44,111));
    ShieldGripTransform=ShieldInMesh.GetRelativeTransform(ReferenceBone(EquipmentReferencePose,TEXT("lowerarm_l")));
    ShieldMount->SetRelativeTransform(ShieldGripTransform);
    Shield->SetRelativeRotation(FRotator::ZeroRotator);
    FitEquipment(Shield,Warden ? FVector(76,25,126) : FVector(68,22,114));
    Shield->SetVisibility(Heavy); Shield->SetHiddenInGame(!Heavy);
    StatusLight->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    StatusLight->AttachToComponent(Mesh,FAttachmentTransformRules::KeepRelativeTransform,TEXT("spine_03"));
    const FTransform LightInMesh(FQuat::Identity,FVector(-12,20,133),FVector(.045f,.018f,.025f));
    StatusLight->SetRelativeTransform(LightInMesh.GetRelativeTransform(ReferenceBone(EquipmentReferencePose,TEXT("spine_03"))));
    StatusLight->SetVisibility(true); StatusLight->SetHiddenInGame(false);
    UMaterialInterface* Glow=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/OrbitBreakout/Materials/M_Emissive.M_Emissive"));
    if (Glow)
    {
        AccentMID=UMaterialInstanceDynamic::Create(Glow,this);
        CoreMID=UMaterialInstanceDynamic::Create(Glow,this);
        WarningMID=UMaterialInstanceDynamic::Create(Glow,this);
        AccentMID->SetVectorParameterValue(TEXT("Color"),FLinearColor(.8f,.46f,.18f));
        CoreMID->SetVectorParameterValue(TEXT("Color"),Warden ? FLinearColor(.55f,.62f,.67f) : Mark);
        CoreMID->SetScalarParameterValue(TEXT("EmissiveStrength"),.5f);
        WarningMID->SetVectorParameterValue(TEXT("Color"),FLinearColor(1.f,.22f,.02f));
        WarningMID->SetScalarParameterValue(TEXT("EmissiveStrength"),3.f);
        MuzzleFlash->SetMaterial(0,AccentMID); StatusLight->SetMaterial(0,CoreMID);
        for (UStaticMeshComponent* Segment:WarningSegments) Segment->SetMaterial(0,WarningMID);
    }
}

void AOBMachineEnemy::BeginPlay()
{
    Super::BeginPlay();
    ConfigureVisuals();
    SnapToFloor();
    InitialLocation = LastProgressLocation = GetActorLocation();
    InitialFacing = GetActorRotation();
    bSpawnRecorded = true;
    Health = MaxHealth;
    Director = AOBMissionDirector::Get(this);
    SetActorHiddenInGame(true);
    GetMesh()->SetComponentTickEnabled(false);
    SetActorEnableCollision(false);
    GetCharacterMovement()->DisableMovement();
    State = EOBMachineState::Dormant;
    bActive = false;
}

void AOBMachineEnemy::SnapToFloor()
{
    if (!GetWorld()) return;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OBMachineFloor),false,this);
    // Only architecture is a floor. Shared dormant entry slots can overlap other
    // capsules, which must never lift a newly configured trooper into the air.
    if (GetWorld()->LineTraceSingleByObjectType(Hit,GetActorLocation()+FVector(0,0,120),GetActorLocation()-FVector(0,0,400),FCollisionObjectQueryParams(ECC_WorldStatic),Params)
        && Hit.ImpactNormal.Z > .55f)
        SetActorLocation(Hit.ImpactPoint+FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f),false,nullptr,ETeleportType::TeleportPhysics);
}

void AOBMachineEnemy::Activate()
{
    if (IsDead() || bActive) return;
    if (!bSpawnRecorded) { InitialLocation=GetActorLocation(); InitialFacing=GetActorRotation(); bSpawnRecorded=true; }
    if (!Director.IsValid()) Director=AOBMissionDirector::Get(this);
    bActive = true;
    State = EOBMachineState::Idle;
    SetActorTickEnabled(true);
    Health = MaxHealth;
    SetActorHiddenInGame(false);
    GetMesh()->SetComponentTickEnabled(true);
    SetActorEnableCollision(true);
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    if (!Controller && GetWorld() && HasActorBegunPlay()) SpawnDefaultController();
    NextAttackTime = GetWorld()->GetTimeSeconds() + (Kind==EOBMachineKind::Warden ? 5.f : 1.1f);
    if (EncounterId==205 || (EncounterId>=210 && EncounterId<=213))
    {
        // An announced security arrival receives one position from the alarm.
        // It must leave its occluded entry before ordinary sight can acquire.
        if (AOBPlayerCharacter* ReportedPlayer=Cast<AOBPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))
        {
            if (!ReportedPlayer->bDead)
            {
                Target=ReportedPlayer;
                BeginAnnouncedInvestigation(ReportedPlayer->CombatBody()->GetActorLocation());
            }
        }
    }
    // The elite's in-world reveal preserves control and gives a five-second safe read.
    if (Kind==EOBMachineKind::Warden && Director.IsValid()) Director->ShowMessage(TEXT("RELAY WARDEN  /  Flank its shield. Leave marked pulse zones."),5.f);
    Cue(TEXT("Door"),.65f);
}

void AOBMachineEnemy::SetResolved()
{
    Die(false);
    SetActorHiddenInGame(true);
    GetMesh()->SetComponentTickEnabled(false);
    SetActorTickEnabled(false);
}

void AOBMachineEnemy::ResetToSpawn()
{
    CancelAttack();
    if (bSpawnRecorded) { SetActorLocation(InitialLocation,false,nullptr,ETeleportType::TeleportPhysics); SetActorRotation(InitialFacing); }
    bActive=bAlerted=bVisibleTarget=bReported=bLungeHit=false;
    Health=MaxHealth;
    State=EOBMachineState::Dormant;
    AttackPattern=PulseNumber=BurstRemaining=0;
    CoreExposedRemaining=DeathElapsed=StuckElapsed=RecoveryRemaining=0;
    LastKnownTime=LastHeardShot=InitialInvestigationUntil=-100.f;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetActorTickEnabled(true);
    GetCharacterMovement()->DisableMovement();
    ConfigureVisuals();
    LastShotTime=-100.f;
    GetMesh()->SetComponentTickEnabled(false);
}

bool AOBMachineEnemy::HasLineTo(const FVector& Point,const AActor* AllowedHit) const
{
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OBMachineSight),false,this);
    const FVector Source = GetActorLocation()+FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*.52f);
    return !GetWorld()->LineTraceSingleByChannel(Hit,Source,Point,ECC_Visibility,Params) || (AllowedHit && Hit.GetActor()==AllowedHit);
}

bool AOBMachineEnemy::HasSight(const AOBPlayerCharacter* Player) const
{
    if (!Player || Player->bDead || !bActive || IsDead()) return false;
    const FVector Offset=Player->CombatBody()->GetActorLocation()-GetActorLocation();
    if (Offset.Size2D()>SightRange || FMath::Abs(Offset.Z)>(Player->Parkour()->IsBusy() ? 410.f : 260.f)) return false;
    const bool Recent = GetWorld()->GetTimeSeconds()-LastKnownTime<AwarenessMemorySeconds;
    if (!Recent && Offset.Size2D()>650.f && FVector::DotProduct(Offset.GetSafeNormal2D(),GetActorForwardVector())<.12f) return false;
    return HasLineTo(Player->CombatBody()->GetActorLocation()+FVector(0,0,24),Player->CombatBody());
}

void AOBMachineEnemy::Remember(const FVector& Position,bool bShare)
{
    LastKnown=Position;
    LastKnownTime=GetWorld()->GetTimeSeconds();
    bAlerted=true;
    if (!bShare || LastKnownTime-LastAlertTime<1.f) return;
    LastAlertTime=LastKnownTime;
    for (TActorIterator<AOBMachineEnemy> It(GetWorld());It;++It)
    {
        if (*It==this || !It->bActive || It->IsDead() || It->EncounterId!=EncounterId || It->Kind==EOBMachineKind::Warden) continue;
        if (FVector::DistSquared(It->GetActorLocation(),GetActorLocation())<FMath::Square(1200.f)) It->Remember(Position,false);
    }
}

void AOBMachineEnemy::BeginAnnouncedInvestigation(const FVector& LastReportedPosition)
{
    if (!bActive || IsDead() || (EncounterId!=205 && (EncounterId<210 || EncounterId>213))) return;
    Remember(LastReportedPosition,false);
    // Long entry routes need enough time to reach the reported area. This is a
    // fixed destination with a bounded expiry, never live through-wall tracking.
    const float TravelSeconds=FVector::Dist2D(GetActorLocation(),LastReportedPosition)/FMath::Max(1.f,GetCharacterMovement()->MaxWalkSpeed);
    InitialInvestigationUntil=GetWorld()->GetTimeSeconds()+FMath::Clamp(TravelSeconds+3.f,8.f,20.f);
    bVisibleTarget=false;
    State=EOBMachineState::Search;
    NextMoveTime=0;
    RequestMove(LastKnown,100.f);
}

void AOBMachineEnemy::UpdatePerception()
{
    if (!Target.IsValid()) Target = Director.IsValid() && Director->Player ? Director->Player.Get() : Cast<AOBPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if (!Target.IsValid() || Target->bDead) { bVisibleTarget=false; return; }
    const float Now=GetWorld()->GetTimeSeconds();
    const float Distance=FVector::Dist2D(GetActorLocation(),Target->CombatBody()->GetActorLocation());
    bVisibleTarget=HasSight(Target.Get());
    if (bVisibleTarget) { InitialInvestigationUntil=-100.f; Remember(Target->CombatBody()->GetActorLocation(),true); }
    else if (FMath::Abs(Target->CombatBody()->GetActorLocation().Z - GetActorLocation().Z) <= 260.f && Distance<1500.f && Target->LastShotTime>LastHeardShot && Now-Target->LastShotTime<.4f)
    { LastHeardShot=Target->LastShotTime; Remember(Target->CombatBody()->GetActorLocation(),true); }
    else if (FMath::Abs(Target->CombatBody()->GetActorLocation().Z - GetActorLocation().Z) <= 260.f && Distance<(Target->bSprinting ? 650.f : 330.f) && Target->CombatBody()->GetVelocity().SizeSquared2D()>FMath::Square(240.f))
        Remember(Target->CombatBody()->GetActorLocation(),false);
    bAlerted=Now-LastKnownTime<AwarenessMemorySeconds || Now<InitialInvestigationUntil;
    if (State==EOBMachineState::Windup || State==EOBMachineState::Burst)
    {
        if (!bVisibleTarget) { CancelAttack(); NextAttackTime=Now+.6f; }
        return;
    }
    if (State==EOBMachineState::Lunge || State==EOBMachineState::PulseWarning || State==EOBMachineState::Recovery) return;
    if (!bAlerted)
    {
        State=EOBMachineState::Idle;
        if (FVector::Dist2D(GetActorLocation(),InitialLocation)>240.f) RequestMove(InitialLocation,100.f);
        else if (AAIController* AI=Cast<AAIController>(Controller)) AI->StopMovement();
        return;
    }
    if (!bVisibleTarget) { State=EOBMachineState::Search; RequestMove(LastKnown,95.f); return; }
    const float Range=Kind==EOBMachineKind::Cutter ? 400.f : Kind==EOBMachineKind::Watcher ? 1250.f : 1450.f;
    // A large acceptance radius can finish at an intermediate corner before
    // entering attack range. Let perception stop the move at combat distance.
    if (Distance>Range) { State=EOBMachineState::Approach; RequestMove(LastKnown,ApproachAcceptanceRadius); }
    else if (Now>=NextAttackTime) BeginAttack();
    else if (Kind==EOBMachineKind::Warden && NextAttackTime-Now>1.f) RequestMove(LastKnown,530.f);
}

void AOBMachineEnemy::RequestMove(const FVector& Destination,float Acceptance)
{
    const float Now=GetWorld()->GetTimeSeconds();
    if (Now<NextMoveTime) return;
    NextMoveTime=Now+.65f;
    if (AAIController* AI=Cast<AAIController>(Controller))
    {
        const auto Result=AI->MoveToLocation(Destination,Acceptance,true,true,true,true,nullptr,false);
        if (Result==EPathFollowingRequestResult::Failed)
            if (auto* Nav=UNavigationSystemV1::GetCurrent(GetWorld()))
            { FNavLocation Point; if (Nav->GetRandomReachablePointInRadius(GetActorLocation(),250.f,Point)) AI->MoveToLocation(Point.Location,45.f); }
    }
}

void AOBMachineEnemy::Reposition()
{
    if (!Target.IsValid() || Kind!=EOBMachineKind::Watcher) return;
    if (auto* Nav=UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        const FVector Forward=(LastKnown-GetActorLocation()).GetSafeNormal2D();
        const FVector Side=FVector::CrossProduct(Forward,FVector::UpVector);
        const float Sign=(GetUniqueID()%2)==0 ? 1.f : -1.f;
        FNavLocation Best;
        bool Found=false;
        float BestScore=-MAX_flt;
        for (const FVector Offset : {Side*Sign*330.f,Side*-Sign*330.f,Side*Sign*230.f-Forward*200.f,Side*-Sign*230.f-Forward*200.f})
        {
            FNavLocation Candidate;
            if (!Nav->ProjectPointToNavigation(GetActorLocation()+Offset,Candidate,FVector(100,100,170))) continue;
            const float Distance=FVector::Dist2D(Candidate.Location,LastKnown);
            if (Distance<320.f || Distance>SightRange) continue;
            FHitResult Cover;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(OBWatcherCover),false,this);
            Params.AddIgnoredActor(Target->CombatBody());
            const bool Sheltered=GetWorld()->LineTraceSingleByChannel(Cover,Candidate.Location+FVector(0,0,125),LastKnown+FVector(0,0,24),ECC_Visibility,Params);
            const float Score=(Sheltered ? 1000.f : 0.f)-FMath::Abs(Distance-900.f);
            if (!Found || Score>BestScore) { Best=Candidate; BestScore=Score; Found=true; }
        }
        // A cover position takes priority. Next acquisition advances from its last
        // sighted point; it never aims through the obstacle that sheltered it.
        if (Found) { NextMoveTime=0; RequestMove(Best.Location,45.f); }
    }
}

void AOBMachineEnemy::BeginAttack()
{
    if (!bVisibleTarget || !Target.IsValid() || !Director.IsValid() || !Director->IsCombatAllowed()) return;
    if (!Director->TryAcquireAttackSlot(this)) { NextAttackTime=GetWorld()->GetTimeSeconds()+.3f; Reposition(); return; }
    bHasSlot=true;
    if (AAIController* AI=Cast<AAIController>(Controller)) AI->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    LockedTarget=Target->CombatBody()->GetActorLocation();
    AttackElapsed=0;
    const bool Warden=Kind==EOBMachineKind::Warden;
    if (Warden && AttackPattern++%2==0) { PulseNumber=0; BeginPulse(); return; }
    State=EOBMachineState::Windup;
    AttackDuration=Kind==EOBMachineKind::Cutter ? .65f : Kind==EOBMachineKind::Watcher ? .8f : Warden ? 1.5f : 1.1f;
    LungeDirection=(LockedTarget-GetActorLocation()).GetSafeNormal2D();
    if (Kind==EOBMachineKind::Cutter) SetWarningVisible(true);
    Cue(TEXT("Charge"),Warden ? .8f : .5f);
}

FVector AOBMachineEnemy::GetMuzzleLocation() const
{
    return MuzzleFlash ? MuzzleFlash->GetComponentLocation() : GetActorLocation()+GetActorForwardVector()*50.f+FVector(0,0,35);
}

float AOBMachineEnemy::GetVisualAimPitch() const
{
    if (!bAlerted || IsDead()) return 0.f;
    const bool Committed=State==EOBMachineState::Windup || State==EOBMachineState::Burst;
    return FMath::Clamp(((Committed ? LockedTarget : LastKnown)-GetMuzzleLocation()).Rotation().Pitch,-60.f,60.f);
}

void AOBMachineEnemy::EmitBolt(bool bArc)
{
    if (!Director.IsValid() || !Director->IsCombatAllowed()) return;
    const FVector Source=GetMuzzleLocation();
    if (!HasLineTo(Source)) return;
    const FVector Delta=LockedTarget-Source;
    const float Speed=bArc ? 1050.f : 1550.f;
    FVector Velocity=Delta.GetSafeNormal()*Speed;
    if (bArc)
    {
        // Deterministic low arc; no homing. The warning fixes its landing position.
        const float Flight=FMath::Clamp(Delta.Size2D()/Speed,.25f,1.6f);
        Velocity=FVector(Delta.X/Flight,Delta.Y/Flight,Delta.Z/Flight+245.f*Flight);
    }
    FActorSpawnParameters Spawn;
    Spawn.Owner=this; Spawn.Instigator=this;
    Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if (AOBEnemyProjectile* Bolt=GetWorld()->SpawnActor<AOBEnemyProjectile>(Source,Velocity.Rotation(),Spawn))
    {
        Bolt->Initialize(Velocity.GetSafeNormal(),Velocity.Size(),bArc ? 18.f : 8.f,Controller);
        if (bArc) { Bolt->Movement->ProjectileGravityScale=.5f; Bolt->Movement->MaxSpeed=3000.f; Bolt->Visual->SetRelativeScale3D(FVector(.42f,.22f,.22f)); }
    }
    LastShotTime=GetWorld()->GetTimeSeconds();
    Cue(TEXT("EnemyShot"),.6f);
}

void AOBMachineEnemy::BeginPulse()
{
    State=EOBMachineState::PulseWarning;
    AttackElapsed=0;
    AttackDuration=1.2f;
    PulseCenter=Target.IsValid() ? Target->CombatBody()->GetActorLocation() : LastKnown;
    FHitResult Floor;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OBPulseFloor),false,this);
    if (Target.IsValid()) Params.AddIgnoredActor(Target->CombatBody());
    if (GetWorld()->LineTraceSingleByChannel(Floor,PulseCenter+FVector(0,0,120),PulseCenter-FVector(0,0,400),ECC_Visibility,Params)) PulseCenter=Floor.ImpactPoint+FVector(0,0,5);
    else PulseCenter.Z-=88.f;
    SetWarningVisible(true);
    Cue(TEXT("Charge"),.8f);
}

void AOBMachineEnemy::ResolvePulse()
{
    if (Target.IsValid() && !Target->bDead && FVector::Dist2D(Target->CombatBody()->GetActorLocation(),PulseCenter)<=PulseRadius
        && FMath::Abs(Target->CombatBody()->GetActorLocation().Z-PulseCenter.Z)<(Target->OccupiedMech.IsValid() ? 210.f : 150.f) && HasLineTo(Target->CombatBody()->GetActorLocation(),Target->CombatBody()))
        Target->DamagePlayer(20.f,PulseCenter);
    Cue(TEXT("Damage"),.45f);
    if (++PulseNumber<2) BeginPulse();
    else { CoreExposedRemaining=3.f; FinishAttack(3.f); }
}

void AOBMachineEnemy::TickAttack(float DeltaSeconds)
{
    AttackElapsed+=DeltaSeconds;
    if (State==EOBMachineState::PulseWarning)
    { UpdateWarning(); if (AttackElapsed>=AttackDuration) ResolvePulse(); return; }
    if (State==EOBMachineState::Windup)
    {
        if (Kind==EOBMachineKind::Cutter) UpdateWarning();
        if (AttackElapsed<AttackDuration) return;
        AttackElapsed=0;
        if (Kind==EOBMachineKind::Cutter)
        {
            State=EOBMachineState::Lunge; bLungeHit=false;
            GetCharacterMovement()->MaxWalkSpeed=1050.f;
            GetCharacterMovement()->Velocity=LungeDirection*950.f;
            GetCharacterMovement()->bUseRVOAvoidance=false;
            SetWarningVisible(false);
        }
        else if (Kind==EOBMachineKind::Watcher)
        { State=EOBMachineState::Burst; BurstRemaining=3; BurstCountdown=0; }
        else { EmitBolt(true); CoreExposedRemaining=Kind==EOBMachineKind::Warden ? 3.f : 2.f; FinishAttack(CoreExposedRemaining); }
    }
    if (State==EOBMachineState::Lunge)
    {
        AddMovementInput(LungeDirection,1.f,true);
        if (!bLungeHit && Target.IsValid() && FVector::Dist2D(GetActorLocation(),Target->CombatBody()->GetActorLocation())<115.f && HasLineTo(Target->CombatBody()->GetActorLocation(),Target->CombatBody()))
        { Target->DamagePlayer(12.f,GetActorLocation()); bLungeHit=true; }
        if (AttackElapsed>=.42f) FinishAttack(.9f);
    }
    else if (State==EOBMachineState::Burst)
    {
        BurstCountdown-=DeltaSeconds;
        if (BurstCountdown<=0.f && BurstRemaining>0) { EmitBolt(false); --BurstRemaining; BurstCountdown=.16f; }
        if (!BurstRemaining) FinishAttack(.5f);
    }
}

void AOBMachineEnemy::FinishAttack(float Recovery)
{
    ReleaseSlot();
    SetWarningVisible(false);
    GetCharacterMovement()->MaxWalkSpeed=Kind==EOBMachineKind::Cutter ? 430.f : Kind==EOBMachineKind::Watcher ? 260.f : Kind==EOBMachineKind::Warden ? 190.f : 170.f;
    GetCharacterMovement()->bUseRVOAvoidance=true;
    GetCharacterMovement()->StopMovementImmediately();
    State=EOBMachineState::Recovery;
    RecoveryRemaining=Recovery;
    NextAttackTime=GetWorld()->GetTimeSeconds()+FMath::Max(Recovery,Kind==EOBMachineKind::Watcher ? 1.8f : 1.3f);
    BurstRemaining=0;
}

void AOBMachineEnemy::ReleaseSlot()
{
    if (bHasSlot && Director.IsValid()) Director->ReleaseAttackSlot(this);
    bHasSlot=false;
}

void AOBMachineEnemy::CancelAttack()
{
    ReleaseSlot();
    SetWarningVisible(false);
    BurstRemaining=0;
    if (AAIController* AI=Cast<AAIController>(Controller)) AI->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->bUseRVOAvoidance=true;
    GetCharacterMovement()->MaxWalkSpeed=Kind==EOBMachineKind::Cutter ? 430.f : Kind==EOBMachineKind::Watcher ? 260.f : Kind==EOBMachineKind::Warden ? 190.f : 170.f;
    if (!IsDead() && bActive) State=EOBMachineState::Idle;
}

void AOBMachineEnemy::SetWarningVisible(bool bVisible)
{
    for (int32 I=0;I<WarningSegments.Num();++I) WarningSegments[I]->SetVisibility(bVisible && (Kind!=EOBMachineKind::Cutter || I<8));
    if (bVisible) UpdateWarning();
}

void AOBMachineEnemy::UpdateWarning()
{
    if (Kind==EOBMachineKind::Cutter)
    {
        for (int32 I=0;I<8;++I)
        {
            UStaticMeshComponent* Segment=WarningSegments[I];
            Segment->SetWorldLocation(GetActorLocation()-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-4.f)+LungeDirection*(65.f+I*48.f));
            Segment->SetWorldRotation(LungeDirection.Rotation());
            Segment->SetWorldScale3D(FVector(.35f,.7f,.025f));
        }
    }
    else
    {
        for (int32 I=0;I<WarningSegments.Num();++I)
        {
            const float Angle=2.f*PI*I/WarningSegments.Num();
            UStaticMeshComponent* Segment=WarningSegments[I];
            Segment->SetWorldLocation(PulseCenter+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*PulseRadius);
            Segment->SetWorldRotation(FRotator(0,FMath::RadiansToDegrees(Angle)+90.f,0));
            Segment->SetWorldScale3D(FVector(.64f,.1f,.035f));
        }
    }
    if (WarningMID) WarningMID->SetScalarParameterValue(TEXT("EmissiveStrength"),2.f+2.f*FMath::Clamp(AttackElapsed/AttackDuration,0.f,1.f));
}

float AOBMachineEnemy::GetDamageMultiplier(const FVector& Source) const
{
    if ((Kind!=EOBMachineKind::Bulwark && Kind!=EOBMachineKind::Warden) || IsCoreExposed()) return 1.f;
    return FVector::DotProduct((Source-GetActorLocation()).GetSafeNormal2D(),GetActorForwardVector())>.55f ? .2f : 1.f;
}

float AOBMachineEnemy::TakeDamage(float Amount,const FDamageEvent& Event,AController* Instigator,AActor* Causer)
{
    if (!bActive || IsDead() || Amount<=0.f || (Director.IsValid() && !Director->IsCombatAllowed())) return 0.f;
    FVector Source=Causer ? Causer->GetActorLocation() : GetActorLocation()-GetActorForwardVector();
    if (Event.IsOfType(FPointDamageEvent::ClassID)) Source=GetActorLocation()-static_cast<const FPointDamageEvent&>(Event).ShotDirection*100.f;
    const float Damage=FMath::Min(Health,Amount*GetDamageMultiplier(Source));
    Health-=Damage;
    HitFlash=.12f;
    Remember(Causer ? Causer->GetActorLocation() : Source,true);
    Cue(TEXT("EnemyDamage"),.4f);
    if (Health<=0.f) Die(true);
    return Damage;
}

void AOBMachineEnemy::Die(bool bNotify)
{
    if (IsDead()) return;
    CancelAttack();
    Health=0;
    InitialInvestigationUntil=-100.f;
    bActive=false;
    State=EOBMachineState::Dead;
    SetActorEnableCollision(false);
    GetCharacterMovement()->DisableMovement();
    DeathElapsed=0;
    if (bNotify)
    {
        Cue(TEXT("EnemyDeath"),.7f);
        if (!bReported && Director.IsValid()) { bReported=true; Director->ResolveEnemy(EncounterId,StableId); }
    }
    // Keep the hidden actor for stable encounter references and checkpoint resets.
}

void AOBMachineEnemy::UpdateVisuals(float DeltaSeconds)
{
    Clock+=DeltaSeconds;
    HitFlash=FMath::Max(0.f,HitFlash-DeltaSeconds);
    const bool Charging=State==EOBMachineState::Windup || State==EOBMachineState::PulseWarning;
    MuzzleFlash->SetVisibility(GetWorld() && GetWorld()->GetTimeSeconds()-LastShotTime<.065f);
    if (AccentMID) AccentMID->SetScalarParameterValue(TEXT("EmissiveStrength"),3.f);
    if (CoreMID) CoreMID->SetScalarParameterValue(TEXT("EmissiveStrength"),IsCoreExposed() ? 2.5f : Charging ? 1.4f : HitFlash>0 ? 1.8f : .35f);
    if (Kind==EOBMachineKind::Bulwark || Kind==EOBMachineKind::Warden)
    {
        // Lower the physical shield during the existing vulnerability window.
        const FQuat Open=FQuat(FVector::RightVector,FMath::DegreesToRadians(IsCoreExposed() ? 68.f : 0.f));
        const FQuat Wanted=ShieldGripTransform.GetRotation()*Open;
        ShieldMount->SetRelativeRotation(FMath::QInterpTo(ShieldMount->GetRelativeRotation().Quaternion(),Wanted,DeltaSeconds,7.f));
    }
}

void AOBMachineEnemy::RecoverNavigation(float DeltaSeconds)
{
    RecoveryCooldown=FMath::Max(0.f,RecoveryCooldown-DeltaSeconds);
    const bool WantsMove=State==EOBMachineState::Approach || State==EOBMachineState::Search;
    if (WantsMove && FVector::Dist2D(LastProgressLocation,GetActorLocation())<15.f) StuckElapsed+=DeltaSeconds;
    else { StuckElapsed=0; LastProgressLocation=GetActorLocation(); }
    const bool Fell=GetActorLocation().Z<InitialLocation.Z-600.f;
    if ((!Fell && StuckElapsed<8.f) || RecoveryCooldown>0) return;
    RecoveryCooldown=4.f;
    if (auto* Nav=UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        FNavLocation Safe;
        // Repath first; teleport only out of the player's view and never close to them.
        if (!Fell && StuckElapsed<16.f)
        {
            if (Nav->GetRandomReachablePointInRadius(GetActorLocation(),300.f,Safe)) { NextMoveTime=0; RequestMove(Safe.Location,40.f); }
            return;
        }
        if (Target.IsValid() && FVector::Dist2D(Target->CombatBody()->GetActorLocation(),InitialLocation)<900.f) return;
        if (Target.IsValid())
        {
            FHitResult Hit;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(OBMachineRecovery),false,this);
            Params.AddIgnoredActor(Target->CombatBody());
            if (!GetWorld()->LineTraceSingleByChannel(Hit,Target->CombatBody()->GetActorLocation()+FVector(0,0,60),InitialLocation,ECC_Visibility,Params)) return;
        }
        if (Nav->ProjectPointToNavigation(InitialLocation,Safe,FVector(150,150,250)))
        {
            SetActorLocation(Safe.Location+FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f),false,nullptr,ETeleportType::TeleportPhysics);
            GetCharacterMovement()->StopMovementImmediately();
            GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            StuckElapsed=0; LastProgressLocation=GetActorLocation(); NextMoveTime=0;
            Cue(TEXT("Door"),.75f);
            UE_LOG(LogTemp,Warning,TEXT("Dead Signal repathed blocked machine %s at its entry"),*StableId.ToString());
        }
    }
}

void AOBMachineEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (IsDead())
    {
        DeathElapsed+=DeltaSeconds;
        if (!IsHidden())
        {
            MuzzleFlash->SetVisibility(false);
            if (CoreMID) CoreMID->SetScalarParameterValue(TEXT("EmissiveStrength"),FMath::Max(0.f,.5f-DeathElapsed));
            if (DeathElapsed>=1.25f) { SetActorHiddenInGame(true); GetMesh()->SetComponentTickEnabled(false); SetActorTickEnabled(false); }
        }
        return;
    }
    if (!bActive) return;
    if (!Director.IsValid()) Director=AOBMissionDirector::Get(this);
    if (!Director.IsValid() || !Director->IsCombatAllowed()) { CancelAttack(); return; }
    CoreExposedRemaining=FMath::Max(0.f,CoreExposedRemaining-DeltaSeconds);
    UpdateVisuals(DeltaSeconds);
    PerceptionElapsed+=DeltaSeconds;
    if (PerceptionElapsed>=.16f) { PerceptionElapsed=0; UpdatePerception(); }
    if (State==EOBMachineState::Recovery)
    {
        RecoveryRemaining-=DeltaSeconds;
        if (RecoveryRemaining<=0) { State=EOBMachineState::Idle; Reposition(); }
        return;
    }
    if (State==EOBMachineState::Windup || State==EOBMachineState::Lunge || State==EOBMachineState::Burst || State==EOBMachineState::PulseWarning)
    {
        if (State!=EOBMachineState::Lunge)
        {
            const FRotator Desired(0,(LockedTarget-GetActorLocation()).Rotation().Yaw,0);
            SetActorRotation(FMath::RInterpTo(GetActorRotation(),Desired,DeltaSeconds,7.f));
        }
        TickAttack(DeltaSeconds);
    }
    else if (bAlerted)
    {
        const FRotator Desired(0,(LastKnown-GetActorLocation()).Rotation().Yaw,0);
        SetActorRotation(FMath::RInterpTo(GetActorRotation(),Desired,DeltaSeconds,4.f));
    }
    RecoverNavigation(DeltaSeconds);
}

FString AOBMachineEnemy::GetAttackCue() const
{
    if (IsCoreExposed()) return TEXT("CORE EXPOSED  /  ATTACK NOW");
    if (State==EOBMachineState::PulseWarning) return TEXT("FLOOR PULSE  /  LEAVE THE MARKED CIRCLE");
    if (State==EOBMachineState::Windup)
        return Kind==EOBMachineKind::Cutter ? TEXT("CUTTER LUNGE  /  SIDESTEP") : Kind==EOBMachineKind::Watcher ? TEXT("WATCHER BURST  /  TAKE COVER") : TEXT("CHARGED SHOT  /  KEEP MOVING");
    return Kind==EOBMachineKind::Warden ? TEXT("RELAY WARDEN  /  FLANK THE SHIELD") : Kind==EOBMachineKind::Bulwark ? TEXT("BULWARK  /  FLANK THE SHIELD") : Kind==EOBMachineKind::Watcher ? TEXT("WATCHER") : TEXT("CUTTER");
}

void AOBMachineEnemy::Cue(FName Name,float Volume)
{
    if (Director.IsValid()) Director->PlaySound(Name,GetActorLocation(),Volume);
}

void AOBMachineEnemy::EndPlay(const EEndPlayReason::Type Reason)
{
    CancelAttack();
    Super::EndPlay(Reason);
}
