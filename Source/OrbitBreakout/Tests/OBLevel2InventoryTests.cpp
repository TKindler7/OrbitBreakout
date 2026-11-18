#include "../OBPlayerCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

// Inventory-only fixture: no controller, BeginPlay, travel, disk save or camera RPC.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBChapterLoadoutTest,
    "OrbitBreakout.Level2.ExactCampaignLoadout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOBChapterLoadoutTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine available"), GEngine)) return false;
    FWorldContext& Context=GEngine->CreateNewWorldContext(EWorldType::Game);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,
        MakeUniqueObjectName(GetTransientPackage(),UWorld::StaticClass(),TEXT("OBChapterLoadout")),GetTransientPackage());
    Context.SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const FTransform Transform(FRotator::ZeroRotator,FVector(0,0,100));
    AOBPlayerCharacter* Player=World->SpawnActorDeferred<AOBPlayerCharacter>(AOBPlayerCharacter::StaticClass(),Transform,
        nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (Player)
    {
        Player->AutoPossessAI=EAutoPossessAI::Disabled;
        Player->AutoPossessPlayer=EAutoReceiveInput::Disabled;
        Player->FinishSpawning(Transform);
        Player->RestoreCampaignLoadout(true,7,51,true,9,72,EOBWeaponType::Helix12);
        TestEqual(TEXT("Selected weapon survives chapter travel"),Player->EquippedWeapon,EOBWeaponType::Helix12);
        TestEqual(TEXT("Inactive ARC magazine stays partial"),Player->GetArcMagazineAmmo(),7);
        TestEqual(TEXT("Inactive ARC reserve survives"),Player->GetArcReserveAmmo(),51);
        TestEqual(TEXT("Active HELIX magazine stays partial"),Player->GetHelixMagazineAmmo(),9);
        TestEqual(TEXT("Active HELIX reserve survives"),Player->GetHelixReserveAmmo(),72);
        Player->EquipWeapon(EOBWeaponType::Arc9);
        TestEqual(TEXT("Switch reveals exact restored ARC magazine"),Player->MagazineAmmo,7);
        TestEqual(TEXT("Switch retains HELIX stored magazine"),Player->GetHelixMagazineAmmo(),9);
        Player->RestoreCampaignLoadout(true,0,0,false,36,180,EOBWeaponType::Helix12);
        TestFalse(TEXT("An unearned HELIX is never granted"),Player->bHelixOwned);
        TestEqual(TEXT("Missing HELIX falls back to ARC"),Player->EquippedWeapon,EOBWeaponType::Arc9);
        TestEqual(TEXT("Empty campaign ammunition is preserved"),Player->MagazineAmmo+Player->ReserveAmmo,0);
        TestEqual(TEXT("Unowned HELIX has no hidden ammunition"),Player->GetHelixMagazineAmmo()+Player->GetHelixReserveAmmo(),0);
        Player->RestoreCampaignLoadout(true,999,999,true,999,999,EOBWeaponType::Helix12);
        TestEqual(TEXT("ARC magazine respects configured capacity"),Player->GetArcMagazineAmmo(),Player->GetArcMagazineCapacity());
        TestEqual(TEXT("ARC reserve respects 144 capacity"),Player->GetArcReserveAmmo(),144);
        TestEqual(TEXT("HELIX magazine respects 36 capacity"),Player->GetHelixMagazineAmmo(),36);
        TestEqual(TEXT("HELIX reserve respects 180 capacity"),Player->GetHelixReserveAmmo(),180);
        Player->RestoreCampaignLoadout(false,24,144,true,36,180,EOBWeaponType::Helix12);
        TestFalse(TEXT("Unarmed restore remains unarmed"),Player->bWeaponOwned);
        TestFalse(TEXT("HELIX cannot bypass base weapon ownership"),Player->bHelixOwned);
        TestEqual(TEXT("Unarmed restore clears all ammunition"),Player->GetArcMagazineAmmo()+Player->GetArcReserveAmmo()+Player->GetHelixMagazineAmmo()+Player->GetHelixReserveAmmo(),0);
    }
    else AddError(TEXT("Player could not spawn"));
    GEngine->ShutdownWorldNetDriver(World);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return Player != nullptr;
}
#endif
