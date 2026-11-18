#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "OBUI.generated.h"

class AOBMissionDirector;
class UOBGameInstance;

/** Owns menu focus and the boundary between menu input and gameplay input. */
UCLASS()
class ORBITBREAKOUT_API AOBPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AOBPlayerController();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
    virtual void FlushPressedKeys() override;
    virtual bool ShouldFlushKeysWhenViewportFocusChanges() const override { return true; }

    UFUNCTION(BlueprintCallable) void ShowScreen(FName Screen);
    UFUNCTION(BlueprintCallable) void HideScreen();
    UFUNCTION(BlueprintPure) bool IsMenuOpen() const { return !MenuScreen.IsNone(); }
    bool IsInteractPhysicallyHeld() const { return bPhysicalInteractHeld || bGamepadInteractHeld; }
    bool IsUsingGamepad() const { return bUsingGamepad; }
    UFUNCTION(BlueprintPure) AOBMissionDirector* GetMissionDirector();
    UFUNCTION(BlueprintPure) UOBGameInstance* GetOBGameInstance() const;
    UFUNCTION(BlueprintCallable) void PerformAction(FName Action);
    void AdjustSetting(FName Setting, float Direction, TOptional<float> Absolute = TOptional<float>());
    void NavigateBack();

    UPROPERTY(BlueprintReadOnly) FName MenuScreen = NAME_None;
    UPROPERTY(BlueprintReadOnly) FName ReturnScreen = TEXT("Main");
    UPROPERTY(BlueprintReadOnly) FName ConfirmAction = NAME_None;
    UPROPERTY(BlueprintReadOnly) FString ConfirmationText;

private:
    TWeakObjectPtr<AOBMissionDirector> CachedDirector;
    bool bBlockFireUntilRelease = false;
    bool bPhysicalInteractHeld = false;
    bool bGamepadInteractHeld = false;
    bool bUsingGamepad = false;
    void RequestConfirmation(FName Action, const FString& Text);
    void QuitToDesktop();
};

/** Compact immediate-mode presentation. Gameplay state remains in the director. */
UCLASS()
class ORBITBREAKOUT_API AOBHUD : public AHUD
{
    GENERATED_BODY()
public:
    AOBHUD();
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
    virtual void NotifyHitBoxBeginCursorOver(FName BoxName) override;
    void ClickAtCursor();
    void MoveFocus(int32 Direction);
    void ActivateFocused();
    void AdjustFocused(float Direction);
    void ResetFocus();

private:
    struct FMenuItem
    {
        FName Action;
        FString Label;
        FBox2D Bounds;
        bool bSlider = false;
        float SliderMinX = 0.f;
        float SliderWidth = 1.f;
    };
    TArray<FMenuItem> Items;
    int32 FocusIndex = 0;
    float UIScale = 1.f;
    float ViewW = 1920.f;
    float ViewH = 1080.f;
    FVector2D PreviousMouse = FVector2D(-1000.f, -1000.f);
    FName LastScreen;

    void DrawGameplay(AOBPlayerController* PC);
    void DrawMenu(AOBPlayerController* PC);
    void DrawSettings(AOBPlayerController* PC, float X, float Y, float W);
    void DrawControls(float X, float Y, float W);
    void Button(FName Action, const FString& Label, float X, float Y, float W, float H = 54.f);
    void SettingRow(FName Action, const FString& Label, const FString& Value, float Normalized, float X, float Y, float W, bool bSlider);
    void Text(const FString& Value, float X, float Y, float Size = 23.f, FLinearColor Color = FLinearColor::White);
    void WrappedText(const FString& Value, float X, float Y, float Width, float Size = 23.f, FLinearColor Color = FLinearColor::White);
    void Rect(float X, float Y, float W, float H, FLinearColor Color);
    void Line(float X1, float Y1, float X2, float Y2, FLinearColor Color, float Thickness = 1.f);
    float TextWidth(const FString& Value, float Size) const;
    void ActivateItem(int32 Index, bool bMouse);
};
