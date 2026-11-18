#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "OBRifleAnimInstance.generated.h"

class UAnimSequence;

/** Native mannequin locomotion, with the authored two-hand rifle grip kept intact. */
UCLASS(Transient, Blueprintable)
class ORBITBREAKOUT_API UOBRifleAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    UOBRifleAnimInstance();

    // SKM_Manny_Simple omits weapon_r. Preserve its authored rifle grip when
    // attaching to hand_r by composing WeaponGrip * GetWeaponBoneInRightHand().
    static FTransform GetWeaponBoneInRightHand();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Animation") float GroundSpeed = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Animation") float AimPitch = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Animation") bool bArmed = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Animation") float CrashPresentationWeight = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Animation") float CrashRecoveryWeight = 0.f;

    // Hard references make these installed template animations available to the cooker.
    UPROPERTY() TObjectPtr<UAnimSequence> UnarmedIdle;
    UPROPERTY() TArray<TObjectPtr<UAnimSequence>> UnarmedWalk;
    UPROPERTY() TArray<TObjectPtr<UAnimSequence>> UnarmedJog;
    UPROPERTY() TObjectPtr<UAnimSequence> RifleIdle;
    UPROPERTY() TArray<TObjectPtr<UAnimSequence>> RifleWalk;
    UPROPERTY() TArray<TObjectPtr<UAnimSequence>> RifleJog;
    UPROPERTY() TObjectPtr<UAnimSequence> RifleReload;
    UPROPERTY() TObjectPtr<UAnimSequence> RifleFire;
    UPROPERTY() TObjectPtr<UAnimSequence> RifleAimUp;
    UPROPERTY() TObjectPtr<UAnimSequence> RifleAimDown;
    UPROPERTY() TObjectPtr<UAnimSequence> UnarmedFall;
    UPROPERTY() TObjectPtr<UAnimSequence> RifleFall;
    UPROPERTY() TObjectPtr<UAnimSequence> Death;
    UPROPERTY() TObjectPtr<UAnimSequence> CrashRecovery;
    UPROPERTY() TObjectPtr<UAnimSequence> TraversalLand;

protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};

/** Wraith uses his own skeleton and authored combat locomotion, never mannequin poses. */
UCLASS(Transient)
class ORBITBREAKOUT_API UOBWraithAnimInstance : public UOBRifleAnimInstance
{
    GENERATED_BODY()
public:
    UOBWraithAnimInstance();
};
