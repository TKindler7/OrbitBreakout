#pragma once
#include "GameFramework/Actor.h"
#include "OBTraversalRoute.generated.h"

class AOBMissionDirector;

/** Additive runtime fixtures. The saved maps and both generators retain ownership of their actors. */
UCLASS()
class ORBITBREAKOUT_API AOBTraversalRoute : public AActor
{
    GENERATED_BODY()
public:
    AOBTraversalRoute();
    static void Install(AOBMissionDirector& Director, bool bTunnel);
private:
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UMaterialInterface> Metal;
    UPROPERTY() TObjectPtr<UMaterialInterface> Stripe;
    void Panel(FVector Center, FVector Size, FName Ability, bool bMark = false);
    void Sign(FVector Location, const TCHAR* Copy);
};
