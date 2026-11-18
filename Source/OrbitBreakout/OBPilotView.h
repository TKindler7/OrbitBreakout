#pragma once

#include "Components/ActorComponent.h"
#include "OBPilotView.generated.h"

class AOBPlayerCharacter;

UCLASS()
class ORBITBREAKOUT_API UOBPilotView : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,Category="Tuning") TSoftObjectPtr<class UOBWeaponSightProfile> ArcProfile;
    UPROPERTY(EditAnywhere,Category="Tuning") TSoftObjectPtr<class UOBWeaponSightProfile> HelixProfile;
    UOBPilotView();
    virtual void BeginPlay() override;
    float ArcFOV = 68.f, HelixFOV = 62.f;
    UPROPERTY(VisibleAnywhere, Category="Precision sights") FVector ArcSightOffset = FVector(41,0,-19.3);
    UPROPERTY(VisibleAnywhere, Category="Precision sights") FVector HelixSightOffset = FVector(44,0,-20.4);
    UPROPERTY(EditAnywhere, Category="Precision sights") float BlendSeconds = .24f;
    UPROPERTY(BlueprintReadOnly, Category="Precision sights") float Alpha = 0.f;
    bool InTransition() const { return Alpha > KINDA_SMALL_NUMBER && Alpha < 1.f - KINDA_SMALL_NUMBER; }
    void UpdateView(AOBPlayerCharacter& Pilot, float Dt, bool bAllowADS);
    void ResetView(AOBPlayerCharacter& Pilot);
};
