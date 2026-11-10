#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "OBPlayerCharacter.generated.h"

class AOBMissionDirector;
class AOBInteractable;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class USceneComponent;

UENUM(BlueprintType)
enum class EOBWeaponType : uint8 { Arc9, Helix12 };

/** Rook: camera-relative movement and an obstruction-aware, timer-driven ARC-9. */
UCLASS()
class ORBITBREAKOUT_API AOBPlayerCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AOBPlayerCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Movement") float WalkSpeed = 450.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Movement") float SprintSpeed = 650.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Movement") float AimSpeed = 260.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Combat", meta=(ClampMin="1")) float MaxHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Combat", meta=(ClampMin="1")) float ShotDamage = 20.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Combat", meta=(ClampMin=".05")) float ShotInterval = .2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Combat", meta=(ClampMin="1")) int32 MagazineCapacity = 24;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Combat", meta=(ClampMin="1")) int32 MaxReserveAmmo = 144;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Combat", meta=(ClampMin=".1")) float ReloadDuration = 1.4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Combat") float WeaponRange = 6000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Combat") float AimSpreadDegrees = .4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Combat") float HipSpreadDegrees = 1.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Presentation") FName WeaponSocketName = TEXT("weapon_r");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Presentation") FTransform WeaponGripTransform = FTransform(FRotator(0.f, -90.f, 0.f));
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Presentation") FVector WeaponMuzzleOffset = FVector(0.f, 52.f, 3.f);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") float Health = 100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") int32 MagazineAmmo = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") int32 ReserveAmmo = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") bool bWeaponOwned = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") bool bHelixOwned = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") EOBWeaponType EquippedWeapon = EOBWeaponType::Arc9;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") bool bReloading = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") bool bAiming = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") bool bDead = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") bool bSprinting = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") bool bInteractHeld = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") bool bInteractionActive = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") float InteractionProgress = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") FString InteractionPrompt;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") float ReloadProgress = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") float LastHitTime = -100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") float LastShotTime = -100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") float LastDamageTime = -100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Runtime") FVector DamageDirection = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Camera") TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Camera") TObjectPtr<UCameraComponent> FollowCamera;

    UFUNCTION(BlueprintCallable) void RestoreLoadout(bool bOwned, int32 Magazine, int32 Reserve);
    UFUNCTION(BlueprintCallable) void CancelActions();
    void ClearHeldInputPreservingInteraction();
    UFUNCTION(BlueprintCallable) bool AddHealth(float Amount);
    UFUNCTION(BlueprintCallable) bool AddAmmo(int32 Amount);
    UFUNCTION(BlueprintPure) bool IsGameplayActive() const;
    UFUNCTION(BlueprintCallable) void AcquireWeapon();
    UFUNCTION(BlueprintCallable) void AcquireHeavyWeapon();
    UFUNCTION(BlueprintCallable) void RestoreHeavyWeapon(bool bOwned, int32 Reserve, bool bEquip = true);
    UFUNCTION(BlueprintCallable) bool EquipWeapon(EOBWeaponType Weapon);
    UFUNCTION(BlueprintPure) FString GetWeaponName() const;
    UFUNCTION(BlueprintPure) int32 GetArcReserveAmmo() const;
    UFUNCTION(BlueprintPure) int32 GetArcMagazineCapacity() const { return bArcTuningCached ? ArcCapacity : MagazineCapacity; }
    UFUNCTION(BlueprintPure) int32 GetHelixReserveAmmo() const;
    UFUNCTION(BlueprintCallable) void DamagePlayer(float Amount, FVector SourceLocation);
    UFUNCTION(BlueprintCallable) void EndSpawnGrace() { SpawnGraceUntil = 0.f; }
    UFUNCTION(BlueprintPure) FVector GetMuzzleLocation() const;
    UFUNCTION(BlueprintPure) FVector GetAimDirection() const;
    UFUNCTION(BlueprintCallable) void CancelReload();
    UFUNCTION(BlueprintCallable, Category="Orbit|Presentation") void RefreshWeaponAttachment();

private:
    UPROPERTY() TObjectPtr<USceneComponent> WeaponPivot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> WeaponBody;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> WeaponBarrel;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> WeaponStrip;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> MuzzleGlow;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> AgentLight;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> HelixShroud;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> HelixLeftRail;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> HelixRightRail;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> HelixCell;
    UPROPERTY() TObjectPtr<UMaterialInterface> ArcRifleMaterial;
    UPROPERTY() TObjectPtr<UStaticMesh> EffectMesh;
    UPROPERTY() TObjectPtr<UMaterialInterface> EffectMaterial;
    UPROPERTY() TWeakObjectPtr<AOBMissionDirector> Director;
    UPROPERTY() TWeakObjectPtr<AOBInteractable> FocusedInteraction;
    UPROPERTY() TWeakObjectPtr<AOBInteractable> HeldInteraction;
    int32 ArcMagazine = 0, ArcReserve = 0, HelixMagazine = 0, HelixReserve = 0;
    bool bArcTuningCached = false;
    float ArcDamage = 20.f, ArcInterval = .2f, ArcReload = 1.4f, ArcAimSpread = .4f, ArcHipSpread = 1.8f;
    int32 ArcCapacity = 24, ArcMaxReserve = 144;
    bool bFireHeld = false;
    bool bSprintHeld = false;
    float SpawnGraceUntil = 0.f;
    float ReloadStartTime = 0.f;
    float NextShotTime = 0.f;
    float NextEmptyCueTime = 0.f;
    float VisualRecoil = 0.f;
    float PromptAccumulator = 0.f;
    FTimerHandle FireTimer;
    FTimerHandle ReloadTimer;
    FTimerHandle AutoReloadTimer;

    void CacheArcTuning();
    void ApplyWeaponProfile(EOBWeaponType Weapon);
    void UpdateWeaponVisuals();
    void StoreEquippedAmmo();
    void SelectArc9();
    void SelectHelix12();
    void CycleWeapon();
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void StartJump();
    void StartSprint();
    void StopSprint();
    void StartAim();
    void StopAim();
    void StartFire();
    void StopFire();
    void FireTick();
    void FireShot();
    void StartReload();
    void CompleteReload();
    void StartInteract();
    void ReleaseInteract();
    void StopInteract();
    void UpdateSpeed();
    void UpdateInteraction(float DeltaSeconds);
    void DrawShotEffect(const FVector& Start, const FVector& End, bool bHit);
    void SetupVisualMaterials();
    void PlayCue(FName Cue, float Volume=1.f);
#if WITH_DEV_AUTOMATION_TESTS
    friend class FOBBossIntroTest;
    friend class FOBBossMechanicsTest;
    friend class FOBWeaponInventoryTest;
#endif
};
