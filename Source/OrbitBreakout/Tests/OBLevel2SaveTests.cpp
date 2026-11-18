#include "../OBCore.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

namespace OBLevel2SaveTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::ClientContext | EAutomationTestFlags::CommandletContext |
        EAutomationTestFlags::ProductFilter;
}

// Only transient objects and in-memory archives are used. No disk checkpoint or
// settings slot is read, replaced or deleted by these tests.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBChapterMigrationTest,
    "OrbitBreakout.Level2.Save.LegacyMigration", OBLevel2SaveTests::Flags)
bool FOBChapterMigrationTest::RunTest(const FString&)
{
    const FName LegacyKeys[] = {TEXT("L1.Cell"), TEXT("L1.Cargo"), TEXT("L1.Reactor")};
    for (int32 CP = 0; CP != 3; ++CP)
    {
        UOBMissionSave* Save = NewObject<UOBMissionSave>();
        Save->SchemaVersion = 1;
        Save->CheckpointId = CP;
        Save->ReserveAmmo = CP == 0 ? 0 : 113;
        Save->bHelixOwned = CP == 2;
        Save->HelixReserveAmmo = CP == 2 ? 99 : 0;
        TestTrue(TEXT("Legacy checkpoint remains readable before migration"), Save->IsValidSave());
        TestTrue(TEXT("Known schema 1 migrates"), Save->MigrateLegacy());
        TestEqual(TEXT("Schema upgrades explicitly"), Save->SchemaVersion, 2);
        TestEqual(TEXT("Legacy checkpoint stays in Chapter 1"), Save->ChapterId, 1);
        TestEqual(TEXT("Legacy mission identity is retained"), Save->MissionId, FName(TEXT("EmergencyDeparture")));
        TestEqual(TEXT("Checkpoint index is retained"), Save->CheckpointId, CP);
        TestEqual(TEXT("Stable Chapter 1 checkpoint key is assigned"), Save->CheckpointKey, LegacyKeys[CP]);
        TestEqual(TEXT("ARC reserve survives migration"), Save->ReserveAmmo, CP == 0 ? 0 : 113);
        TestEqual(TEXT("HELIX ownership survives migration"), Save->bHelixOwned, CP == 2);
        TestEqual(TEXT("HELIX reserve survives migration"), Save->HelixReserveAmmo, CP == 2 ? 99 : 0);
        TestFalse(TEXT("Old checkpoint never becomes tunnel progress"), Save->bAuxPowerRestored || Save->bWardenDefeated || Save->bLevel2Completed);
        TestTrue(TEXT("Migrated checkpoint validates"), Save->IsValidSave());
        TestTrue(TEXT("Migration can safely repeat"), Save->MigrateLegacy());
        TestEqual(TEXT("Repeated migration preserves identity"), Save->CheckpointKey, LegacyKeys[CP]);
    }
    UOBMissionSave* Unsupported = NewObject<UOBMissionSave>();
    Unsupported->SchemaVersion = 99;
    TestFalse(TEXT("Future unknown schema is not silently reinterpreted"), Unsupported->MigrateLegacy());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBChapterSaveRoundTripTest,
    "OrbitBreakout.Level2.Save.ChapterAndLoadoutRoundTrip", OBLevel2SaveTests::Flags)
bool FOBChapterSaveRoundTripTest::RunTest(const FString&)
{
    UOBMissionSave* Save = NewObject<UOBMissionSave>();
    Save->ChapterId = 2;
    Save->MissionId = TEXT("DeadSignal");
    Save->SetCheckpointIdentity(3);
    Save->bArcOwned = true;
    Save->ArcMagazine = 7;
    Save->ReserveAmmo = 61;
    Save->bHelixOwned = true;
    Save->HelixMagazine = 19;
    Save->HelixReserveAmmo = 47;
    Save->SelectedWeapon = 1;
    Save->bLevel2IntroComplete = true;
    Save->bJunctionIsolated = true;
    Save->bJunctionDrained = true;
    Save->bCouplerOwned = true;
    Save->bCouplerInserted = true;
    Save->bAuxPowerRestored = true;
    Save->bWardenDefeated = true;
    TestTrue(TEXT("Canonical finale-entry checkpoint validates"), Save->IsValidSave());
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Chapter 2 serializes with Unreal's save archive"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return false;
    UOBMissionSave* Copy = Cast<UOBMissionSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("Chapter 2 archive restores its native class"), Copy)) return false;
    TestTrue(TEXT("Restored chapter validates"), Copy->IsValidSave());
    TestEqual(TEXT("Map identity survives"), Copy->MissionId, FName(TEXT("DeadSignal")));
    TestEqual(TEXT("Chapter identity survives"), Copy->ChapterId, 2);
    TestEqual(TEXT("Stable checkpoint survives"), Copy->CheckpointKey, FName(TEXT("L2.Finale")));
    TestEqual(TEXT("ARC magazine survives exactly"), Copy->ArcMagazine, 7);
    TestEqual(TEXT("ARC reserve survives exactly"), Copy->ReserveAmmo, 61);
    TestEqual(TEXT("HELIX magazine survives exactly"), Copy->HelixMagazine, 19);
    TestEqual(TEXT("HELIX reserve survives exactly"), Copy->HelixReserveAmmo, 47);
    TestEqual(TEXT("Selected weapon survives"), Copy->SelectedWeapon, uint8(1));
    TestTrue(TEXT("Completed junction survives"), Copy->bJunctionIsolated && Copy->bJunctionDrained);
    TestTrue(TEXT("Coupler and network progress survive"), Copy->bCouplerOwned && Copy->bCouplerInserted && Copy->bAuxPowerRestored);
    TestTrue(TEXT("Warden defeat survives"), Copy->bWardenDefeated);
    TestFalse(TEXT("Finale checkpoint starts with both stabilizers inactive"), Copy->bStabilizerA || Copy->bStabilizerB);
    Copy->ChapterId = 1;
    TestFalse(TEXT("Chapter 1 cannot reinterpret a Chapter 2 checkpoint"), Copy->IsValidSave());
    Copy->ChapterId = 2;
    Copy->CheckpointKey = TEXT("L1.Reactor");
    TestFalse(TEXT("Foreign checkpoint key is rejected"), Copy->IsValidSave());
    Copy->CheckpointKey = TEXT("L2.Finale");
    Copy->ArcMagazine = 25;
    TestFalse(TEXT("Over-capacity magazine is rejected"), Copy->IsValidSave());
    Copy->ArcMagazine = 7;
    Copy->SelectedWeapon = 9;
    TestFalse(TEXT("Unknown equipped weapon is rejected"), Copy->IsValidSave());
    return true;
}
#endif
