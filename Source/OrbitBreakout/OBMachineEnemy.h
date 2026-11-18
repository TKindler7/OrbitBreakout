#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "OBMachineEnemy.generated.h"

class AOBMissionDirector;
class AOBPlayerCharacter;
class UStaticMeshComponent;
class USceneComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USkeletalMesh;
class UStaticMesh;
class UAnimSequence;

UENUM(BlueprintType)
enum class EOBMachineKind : uint8 { Cutter, Watcher, Bulwark, Warden };

UENUM(BlueprintType)
enum class EOBMachineState : uint8 { Dormant, Idle, Search, Approach, Windup, Lunge, Burst, PulseWarning, Recovery, Dead };

/** Encounter-owned synthetic troopers. All attacks are visible, committed and interruptible by cover. */
UCLASS()
class ORBITBREAKOUT_API AOBMachineEnemy : public ACharacter
{
    GENERATED_BODY()
public:
    AOBMachineEnemy();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual float TakeDamage(float Amount, const FDamageEvent& Event, AController* Instigator, AActor* Causer) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dead Signal") EOBMachineKind Kind = EOBMachineKind::Cutter;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dead Signal") FName StableId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dead Signal") int32 EncounterId = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dead Signal", meta=(ClampMin="100")) float SightRange = 1900.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dead Signal", meta=(ClampMin="1")) float AwarenessMemorySeconds = 8.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dead Signal") bool bActive = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dead Signal") bool bAlerted = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dead Signal") float Health = 70.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dead Signal") float MaxHealth = 70.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dead Signal") EOBMachineState State = EOBMachineState::Dormant;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dead Signal|Animation") float LastShotTime = -100.f;
    UPROPERTY() TObjectPtr<UAnimSequence> LungeAnimation;

    UFUNCTION(BlueprintCallable) void Activate();
    UFUNCTION(BlueprintCallable) void SetResolved();
    UFUNCTION(BlueprintCallable) void ResetToSpawn();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Dead Signal|Presentation") void RefreshPresentation() { ConfigureVisuals(); }
    UFUNCTION(BlueprintPure) bool IsDead() const { return State == EOBMachineState::Dead; }
    UFUNCTION(BlueprintPure) float GetHealthFraction() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }
    UFUNCTION(BlueprintPure) FString GetAttackCue() const;
    UFUNCTION(BlueprintPure) bool IsCoreExposed() const { return CoreExposedRemaining > 0.f; }
    UFUNCTION(BlueprintPure) FVector GetMuzzleLocation() const;
    float GetVisualAimPitch() const;
    UAnimSequence* GetVisualActionAnimation() const { return State == EOBMachineState::Lunge ? LungeAnimation.Get() : nullptr; }
    float GetVisualActionProgress() const { return FMath::Clamp(AttackElapsed / .42f, 0.f, 1.f); }
    /** Source position is used so frontal shielding also works for non-point damage. */
    float GetDamageMultiplier(const FVector& Source) const;

private:
    // Combat distance is checked separately; navigation must clear path corners.
    static constexpr float ApproachAcceptanceRadius = 45.f;
    UPROPERTY() TObjectPtr<USceneComponent> Body;
    UPROPERTY() TObjectPtr<USkeletalMesh> TrooperMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> RifleMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> ShieldMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> ReinforcedArmorMesh;
    UPROPERTY() TObjectPtr<UMaterialInterface> TrooperTopMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> TrooperBottomMaterial;
    UPROPERTY() TObjectPtr<UAnimSequence> EquipmentReferencePose;
    UPROPERTY() TObjectPtr<USceneComponent> RifleGrip;
    UPROPERTY() TObjectPtr<USceneComponent> ShieldMount;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Rifle;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Shield;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ReinforcedArmor;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> MuzzleFlash;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> StatusLight;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> UniformMIDs;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> WarningSegments;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ArmorMID;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DarkMID;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> AccentMID;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CoreMID;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WarningMID;
    UPROPERTY() TWeakObjectPtr<AOBMissionDirector> Director;
    UPROPERTY() TWeakObjectPtr<AOBPlayerCharacter> Target;

    FVector InitialLocation = FVector::ZeroVector, LastKnown = FVector::ZeroVector, LockedTarget = FVector::ZeroVector;
    FVector LungeDirection = FVector::ForwardVector, PulseCenter = FVector::ZeroVector, LastProgressLocation = FVector::ZeroVector;
    FRotator InitialFacing = FRotator::ZeroRotator;
    bool bSpawnRecorded = false, bVisibleTarget = false, bHasSlot = false, bReported = false, bLungeHit = false;
    float Clock = 0.f, PerceptionElapsed = 0.f, LastKnownTime = -100.f, LastHeardShot = -100.f, LastAlertTime = -100.f;
    float NextAttackTime = 0.f, NextMoveTime = 0.f, AttackElapsed = 0.f, AttackDuration = .65f;
    float InitialInvestigationUntil = -100.f;
    FTransform ShieldGripTransform = FTransform::Identity;
    float RecoveryRemaining = 0.f, CoreExposedRemaining = 0.f, HitFlash = 0.f, DeathElapsed = 0.f;
    float BurstCountdown = 0.f, StuckElapsed = 0.f, RecoveryCooldown = 0.f;
    int32 BurstRemaining = 0, AttackPattern = 0, PulseNumber = 0;

    void ConfigureVisuals();
    void UpdateVisuals(float DeltaSeconds);
    void UpdatePerception();
    void Remember(const FVector& Position, bool bShare);
    void BeginAnnouncedInvestigation(const FVector& LastReportedPosition);
    bool HasSight(const AOBPlayerCharacter* Player) const;
    bool HasLineTo(const FVector& Point, const AActor* AllowedHit = nullptr) const;
    void RequestMove(const FVector& Destination, float Acceptance);
    void Reposition();
    void BeginAttack();
    void TickAttack(float DeltaSeconds);
    void EmitBolt(bool bArc);
    void BeginPulse();
    void ResolvePulse();
    void FinishAttack(float Recovery);
    void CancelAttack();
    void ReleaseSlot();
    void Die(bool bNotify);
    void SetWarningVisible(bool bVisible);
    void UpdateWarning();
    void RecoverNavigation(float DeltaSeconds);
    void SnapToFloor();
    void Cue(FName Name, float Volume = .6f);

#if WITH_DEV_AUTOMATION_TESTS
    friend class FOBMachineRulesTest;
#endif
};
