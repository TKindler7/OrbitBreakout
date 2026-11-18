#pragma once
#include "GameFramework/Character.h"
#include "OBMech.generated.h"

class AOBPlayerCharacter;
class AOBLevel2Director;
class UCameraComponent;

/** Articulated tunnel-service loader. The pilot's capsule reserves a safe dismount point. */
UCLASS()
class ORBITBREAKOUT_API AOBMech : public ACharacter
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,Category="Tuning") TSoftObjectPtr<class UOBMechTuning> Tuning;
    AOBMech();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual float TakeDamage(float Amount, const FDamageEvent&, AController*, AActor*) override;
    bool CanBoard(const AOBPlayerCharacter& P) const;
    bool Board(AOBPlayerCharacter& P);
    void Dismount();
    void DamageArmor(float Amount);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Armor = 280.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Heat = 0.f;
    UPROPERTY(EditAnywhere, Category="Mech") float CannonDamage = 68.f;
    UPROPERTY(EditAnywhere, Category="Mech") float HeatPerShot = 32.f;
    UPROPERTY(BlueprintReadOnly) bool bOverheated = false;
private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FOBPilotUpgradeTest;
#endif
    UPROPERTY() TObjectPtr<UCameraComponent> Cockpit;
    UPROPERTY() TWeakObjectPtr<AOBPlayerCharacter> Pilot;
    UPROPERTY() TWeakObjectPtr<AOBLevel2Director> Chapter;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Thighs[2];
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Shins[2];
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Feet[2];
    float Stride = 0.f;
    float GaitWeight = 0.f;
    FVector Pad;
    FRotator ReturnView;
    float NextShot = 0.f;
    bool ControlsLive() const;
    void Forward(float V);
    void Strafe(float V);
    void Turn(float V);
    void Look(float V);
    void TurnStick(float V);
    void LookStick(float V);
    void Fire();
};
