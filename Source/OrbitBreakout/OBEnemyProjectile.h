#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OBEnemyProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/** A visible non-homing bolt. Only its swept collision can deliver damage. */
UCLASS()
class ORBITBREAKOUT_API AOBEnemyProjectile : public AActor
{
    GENERATED_BODY()

public:
    AOBEnemyProjectile();
    virtual void BeginPlay() override;
    void Initialize(const FVector& Direction, float Speed, float Damage, AController* InstigatorController);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit") TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit") TObjectPtr<UProjectileMovementComponent> Movement;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit") TObjectPtr<UStaticMeshComponent> Visual;

private:
    UPROPERTY() TWeakObjectPtr<AController> ShotController;
    float DamageAmount = 10.f;
    bool bSpent = false;

    UFUNCTION() void OnBoltHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
};
