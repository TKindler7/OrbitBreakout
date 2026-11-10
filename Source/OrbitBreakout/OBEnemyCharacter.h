#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "OBEnemyCharacter.generated.h"

class AOBMissionDirector;
class AOBPlayerCharacter;
class UStaticMeshComponent;
class USceneComponent;
class UMaterialInstanceDynamic;
class USoundBase;
class USkeletalMesh;
class UStaticMesh;
class UAnimationAsset;

UENUM(BlueprintType)
enum class EOBEnemyState : uint8
{
    Dormant, Idle, Acquire, Approach, Attack, Reposition, Dead
};

UENUM(BlueprintType)
enum class EOBBossAttack : uint8
{
    None, RushWarning, Rush, WaveWarning, Shockwave, Recovery
};

/** One encounter-owned synthetic security unit; a Heavy uses the same fair attack rules. */
UCLASS()
class ORBITBREAKOUT_API AOBEnemyCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AOBEnemyCharacter();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Encounter") FName StableId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Encounter", meta=(ClampMin="1",ClampMax="6")) int32 EncounterId = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Enemy") bool bHeavy = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Tuning", meta=(ClampMin="1")) float TrooperHealth = 60.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Tuning", meta=(ClampMin="1")) float SentinelHealth = 2400.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Boss", meta=(ClampMin="0.8")) float RushWarningSeconds = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Boss", meta=(ClampMin="100")) float RushSpeed = 950.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Boss", meta=(ClampMin="100")) float RushMaxDistance = 1050.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Boss", meta=(ClampMin="1")) float RushDamage = 28.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Boss", meta=(ClampMin="1")) float WaveWarningSeconds = 1.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Boss", meta=(ClampMin="100")) float ShockwaveRadius = 720.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Boss", meta=(ClampMin="100")) float ShockwaveSpeed = 600.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Boss", meta=(ClampMin="1")) float ShockwaveDamage = 24.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Tuning", meta=(ClampMin="1")) float TrooperDamage = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Tuning", meta=(ClampMin="1")) float SentinelDamage = 18.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Tuning", meta=(ClampMin="100")) float AcquisitionRange = 1800.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Awareness", meta=(ClampMin="100")) float NearbyAwarenessRange = 900.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Awareness", meta=(ClampMin="100")) float GunshotHearingRange = 1800.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Awareness", meta=(ClampMin="100")) float SquadAlertRange = 1800.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Awareness", meta=(ClampMin="1")) float AlertMemorySeconds = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Visual", meta=(ClampMin="0.1",ClampMax="5")) float CorpseVisibleSeconds = 1.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Tuning", meta=(ClampMin="0.2")) float TrooperChargeSeconds = .45f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Tuning", meta=(ClampMin="0.2")) float SentinelChargeSeconds = .8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Tuning", meta=(ClampMin="100")) float ProjectileSpeed = 1800.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Tuning", meta=(ClampMin="0")) float InitialAttackDelay = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Audio") TObjectPtr<USoundBase> ChargeSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Audio") TObjectPtr<USoundBase> ShotSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Audio") TObjectPtr<USoundBase> HitSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Audio") TObjectPtr<USoundBase> DefeatSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Boss Intro") TObjectPtr<USoundBase> HeavyIntroSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Boss Intro") TObjectPtr<UAnimationAsset> HeavyIntroAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Boss Intro", meta=(ClampMin="1",ClampMax="6")) float BossIntroSeconds = 2.4f;
    /** Authored Fab bull assets. Their materials and skeleton are preserved. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Heavy Visual") TObjectPtr<USkeletalMesh> HeavySkeletalMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Heavy Visual") TObjectPtr<UAnimationAsset> HeavyIdleAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Heavy Visual") TObjectPtr<UAnimationAsset> HeavyMoveAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Heavy Visual") TObjectPtr<UAnimationAsset> HeavyAttackAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Heavy Visual") TObjectPtr<UAnimationAsset> HeavyDeathAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Heavy Visual") FVector HeavyMeshOffset = FVector(0,0,-112.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Heavy Visual") FRotator HeavyMeshRotation = FRotator(0,-90,0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Heavy Visual") FVector HeavyMeshScale = FVector(1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Heavy Visual") FName HeavyWeaponSocket = TEXT("lowerarm_r");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Heavy Visual") FVector HeavyEmitterOffset = FVector(0,0,-16.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Heavy Visual") FRotator HeavyEmitterRotation = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Heavy Visual") FVector HeavyEmitterScale = FVector(1.5f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Visual") FVector WeaponGripOffset = FVector(0,0,0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Visual") FRotator WeaponGripRotation = FRotator(0,-90,0);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") float Health = 60.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") EOBEnemyState State = EOBEnemyState::Dormant;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") bool bActive = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") bool bAlerted = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") float LastShotTime = -100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") EOBBossAttack BossAttack = EOBBossAttack::None;

    UFUNCTION(BlueprintCallable, Category="Orbit") void Activate();
    UFUNCTION(BlueprintCallable, Category="Orbit") void SetResolved();
    UFUNCTION(BlueprintPure, Category="Orbit") bool IsDead() const { return State == EOBEnemyState::Dead; }
    UFUNCTION(BlueprintPure, Category="Orbit") FVector GetMuzzleLocation() const;
    UFUNCTION(BlueprintPure, Category="Orbit|Visual") float GetVisualAimPitch() const;
    UFUNCTION(BlueprintPure, Category="Orbit|Boss") float GetBossHealthFraction() const;
    UFUNCTION(BlueprintPure, Category="Orbit|Boss") bool IsBossEnraged() const;
    UFUNCTION(BlueprintPure, Category="Orbit|Boss") FString GetBossAttackCue() const;
    UFUNCTION(BlueprintPure, Category="Orbit|Boss Intro") bool IsBossIntroPlaying() const { return bBossIntroPlaying; }
    UFUNCTION(BlueprintPure, Category="Orbit|Boss Intro") bool HasPlayedBossIntro() const { return bBossIntroPlayed; }

private:
    UPROPERTY() TObjectPtr<USceneComponent> BodyPivot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Torso;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Head;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Visor;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Core;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftArm;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RightArm;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftLeg;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RightLeg;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Weapon;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Muzzle;
    UPROPERTY() TObjectPtr<USceneComponent> WeaponRoot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> WeaponBarrel;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> WeaponStock;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> WeaponGrip;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> WeaponMagazine;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> WeaponRail;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> WeaponCell;
    UPROPERTY() TObjectPtr<USkeletalMesh> TrooperSkeletalMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> TrooperRifleMesh;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> UniformMIDs;
    UPROPERTY(Transient) TObjectPtr<UAnimationAsset> CurrentHeavyAnimation;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ArmorMID;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DarkMID;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> AccentMID;
    UPROPERTY() TWeakObjectPtr<AOBMissionDirector> Director;
    UPROPERTY() TWeakObjectPtr<AOBPlayerCharacter> Target;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> BossRingSegments;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> BossLaneSegments;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BossTelegraphMID;

    FVector LastSeenLocation = FVector::ZeroVector;
    FVector LockedTarget = FVector::ZeroVector;
    FVector InitialLocation = FVector::ZeroVector;
    FRotator InitialFacing = FRotator::ZeroRotator;
    float LastSeenTime = -100.f;
    float LastHeardShotTime = -100.f;
    float LastSquadAlertTime = -100.f;
    float NextAttackTime = 0.f;
    float NextMoveTime = 0.f;
    float ChargeElapsed = 0.f;
    float BoltCountdown = 0.f;
    float SlotReleaseCountdown = 0.f;
    float VisualClock = 0.f;
    float FlashRemaining = 0.f;
    float ChargeDuration = .45f;
    int32 BoltsRemaining = 0;
    bool bHasAttackSlot = false;
    bool bCharging = false;
    bool bCanSeeTarget = false;
    bool bDeathReported = false;
    FTimerHandle PerceptionTimer;
    FTimerHandle CorpseHideTimer;
    FVector BossAttackOrigin = FVector::ZeroVector;
    FVector RushDirection = FVector::ForwardVector;
    float BossAttackElapsed = 0.f;
    float RushRemainingDistance = 0.f;
    float WaveRadius = 0.f;
    float RecoveryDuration = 0.f;
    int32 BossAttackSequence = 0;
    bool bBossAttackHitPlayer = false;
    bool bBossIntroPlaying = false;
    bool bBossIntroPlayed = false;

    void Perceive();
    bool HasRecentAwareness() const;
    bool CanVisuallyDetect(const AOBPlayerCharacter* Player) const;
    void RememberTarget(const FVector& Location);
    void AlertNearbySquad();
    void ReceiveSquadAlert(const AOBEnemyCharacter* Source, const FVector& Location);
    bool HasLineOfSight(const AOBPlayerCharacter* Player) const;
    void RequestMove(const FVector& Destination, float AcceptanceRadius = 80.f);
    void BeginCharge();
    void EmitBolt();
    void CancelAttack();
    void ReleaseAttackSlot();
    void Die(bool bReportEncounter);
    void HideDefeatedBody();
    void SetGlow(float Brightness);
    void PlayCue(USoundBase* Cue, float Volume = 1.f);
    void ConfigureCharacterVisuals();
    void UpdateHeavyAnimation();
    void BeginBossAttack(EOBBossAttack Warning);
    void TickBossAttack(float DeltaSeconds);
    void FinishBossAttack();
    void ResetBossAttack();
    bool CanShockwaveHit(const AOBPlayerCharacter* Player, float PreviousRadius, float NewRadius) const;
    void ApplyBossHit(AOBPlayerCharacter* Player, float Damage);
    void CreateBossTelegraph();
    void UpdateBossTelegraph();
    void HideBossTelegraph();
    void StartBossIntroAnimation();
    void FinishBossIntroAnimation();
    void ResumeAfterBossIntro();
    friend class AOBMissionDirector;

#if WITH_DEV_AUTOMATION_TESTS
    friend class FOBEnemyAwarenessTest;
    friend class FOBEnemyCleanupTest;
    friend class FOBBossMechanicsTest;
    friend class FOBBossIntroTest;
#endif
};
