#pragma once

#include "Engine/DataAsset.h"
#include "OBPilotTuning.generated.h"

UCLASS(BlueprintType)
class ORBITBREAKOUT_API UOBParkourTuning : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,Category="Jump",meta=(ClampMin="0",ClampMax=".25")) float CoyoteSeconds = .1f;
    UPROPERTY(EditAnywhere,Category="Jump",meta=(ClampMin="0",ClampMax=".25")) float BufferSeconds = .13f;
    UPROPERTY(EditAnywhere,Category="Slide",meta=(ClampMin=".1",ClampMax="2")) float SlideSeconds = .75f;
    UPROPERTY(EditAnywhere,Category="Wall",meta=(ClampMin=".1",ClampMax="2")) float WallSeconds = .95f;
    UPROPERTY(EditAnywhere,Category="Wall",meta=(ClampMin="300",ClampMax="850")) float WallSpeed = 620.f;
};

UCLASS(BlueprintType)
class ORBITBREAKOUT_API UOBWeaponSightProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,Category="Sight") FVector Offset = FVector(41,0,-19.3);
    UPROPERTY(EditAnywhere,Category="Sight",meta=(ClampMin="50",ClampMax="85")) float FieldOfView = 68.f;
};

UCLASS(BlueprintType)
class ORBITBREAKOUT_API UOBMechTuning : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,Category="Chassis",meta=(ClampMin="1")) float Armor = 280.f;
    UPROPERTY(EditAnywhere,Category="Chassis",meta=(ClampMin="100",ClampMax="600")) float WalkSpeed = 355.f;
    UPROPERTY(EditAnywhere,Category="Pulse",meta=(ClampMin="1")) float Damage = 68.f;
    UPROPERTY(EditAnywhere,Category="Pulse",meta=(ClampMin="1",ClampMax="100")) float HeatPerShot = 32.f;
};
