#include "../OBPlayerCharacter.h"
#include "../OBCore.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"

namespace OBWeaponTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
    // Real components and timers, but no GameInstance, BeginPlay, travel or disk saves.
    struct FIsolatedWorld
    {
        UWorld* World = nullptr;
        FIsolatedWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context=GEngine->CreateNewWorldContext(EWorldType::Game);
            World=UWorld::CreateWorld(EWorldType::Game,false,
                MakeUniqueObjectName(GetTransientPackage(),UWorld::StaticClass(),TEXT("OBWeaponTest")),GetTransientPackage());
            Context.SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
        }
        ~FIsolatedWorld()
        {
            if(!World)return;
            GEngine->ShutdownWorldNetDriver(World);
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBWeaponInventoryTest,
    "OrbitBreakout.Weapons.HelixInventoryAndSwitch",OBWeaponTests::Flags)

bool FOBWeaponInventoryTest::RunTest(const FString& Parameters)
{
    OBWeaponTests::FIsolatedWorld Fixture;
    if(!TestNotNull(TEXT("Isolated world exists"),Fixture.World))return false;
    const FTransform Spawn(FRotator::ZeroRotator,FVector(0,0,100));
    AOBPlayerCharacter* Player=Fixture.World->SpawnActorDeferred<AOBPlayerCharacter>(AOBPlayerCharacter::StaticClass(),Spawn,
        nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if(!TestNotNull(TEXT("Player spawned"),Player))return false;
    Player->AutoPossessAI=EAutoPossessAI::Disabled;
    Player->AutoPossessPlayer=EAutoReceiveInput::Disabled;
    Player->FinishSpawning(Spawn);
    Player->RestoreLoadout(true,24,96);
    TestFalse(TEXT("Unclaimed HELIX cannot be selected"),Player->EquipWeapon(EOBWeaponType::Helix12));
    TestEqual(TEXT("ARC-9 still selected"),Player->EquippedWeapon,EOBWeaponType::Arc9);
    Player->MagazineAmmo=7;
    Player->ReserveAmmo=51;
    Player->AcquireHeavyWeapon();
    TestTrue(TEXT("HELIX pickup grants ownership"),Player->bHelixOwned);
    TestEqual(TEXT("Pickup automatically equips HELIX"),Player->EquippedWeapon,EOBWeaponType::Helix12);
    TestEqual(TEXT("HELIX has a full 36-round magazine"),Player->MagazineAmmo,36);
    TestEqual(TEXT("HELIX pickup supplies its own ammunition"),Player->ReserveAmmo,144);
    TestEqual(TEXT("HELIX damage profile"),Player->ShotDamage,36.f);
    TestEqual(TEXT("HELIX cadence"),Player->ShotInterval,.13f);
    TestTrue(TEXT("HELIX has visible added barrel geometry"),Player->HelixShroud->IsVisible());
    Player->MagazineAmmo=9;
    Player->ReserveAmmo=72;
    Player->AcquireHeavyWeapon();
    TestEqual(TEXT("Repeated acquisition does not refill ammunition"),Player->MagazineAmmo,9);
    TestEqual(TEXT("Repeated acquisition does not duplicate reserve"),Player->ReserveAmmo,72);
    Player->bReloading=true;
    Fixture.World->GetTimerManager().SetTimer(Player->ReloadTimer,Player,&AOBPlayerCharacter::CompleteReload,1.f,false);
    Fixture.World->GetTimerManager().SetTimer(Player->AutoReloadTimer,Player,&AOBPlayerCharacter::StartReload,.3f,false);
    TestTrue(TEXT("ARC-9 can be reselected"),Player->EquipWeapon(EOBWeaponType::Arc9));
    TestFalse(TEXT("Switch cancels an in-progress reload"),Player->bReloading);
    TestFalse(TEXT("Old reload callback is removed"),Fixture.World->GetTimerManager().TimerExists(Player->ReloadTimer));
    TestFalse(TEXT("Old automatic reload callback is removed"),Fixture.World->GetTimerManager().TimerExists(Player->AutoReloadTimer));
    TestEqual(TEXT("ARC-9 magazine survived pickup and switch"),Player->MagazineAmmo,7);
    TestEqual(TEXT("ARC-9 reserve is separate"),Player->ReserveAmmo,51);
    TestEqual(TEXT("ARC-9 damage returns"),Player->ShotDamage,20.f);
    TestFalse(TEXT("ARC-9 hides HELIX attachments"),Player->HelixShroud->IsVisible());
    Player->AddAmmo(1000);
    TestEqual(TEXT("ARC-9 ammo respects its 144 cap"),Player->ReserveAmmo,144);
    Player->EquipWeapon(EOBWeaponType::Helix12);
    TestEqual(TEXT("HELIX magazine survives switching"),Player->MagazineAmmo,9);
    TestEqual(TEXT("ARC refill did not refill HELIX"),Player->ReserveAmmo,72);
    Player->AddAmmo(1000);
    TestEqual(TEXT("HELIX ammo respects its separate 180 cap"),Player->ReserveAmmo,180);
    TestEqual(TEXT("Inactive ARC reserve remains available to checkpoints"),Player->GetArcReserveAmmo(),144);
    Player->RestoreLoadout(true,24,96);
    Player->RestoreHeavyWeapon(true,108,true);
    TestEqual(TEXT("Checkpoint recovery equips HELIX"),Player->EquippedWeapon,EOBWeaponType::Helix12);
    TestEqual(TEXT("Checkpoint recovery refills magazine"),Player->MagazineAmmo,36);
    TestEqual(TEXT("Checkpoint recovery restores HELIX reserve"),Player->ReserveAmmo,108);
    Player->RestoreLoadout(false,0,0);
    TestFalse(TEXT("Fresh mission does not retain HELIX ownership"),Player->bHelixOwned);
    TestFalse(TEXT("Fresh mission hides HELIX attachments"),Player->HelixShroud->IsVisible());
    TestEqual(TEXT("Fresh mission is unarmed"),Player->MagazineAmmo+Player->ReserveAmmo,0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBHelixSaveTest,
    "OrbitBreakout.Weapons.HelixMemoryPersistence",OBWeaponTests::Flags)

bool FOBHelixSaveTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Original ammo enum ordinal stays serialized-compatible"),static_cast<uint8>(EOBInteractionKind::Ammo),static_cast<uint8>(5));
    TestEqual(TEXT("New weapon kind appends after original kinds"),static_cast<uint8>(EOBInteractionKind::HeavyWeapon),static_cast<uint8>(6));
    UOBMissionSave* Snapshot=NewObject<UOBMissionSave>();
    Snapshot->CheckpointId=2;
    Snapshot->ReserveAmmo=96;
    TestTrue(TEXT("Schema-1 checkpoint with default absent weapon fields remains valid"),Snapshot->IsValidSave());
    TestFalse(TEXT("Old checkpoint leaves new rack available"),Snapshot->bHelixOwned);
    Snapshot->bHelixOwned=true;
    Snapshot->HelixReserveAmmo=133;
    TArray<uint8> Bytes;
    if(!TestTrue(TEXT("HELIX checkpoint serializes in memory only"),UGameplayStatics::SaveGameToMemory(Snapshot,Bytes)))return false;
    auto* Restored=Cast<UOBMissionSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if(!TestNotNull(TEXT("Checkpoint deserializes"),Restored))return false;
    TestTrue(TEXT("Restored snapshot passes validation"),Restored->IsValidSave());
    TestTrue(TEXT("Weapon ownership survives archive"),Restored->bHelixOwned);
    TestEqual(TEXT("Separate HELIX ammo survives archive"),Restored->HelixReserveAmmo,133);
    TestEqual(TEXT("ARC ammo remains independent"),Restored->ReserveAmmo,96);
    TestEqual(TEXT("Existing schema version remains compatible"),Restored->SchemaVersion,1);
    Restored->HelixReserveAmmo=181;
    TestFalse(TEXT("Oversized HELIX reserve is rejected"),Restored->IsValidSave());
    Restored->HelixReserveAmmo=-1;
    TestFalse(TEXT("Negative HELIX reserve is rejected"),Restored->IsValidSave());
    Restored->HelixReserveAmmo=108;
    Restored->CheckpointId=1;
    TestFalse(TEXT("HELIX cannot be restored before its encounter"),Restored->IsValidSave());
    return true;
}
#endif
