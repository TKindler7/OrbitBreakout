#include "OBEnemyProjectile.h"
#include "OBMech.h"
#include "OBPlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AOBEnemyProjectile::AOBEnemyProjectile()
{
    PrimaryActorTick.bCanEverTick = false;
    Tags.Add(TEXT("OB_PROJECTILE"));
    InitialLifeSpan = 4.f;
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("BoltCollision"));
    SetRootComponent(Collision);
    Collision->InitSphereRadius(7.f);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Collision->SetCollisionObjectType(ECC_GameTraceChannel1);
    Collision->SetCollisionResponseToAllChannels(ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
    Collision->SetGenerateOverlapEvents(false);
    Collision->SetNotifyRigidBodyCollision(true);
    Collision->OnComponentHit.AddDynamic(this, &AOBEnemyProjectile::OnBoltHit);
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrangeBolt"));
    Visual->SetupAttachment(Collision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Sphere.Succeeded()) Visual->SetStaticMesh(Sphere.Object);
    Visual->SetRelativeScale3D(FVector(.34f,.11f,.11f));
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetCastShadow(false);
    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("SweptMovement"));
    Movement->SetUpdatedComponent(Collision);
    Movement->InitialSpeed = 1800.f;
    Movement->MaxSpeed = 1800.f;
    Movement->ProjectileGravityScale = 0.f;
    Movement->bRotationFollowsVelocity = true;
    Movement->bShouldBounce = false;
    Movement->bSweepCollision = true;
    Movement->bForceSubStepping = true;
    Movement->MaxSimulationTimeStep = 1.f / 90.f;
}

void AOBEnemyProjectile::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner()) Collision->IgnoreActorWhenMoving(GetOwner(), true);
    if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/OrbitBreakout/Materials/M_Emissive.M_Emissive")))
    {
        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, this);
        MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f,.19f,.035f));
        MID->SetScalarParameterValue(TEXT("EmissiveStrength"), 5.f);
        Visual->SetMaterial(0, MID);
    }
    else if (UMaterialInstanceDynamic* MID = Visual->CreateDynamicMaterialInstance(0))
    {
        MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f,.22f,.03f));
    }
}

void AOBEnemyProjectile::Initialize(const FVector& Direction, float Speed, float Damage, AController* InstigatorController)
{
    DamageAmount = FMath::Max(0.f, Damage);
    ShotController = InstigatorController;
    const float SafeSpeed = FMath::Max(1.f, Speed);
    const FVector Velocity = Direction.GetSafeNormal() * SafeSpeed;
    SetActorRotation(Velocity.Rotation());
    Movement->InitialSpeed = SafeSpeed;
    Movement->MaxSpeed = SafeSpeed;
    Movement->Velocity = Velocity;
}

void AOBEnemyProjectile::OnBoltHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (bSpent || OtherActor == GetOwner()) return;
    bSpent = true;
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Movement->StopMovementImmediately();
    if (Cast<AOBPlayerCharacter>(OtherActor) || Cast<AOBMech>(OtherActor))
    {
        UGameplayStatics::ApplyPointDamage(OtherActor, DamageAmount, GetActorForwardVector(), Hit,
            ShotController.Get(), this, nullptr);
    }
    // World geometry and other blocking objects absorb the bolt without a damage event.
    Destroy();
}
