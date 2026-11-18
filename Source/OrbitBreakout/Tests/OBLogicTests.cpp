#include "../OBCore.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include <limits>

// These tests use transient objects and in-memory save archives. They never
// overwrite the owner's checkpoint or settings slots and need no loaded map.
namespace OBLogicTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::ClientContext | EAutomationTestFlags::CommandletContext |
        EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBSettingsClampTest,
    "OrbitBreakout.Logic.Settings.ClampAndPreserve", OBLogicTests::Flags)

bool FOBSettingsClampTest::RunTest(const FString& Parameters)
{
    UOBSettings* Settings = NewObject<UOBSettings>();
    struct FFloatSetting
    {
        const TCHAR* Name;
        float UOBSettings::*Member;
        float Min;
        float Max;
    };
    const FFloatSetting Fields[] = {
        {TEXT("Mouse sensitivity"), &UOBSettings::MouseSensitivity, .2f, 3.f},
        {TEXT("Master volume"), &UOBSettings::MasterVolume, 0.f, 1.f},
        {TEXT("Effects volume"), &UOBSettings::EffectsVolume, 0.f, 1.f},
        {TEXT("Ambience volume"), &UOBSettings::AmbienceVolume, 0.f, 1.f},
        {TEXT("Camera shake"), &UOBSettings::CameraShakeScale, 0.f, 1.f}
    };
    for (const FFloatSetting& Field : Fields)
    {
        Settings->*Field.Member = Field.Min - 100.f;
        Settings->Clamp();
        TestEqual(FString::Printf(TEXT("%s clamps below range"), Field.Name), Settings->*Field.Member, Field.Min);
        Settings->*Field.Member = Field.Max + 100.f;
        Settings->Clamp();
        TestEqual(FString::Printf(TEXT("%s clamps above range"), Field.Name), Settings->*Field.Member, Field.Max);
        const float UserValue = FMath::Lerp(Field.Min, Field.Max, .37f);
        Settings->*Field.Member = UserValue;
        Settings->Clamp();
        Settings->Clamp();
        TestEqual(FString::Printf(TEXT("%s preserves valid preference through repeated clamps"), Field.Name), Settings->*Field.Member, UserValue);

        // A malformed settings archive must not feed non-finite values into
        // camera, audio or rendering code.
        const float InvalidValues[] = {std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
        for (const float InvalidValue : InvalidValues)
        {
            Settings->*Field.Member = InvalidValue;
            Settings->Clamp();
            const float Result = Settings->*Field.Member;
            TestTrue(FString::Printf(TEXT("%s recovers a finite in-range preference"), Field.Name),
                FMath::IsFinite(Result) && Result >= Field.Min && Result <= Field.Max);
        }
    }
    Settings->GraphicsPreset = -1;
    Settings->Clamp();
    TestEqual(TEXT("Invalid low preset becomes Low"), Settings->GraphicsPreset, 0);
    Settings->GraphicsPreset = 3;
    Settings->Clamp();
    TestEqual(TEXT("Invalid high preset becomes High"), Settings->GraphicsPreset, 2);
    Settings->GraphicsPreset = 1;
    Settings->bInvertVerticalLook = true;
    Settings->bReducedFlashes = true;
    Settings->Clamp();
    TestEqual(TEXT("Medium remains selected"), Settings->GraphicsPreset, 1);
    TestTrue(TEXT("Clamping preserves inverted look"), Settings->bInvertVerticalLook);
    TestTrue(TEXT("Clamping preserves reduced flashes"), Settings->bReducedFlashes);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBSaveValidationTest,
    "OrbitBreakout.Logic.Checkpoint.ValidateBoundaries", OBLogicTests::Flags)

bool FOBSaveValidationTest::RunTest(const FString& Parameters)
{
    UOBMissionSave* Save = NewObject<UOBMissionSave>();
    TestTrue(TEXT("CP00 default snapshot is valid"), Save->IsValidSave());
    for (int32 Checkpoint = 0; Checkpoint <= 2; ++Checkpoint)
    {
        for (const int32 Reserve : {0, 96, 144})
        {
            for (const bool bCompleted : {false, true})
            {
                Save->SetCheckpointIdentity(Checkpoint);
                Save->ReserveAmmo = Reserve;
                Save->bMissionCompleted = bCompleted;
                TestTrue(FString::Printf(TEXT("CP%02d reserve %d completed %d is loadable"),
                    Checkpoint, Reserve, bCompleted), Save->IsValidSave());
            }
        }
    }
    Save->SetCheckpointIdentity(1);
    Save->ReserveAmmo = 96;
    Save->bMissionCompleted = false;
    for (const int32 Schema : {-1, 0, 3, MAX_int32})
    {
        Save->SchemaVersion = Schema;
        TestFalse(FString::Printf(TEXT("Unsupported schema %d cannot Continue"), Schema), Save->IsValidSave());
    }
    Save->SchemaVersion = 1;
    for (const FName Mission : {FName(NAME_None), FName(TEXT("DifferentMission"))})
    {
        Save->MissionId = Mission;
        TestFalse(TEXT("Unknown mission cannot Continue"), Save->IsValidSave());
    }
    Save->MissionId = TEXT("EmergencyDeparture");
    for (const int32 Checkpoint : {-1, 3, MIN_int32, MAX_int32})
    {
        Save->CheckpointId = Checkpoint;
        TestFalse(FString::Printf(TEXT("Unknown checkpoint %d cannot Continue"), Checkpoint), Save->IsValidSave());
    }
    Save->CheckpointId = 1;
    for (const int32 Reserve : {-1, 145, MIN_int32, MAX_int32})
    {
        Save->ReserveAmmo = Reserve;
        TestFalse(FString::Printf(TEXT("Out-of-range reserve %d cannot Continue"), Reserve), Save->IsValidSave());
    }
    Save->ReserveAmmo = 96;
    TestTrue(TEXT("A corrected snapshot is accepted again"), Save->IsValidSave());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBContinueEligibilityTest,
    "OrbitBreakout.Logic.Checkpoint.ContinueEligibility", OBLogicTests::Flags)

bool FOBContinueEligibilityTest::RunTest(const FString& Parameters)
{
    UOBGameInstance* Session = NewObject<UOBGameInstance>();
    TestFalse(TEXT("An empty session does not offer Continue"), Session->HasValidSave());
    Session->bSaveValid = true;
    TestFalse(TEXT("Validity flag without a snapshot does not offer Continue"), Session->HasValidSave());
    Session->MissionSave = NewObject<UOBMissionSave>(Session);
    Session->bSaveValid = false;
    TestFalse(TEXT("Default fallback snapshot does not masquerade as a saved mission"), Session->HasValidSave());
    Session->bSaveValid = true;
    TestTrue(TEXT("Validated in-memory CP00 is available for recovery"), Session->HasValidSave());
    Session->MissionSave->CheckpointId = 99;
    TestFalse(TEXT("Invalid snapshot is rejected even with a stale validity flag"), Session->HasValidSave());
    Session->MissionSave->SetCheckpointIdentity(2);
    Session->MissionSave->bMissionCompleted = true;
    TestTrue(TEXT("Completed snapshot remains valid for Replay presentation"), Session->HasValidSave());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBSaveRoundTripTest,
    "OrbitBreakout.Logic.Persistence.MemoryRoundTrip", OBLogicTests::Flags)

bool FOBSaveRoundTripTest::RunTest(const FString& Parameters)
{
    UOBMissionSave* Mission = NewObject<UOBMissionSave>();
    Mission->SetCheckpointIdentity(2);
    Mission->ReserveAmmo = 127;
    Mission->bMissionCompleted = true;
    TArray<uint8> MissionBytes;
    if (!TestTrue(TEXT("Mission serializes through Unreal SaveGame"), UGameplayStatics::SaveGameToMemory(Mission, MissionBytes))) return false;
    UOBMissionSave* RestoredMission = Cast<UOBMissionSave>(UGameplayStatics::LoadGameFromMemory(MissionBytes));
    if (!TestNotNull(TEXT("Mission archive restores the correct native class"), RestoredMission)) return false;
    TestTrue(TEXT("Restored mission passes validation"), RestoredMission->IsValidSave());
    TestEqual(TEXT("Checkpoint survives serialization"), RestoredMission->CheckpointId, 2);
    TestEqual(TEXT("Reserve survives serialization"), RestoredMission->ReserveAmmo, 127);
    TestEqual(TEXT("Mission ID survives serialization"), RestoredMission->MissionId, FName(TEXT("EmergencyDeparture")));
    TestTrue(TEXT("Completion survives serialization"), RestoredMission->bMissionCompleted);

    UOBSettings* Settings = NewObject<UOBSettings>();
    Settings->MouseSensitivity = 1.7f;
    Settings->MasterVolume = .61f;
    Settings->EffectsVolume = .42f;
    Settings->AmbienceVolume = .23f;
    Settings->GraphicsPreset = 2;
    Settings->CameraShakeScale = .12f;
    Settings->bInvertVerticalLook = true;
    Settings->bReducedFlashes = true;
    TArray<uint8> SettingsBytes;
    if (!TestTrue(TEXT("Settings serialize independently"), UGameplayStatics::SaveGameToMemory(Settings, SettingsBytes))) return false;
    UOBSettings* RestoredSettings = Cast<UOBSettings>(UGameplayStatics::LoadGameFromMemory(SettingsBytes));
    if (!TestNotNull(TEXT("Settings archive restores the correct native class"), RestoredSettings)) return false;
    RestoredSettings->Clamp();
    TestEqual(TEXT("Sensitivity survives serialization and clamp"), RestoredSettings->MouseSensitivity, 1.7f);
    TestEqual(TEXT("Master volume survives serialization"), RestoredSettings->MasterVolume, .61f);
    TestEqual(TEXT("Effects volume survives serialization"), RestoredSettings->EffectsVolume, .42f);
    TestEqual(TEXT("Ambience volume survives serialization"), RestoredSettings->AmbienceVolume, .23f);
    TestEqual(TEXT("Graphics preference survives serialization"), RestoredSettings->GraphicsPreset, 2);
    TestEqual(TEXT("Shake preference survives serialization"), RestoredSettings->CameraShakeScale, .12f);
    TestTrue(TEXT("Invert preference survives serialization"), RestoredSettings->bInvertVerticalLook);
    TestTrue(TEXT("Reduced-flashes preference survives serialization"), RestoredSettings->bReducedFlashes);
    TestEqual(TEXT("Settings archive leaves mission recovery unchanged"), RestoredMission->CheckpointId, 2);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
