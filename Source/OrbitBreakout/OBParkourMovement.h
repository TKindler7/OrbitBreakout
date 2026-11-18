#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "OBParkourMovement.generated.h"

UENUM(BlueprintType)
enum class EOBTraversal : uint8 { Free, Slide, Mantle, WallRun };

/** The character capsule remains the authority for every traversal sweep. */
UCLASS()
class ORBITBREAKOUT_API UOBParkourMovement : public UCharacterMovementComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,Category="Tuning") TSoftObjectPtr<class UOBParkourTuning> Tuning;
    UOBParkourMovement();
    virtual void BeginPlay() override;
    float WallSpeed = 620.f;
    UPROPERTY(VisibleAnywhere, Category="Parkour") float CoyoteSeconds = .10f;
    UPROPERTY(VisibleAnywhere, Category="Parkour") float BufferSeconds = .13f;
    UPROPERTY(VisibleAnywhere, Category="Parkour") float SlideSeconds = .75f;
    UPROPERTY(VisibleAnywhere, Category="Parkour") float WallSeconds = .95f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parkour") EOBTraversal Traversal = EOBTraversal::Free;
    FVector WallNormal = FVector::ZeroVector;
    void PressJump();
    void ReleaseJump();
    void StartSlide();
    void EndSlide();
    void ResetTraversal();
    bool IsBusy() const { return Traversal != EOBTraversal::Free; }
    virtual float GetMaxSpeed() const override;
    virtual void OnMovementModeChanged(EMovementMode Previous, uint8 PreviousCustom) override;
    virtual void UpdateCharacterStateBeforeMovement(float Dt) override;
    virtual void PhysCustom(float Dt, int32 Iterations) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FOBPilotUpgradeTest;
#endif
    double BufferUntil = -1., CoyoteUntil = -1.;
    float Remaining = 0.f;
    bool bCanCutJump = false;
    bool bJumpHeld = false, bUsedAirJump = true, bUsedWall = false;
    FVector MantleTop = FVector::ZeroVector, MantleLift = FVector::ZeroVector, TravelDirection = FVector::ZeroVector;
    TWeakObjectPtr<AActor> Surface;
    bool bLifting = false;
    bool FindWall(FHitResult& Hit) const;
    bool TryMantle();
    bool CanTraverse() const;
    void StartCustom(EOBTraversal Mode);
    void Detach();
};
