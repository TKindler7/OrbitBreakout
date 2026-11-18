#include "OBTraversalRoute.h"
#include "OBCore.h"
#include "OBMech.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AOBTraversalRoute::AOBTraversalRoute()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Route")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Plate(TEXT("/Game/OrbitBreakout/Materials/M_Metal"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Light(TEXT("/Game/OrbitBreakout/Materials/M_Emissive"));
    Cube = Shape.Object; Metal = Plate.Object; Stripe = Light.Object;
}

void AOBTraversalRoute::Panel(FVector Center, FVector Size, FName Ability, bool bMark)
{
    auto* C = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(C);
    C->SetupAttachment(GetRootComponent());
    C->SetStaticMesh(Cube);
    C->SetRelativeLocation(Center);
    C->SetRelativeScale3D(Size / 100.);
    C->SetCollisionProfileName(bMark ? TEXT("NoCollision") : TEXT("BlockAll"));
    C->SetCanEverAffectNavigation(!bMark);
    if (!Ability.IsNone()) C->ComponentTags.Add(Ability);
    if (UMaterialInterface* Mat = bMark ? Stripe.Get() : Metal.Get())
    {
        auto* MID = UMaterialInstanceDynamic::Create(Mat,this);
        MID->SetVectorParameterValue(TEXT("Color"),bMark ? FLinearColor(.06,.7,.64) : FLinearColor(.09,.17,.19));
        if (bMark) MID->SetScalarParameterValue(TEXT("EmissiveStrength"),1.3f);
        C->SetMaterial(0,MID);
    }
    C->RegisterComponent();
}

void AOBTraversalRoute::Sign(FVector Location, const TCHAR* Copy)
{
    auto* Label = NewObject<UTextRenderComponent>(this);
    AddInstanceComponent(Label);
    Label->SetupAttachment(GetRootComponent());
    Label->SetRelativeLocation(Location);
    Label->SetRelativeRotation(FRotator(0,180,0));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetText(FText::FromString(Copy));
    Label->SetTextRenderColor(FColor(130,238,222));
    Label->SetWorldSize(17);
    Label->RegisterComponent();
}

void AOBTraversalRoute::Install(AOBMissionDirector& D, bool bTunnel)
{
    if (!D.bEnableParkour) return;
    const FString Level = D.GetWorld()->GetMapName();
    if (!Level.EndsWith(bTunnel ? TEXT("L_DeadSignal") : TEXT("L_EmergencyDeparture"))) return;
    auto* R = D.GetWorld()->SpawnActor<AOBTraversalRoute>();
    if (!R) return;
    R->SetOwner(&D);
    const FName Vault(TEXT("OB_Mantle")), Wall(TEXT("OB_WallRun"));
    // All lane pieces are offset from the mandatory route, between existing mission gates.
    const FVector VaultBase = bTunnel ? FVector(5960,3550,34) : FVector(410,-270,0);
    R->Panel(VaultBase + FVector(0,0,39),FVector(160,175,78),Vault);
    R->Panel(VaultBase + FVector(0,0,78.5),FVector(151,168,1),NAME_None,true);
    R->Sign(VaultBase + FVector(-82,0,61),TEXT("VAULT  /  JUMP"));
    const FVector Lane = bTunnel ? FVector(9010,3050,0) : FVector(5560,-840,0);
    for (int Side : {-1,1})
        R->Panel(Lane + FVector(0,Side*105,64),FVector(140,16,128),NAME_None);
    R->Panel(Lane + FVector(0,0,125),FVector(140,225,22),NAME_None);
    R->Sign(Lane + FVector(-72,0,134),TEXT("SLIDE  /  CTRL • R3"));
    const FVector Run = bTunnel ? FVector(10060,2960,0) : FVector(6580,-990,0);
    R->Panel(Run + FVector(0,0,146),FVector(720,30,292),Wall);
    for (int Side : {-1,1})
    {
        R->Panel(Run + FVector(0,Side*16,155),FVector(700,2,7),NAME_None,true);
        R->Panel(Run + FVector(Side*450,90,32),FVector(175,160,64),Vault);
    }
    R->Sign(Run + FVector(-361,0,230),TEXT("SPRINT + JUMP\nWALL RUN"));
    if (!bTunnel)
    {
        R->Sign(FVector(9430,-390,190),TEXT("PRECISION SIGHTS  /  RMB • L2"));
        R->Panel(FVector(13650,-835,153),FVector(900,18,306),Wall);
        R->Panel(FVector(13650,-825,166),FVector(880,2,7),NAME_None,true);
    }
    else
    {
        // The dock is inside the Warden's room. Both exits stay accessible on foot.
        D.GetWorld()->SpawnActor<AOBMech>(FVector(16950,7490,158),FRotator::ZeroRotator);
    }
}
