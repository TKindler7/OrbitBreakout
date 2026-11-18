#include "OBUI.h"
#include "OBMech.h"
#include "OBCore.h"
#include "OBPlayerCharacter.h"
#include "OBEnemyCharacter.h"
#include "OBLevel2.h"
#include "OBMachineEnemy.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SViewport.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogOrbitGamepad, Log, All);

namespace OBPalette
{
    const FLinearColor Ink(.025f,.045f,.067f,1.f);
    const FLinearColor Panel(.045f,.075f,.105f,.96f);
    const FLinearColor White(.945f,.965f,.985f,1.f);
    const FLinearColor Muted(.58f,.69f,.76f,1.f);
    const FLinearColor Teal(.28f,.86f,.82f,1.f);
    const FLinearColor Amber(1.f,.70f,.33f,1.f);
    const FLinearColor Red(.95f,.43f,.43f,1.f);
    const FLinearColor Green(.55f,.88f,.67f,1.f);
}

AOBPlayerController::AOBPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    bShouldPerformFullTickWhenPaused = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

void AOBPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (PlayerCameraManager)
    {
        PlayerCameraManager->ViewPitchMin = -65.f;
        PlayerCameraManager->ViewPitchMax = 65.f;
    }
}

void AOBPlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
}

void AOBPlayerController::FlushPressedKeys()
{
    Super::FlushPressedKeys();
    // A release outside the game window never reaches InputKey. Clear our
    // separately tracked held actions whenever the engine flushes on focus loss.
    bPhysicalInteractHeld = false;
    bGamepadInteractHeld = false;
    bBlockFireUntilRelease = false;
    if (auto* L2 = Cast<AOBLevel2Director>(GetMissionDirector())) L2->SetSkipHeld(false);
    if (AOBPlayerCharacter* P = Cast<AOBPlayerCharacter>(GetPawn()))
    {
        P->ClearHeldInputPreservingInteraction();
        P->bInteractHeld = false;
        if (AOBMissionDirector* D = GetMissionDirector()) D->CancelHold();
    }
}

UOBGameInstance* AOBPlayerController::GetOBGameInstance() const
{
    return GetWorld() ? Cast<UOBGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
}

AOBMissionDirector* AOBPlayerController::GetMissionDirector()
{
    if (!CachedDirector.IsValid()) CachedDirector = AOBMissionDirector::Get(this);
    return CachedDirector.Get();
}

void AOBPlayerController::ShowScreen(FName Screen)
{
    if (auto* Mech = Cast<AOBMech>(GetPawn())) Mech->Dismount();
    if (Screen == TEXT("Settings") || Screen == TEXT("Controls") || Screen == TEXT("Confirm"))
    {
        if (MenuScreen != TEXT("Settings") && MenuScreen != TEXT("Controls") && MenuScreen != TEXT("Confirm"))
            ReturnScreen = MenuScreen.IsNone() ? FName(TEXT("Pause")) : MenuScreen;
    }
    const bool bChapter2 = Cast<AOBLevel2Director>(GetMissionDirector()) != nullptr;
    if (auto* L2 = Cast<AOBLevel2Director>(GetMissionDirector())) L2->SetSkipHeld(false);
    const bool bPreserveHold = !bChapter2 && (Screen == TEXT("Pause") ||
        ((Screen == TEXT("Settings") || Screen == TEXT("Controls") || Screen == TEXT("Confirm")) && ReturnScreen == TEXT("Pause")));
    if (AOBPlayerCharacter* P = Cast<AOBPlayerCharacter>(GetPawn()))
    {
        if (bPreserveHold) P->ClearHeldInputPreservingInteraction();
        else P->CancelActions();
    }
    MenuScreen = Screen;
    SetPause(true);
    ResetIgnoreMoveInput();
    ResetIgnoreLookInput();
    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);
    bShowMouseCursor = true;
    FInputModeGameAndUI Mode;
    // Canvas buttons have no focusable Slate widgets. Route the local player's
    // first controller event to the game even when the mouse has never clicked.
    if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
        Mode.SetWidgetToFocus(Viewport->GetGameViewportWidget());
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    // Changing menus is not a physical focus loss: retain the E state so a
    // paused terminal hold can resume only while the key is still held.
    Super::FlushPressedKeys();
    if (AOBHUD* H = Cast<AOBHUD>(GetHUD())) H->ResetFocus();
}

void AOBPlayerController::HideScreen()
{
    MenuScreen = NAME_None;
    bShowMouseCursor = false;
    ResetIgnoreMoveInput();
    ResetIgnoreLookInput();
    FInputModeGameOnly Mode;
    Mode.SetConsumeCaptureMouseDown(true);
    SetInputMode(Mode);
    Super::FlushPressedKeys();
    if (AOBPlayerCharacter* P = Cast<AOBPlayerCharacter>(GetPawn()))
    {
        P->ClearHeldInputPreservingInteraction();
        P->bInteractHeld = !Cast<AOBLevel2Director>(GetMissionDirector()) && IsInteractPhysicallyHeld();
        if (!IsInteractPhysicallyHeld())
            if (AOBMissionDirector* D = GetMissionDirector()) D->CancelHold();
    }
    SetPause(false);
}

bool AOBPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    if (Params.Key.IsGamepadKey() && (Params.Event == IE_Pressed ||
        (Params.Event == IE_Axis && FMath::Abs(Params.AmountDepressed) > .25f)))
    {
        UE_LOG(LogOrbitGamepad, Verbose, TEXT("%s value=%.2f screen=%s source=%s"), *Params.Key.ToString(),
            Params.AmountDepressed, *MenuScreen.ToString(), Params.IsSimulatedInput() ? TEXT("simulated") : TEXT("viewport"));
    }
    if (Params.Event == IE_Pressed || (Params.Event == IE_Axis && FMath::Abs(Params.AmountDepressed) > .25f))
        bUsingGamepad = Params.Key.IsGamepadKey();
    AOBLevel2Director* L2 = Cast<AOBLevel2Director>(GetMissionDirector());
    const bool bSkipKey = Params.Key == EKeys::C || Params.Key == EKeys::Gamepad_FaceButton_Right;
    if (L2 && bSkipKey && Params.Event == IE_Released) L2->SetSkipHeld(false);
    if (Params.Key == EKeys::E)
    {
        if (Params.Event == IE_Pressed) bPhysicalInteractHeld = true;
        if (Params.Event == IE_Released) bPhysicalInteractHeld = false;
    }
    if (Params.Key == EKeys::Gamepad_FaceButton_Left)
    {
        if (Params.Event == IE_Pressed && !IsMenuOpen()) bGamepadInteractHeld = true;
        if (Params.Event == IE_Released) bGamepadInteractHeld = false;
    }
    if (bBlockFireUntilRelease && Params.Key == EKeys::LeftMouseButton)
    {
        if (Params.Event == IE_Released) bBlockFireUntilRelease = false;
        return true;
    }
    const bool bPressed = Params.Event == IE_Pressed;
    const bool bRepeat = bPressed || Params.Event == IE_Repeat;
    if ((Params.Key == EKeys::Escape || Params.Key == EKeys::P || Params.Key == EKeys::Gamepad_Special_Right) && bPressed)
    {
        if (IsMenuOpen()) NavigateBack();
        else if (AOBMissionDirector* D = GetMissionDirector())
        {
            if ((L2 && L2->IsCinematicActive()) || D->Phase == EOBPhase::Ending) ShowScreen(TEXT("EndingPause"));
            else if (D->IsGameplayActive() || D->IsBossIntroActive()) ShowScreen(TEXT("Pause"));
        }
        return true;
    }
    if (IsMenuOpen())
    {
        AOBHUD* H = Cast<AOBHUD>(GetHUD());
        if (!H) return true;
        if (Params.Key == EKeys::LeftMouseButton && bPressed)
        {
            bBlockFireUntilRelease = true;
            H->ClickAtCursor();
        }
        else if ((Params.Key == EKeys::Down || Params.Key == EKeys::Tab || Params.Key == EKeys::Gamepad_DPad_Down || Params.Key == EKeys::Gamepad_LeftStick_Down) && bRepeat) H->MoveFocus(1);
        else if ((Params.Key == EKeys::Up || Params.Key == EKeys::Gamepad_DPad_Up || Params.Key == EKeys::Gamepad_LeftStick_Up) && bRepeat) H->MoveFocus(-1);
        else if ((Params.Key == EKeys::Enter || Params.Key == EKeys::SpaceBar || Params.Key == EKeys::Gamepad_FaceButton_Bottom) && bPressed) H->ActivateFocused();
        else if ((Params.Key == EKeys::Left || Params.Key == EKeys::Gamepad_DPad_Left) && bRepeat) H->AdjustFocused(-1.f);
        else if ((Params.Key == EKeys::Right || Params.Key == EKeys::Gamepad_DPad_Right) && bRepeat) H->AdjustFocused(1.f);
        else if (Params.Key == EKeys::Gamepad_FaceButton_Right && bPressed) NavigateBack();
        return true;
    }
    // Cinematic skip is separate from gameplay interaction and consumes the press/release.
    if (L2 && L2->IsCinematicActive())
    {
        if (bSkipKey) L2->SetSkipHeld(Params.Event != IE_Released);
        if (Params.Key == EKeys::Gamepad_FaceButton_Left || bSkipKey) return true;
    }
    else if (Params.Key == EKeys::Gamepad_FaceButton_Left)
    {
        if (auto* M = Cast<AOBMech>(GetPawn())) { if (bPressed) M->Dismount(); }
        else if (auto* P = Cast<AOBPlayerCharacter>(GetPawn()))
        {
            if (bPressed) P->GamepadContextPressed();
            if (Params.Event == IE_Released) P->GamepadContextReleased();
        }
        return true;
    }
    return Super::InputKey(Params);
}

void AOBPlayerController::NavigateBack()
{
    if (MenuScreen == TEXT("Pause") || MenuScreen == TEXT("EndingPause")) HideScreen();
    else if (MenuScreen == TEXT("Settings") || MenuScreen == TEXT("Controls") || MenuScreen == TEXT("Confirm")) ShowScreen(ReturnScreen);
}

void AOBPlayerController::RequestConfirmation(FName Action, const FString& Message)
{
    ConfirmAction = Action;
    ConfirmationText = Message;
    ShowScreen(TEXT("Confirm"));
}

void AOBPlayerController::QuitToDesktop()
{
    UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void AOBPlayerController::PerformAction(FName Action)
{
    UOBGameInstance* GI = GetOBGameInstance();
    AOBMissionDirector* D = GetMissionDirector();
    if (!GI) return;
    if (Action == TEXT("Resume")) HideScreen();
    else if (Action == TEXT("Continue")) GI->ContinueMission();
    else if (Action == TEXT("PlayLevel2") || Action == TEXT("ReplayLevel2")) GI->ReplayLevel2();
    else if (Action == TEXT("NewMission") || Action == TEXT("Restart") || Action == TEXT("Replay"))
    {
        if (Action != TEXT("NewMission") && Cast<AOBLevel2Director>(D)) { GI->ReplayLevel2(); return; }
        if (Action != TEXT("Replay") && GI->HasValidSave() && GI->MissionSave && !GI->MissionSave->bMissionCompleted)
            RequestConfirmation(TEXT("NewMission"), TEXT("Start from the holding cell? This replaces your saved mission progress. Your settings stay the same."));
        else GI->NewMission();
    }
    else if (Action == TEXT("Retry")) { if (D) D->RetryCheckpoint(); }
    else if (Action == TEXT("MainMenu"))
    {
        if (MenuScreen == TEXT("Pause"))
            RequestConfirmation(TEXT("MainMenu"), TEXT("Return to the main menu? Progress since your last checkpoint will be lost. Continue will restore that checkpoint."));
        else GI->ReturnToMenu();
    }
    else if (Action == TEXT("Settings")) ShowScreen(TEXT("Settings"));
    else if (Action == TEXT("Controls")) ShowScreen(TEXT("Controls"));
    else if (Action == TEXT("Back") || Action == TEXT("Cancel")) NavigateBack();
    else if (Action == TEXT("Confirm"))
    {
        if (ConfirmAction == TEXT("NewMission")) GI->NewMission();
        else if (ConfirmAction == TEXT("MainMenu")) GI->ReturnToMenu();
        ConfirmAction = NAME_None;
    }
    else if (Action == TEXT("SkipEnding"))
    {
        if (D && !Cast<AOBLevel2Director>(D) && D->GetEndingElapsed() >= 1.f) { SetPause(false); D->CompleteMission(); }
    }
    else if (Action == TEXT("ResetSettings"))
    {
        GI->Settings = NewObject<UOBSettings>(GI);
        GI->ApplySettings();
        GI->SaveSettings();
    }
    else if (Action == TEXT("Quit")) QuitToDesktop();
    else AdjustSetting(Action, 1.f);
}

void AOBPlayerController::AdjustSetting(FName Setting, float Direction, TOptional<float> Absolute)
{
    UOBGameInstance* GI = GetOBGameInstance();
    if (!GI || !GI->Settings) return;
    UOBSettings* S = GI->Settings;
    bool bChanged = true;
    const auto Unit = [&](float Current) { return Absolute.IsSet() ? Absolute.GetValue() : Current + Direction * .05f; };
    if (Setting == TEXT("Sensitivity")) S->MouseSensitivity = Absolute.IsSet() ? .2f + Absolute.GetValue() * 2.8f : S->MouseSensitivity + Direction * .1f;
    else if (Setting == TEXT("ADSSensitivity")) S->ADSSensitivity = Absolute.IsSet() ? .5f+.5f*Absolute.GetValue() : S->ADSSensitivity+Direction*.05f;
    else if (Setting == TEXT("Motion")) S->TraversalMotion = (S->TraversalMotion + (Direction < 0.f ? 2 : 1)) % 3;
    else if (Setting == TEXT("Roll")) S->WallRoll = Absolute.IsSet() ? 8.f*Absolute.GetValue() : S->WallRoll+Direction;
    else if (Setting == TEXT("Invert")) S->bInvertVerticalLook = !S->bInvertVerticalLook;
    else if (Setting == TEXT("Master")) S->MasterVolume = Unit(S->MasterVolume);
    else if (Setting == TEXT("Effects")) S->EffectsVolume = Unit(S->EffectsVolume);
    else if (Setting == TEXT("Ambience")) S->AmbienceVolume = Unit(S->AmbienceVolume);
    else if (Setting == TEXT("Graphics")) S->GraphicsPreset = (S->GraphicsPreset + (Direction < 0 ? 2 : 1)) % 3;
    else if (Setting == TEXT("Shake")) S->CameraShakeScale = Unit(S->CameraShakeScale);
    else if (Setting == TEXT("Flashes")) S->bReducedFlashes = !S->bReducedFlashes;
    else bChanged = false;
    if (bChanged)
    {
        S->Clamp();
        GI->ApplySettings();
        GI->SaveSettings();
    }
}

AOBHUD::AOBHUD()
{
    PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AOBHUD::Rect(float X, float Y, float W, float H, FLinearColor Color)
{
    DrawRect(Color, X*UIScale, Y*UIScale, W*UIScale, H*UIScale);
}

void AOBHUD::Line(float X1, float Y1, float X2, float Y2, FLinearColor Color, float Thickness)
{
    DrawLine(X1*UIScale,Y1*UIScale,X2*UIScale,Y2*UIScale,Color,Thickness*UIScale);
}

void AOBHUD::Text(const FString& Value, float X, float Y, float Size, FLinearColor Color)
{
    UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
    if (!Font || !Canvas) return;
    FCanvasTextItem Item(FVector2D(X*UIScale,Y*UIScale), FText::FromString(Value), Font, Color);
    const float FontScale = Size / FMath::Max(1.f,Font->GetMaxCharHeight()) * UIScale;
    Item.Scale = FVector2D(FontScale, FontScale);
    Item.EnableShadow(FLinearColor(0,0,0,.5f),FVector2D(1,1));
    Canvas->DrawItem(Item);
}

float AOBHUD::TextWidth(const FString& Value, float Size) const
{
    UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
    if (!Font || !Canvas) return 0.f;
    float W=0,H=0;
    Canvas->StrLen(Font,Value,W,H);
    return W * Size / FMath::Max(1.f,Font->GetMaxCharHeight());
}

void AOBHUD::WrappedText(const FString& Value,float X,float Y,float Width,float Size,FLinearColor Color)
{
    TArray<FString> Words;
    Value.ParseIntoArrayWS(Words);
    FString Row;
    float CY=Y;
    for (const FString& Word : Words)
    {
        const FString Candidate = Row.IsEmpty() ? Word : Row+TEXT(" ")+Word;
        if (!Row.IsEmpty() && TextWidth(Candidate,Size)>Width)
        {
            Text(Row,X,CY,Size,Color);
            CY += Size*1.35f;
            Row=Word;
        }
        else Row=Candidate;
    }
    if (!Row.IsEmpty()) Text(Row,X,CY,Size,Color);
}

void AOBHUD::ResetFocus()
{
    FocusIndex=0;
    PreviousMouse=FVector2D(-1000,-1000);
    Items.Reset();
}

void AOBHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;
    UIScale = FMath::Max(.1f,Canvas->ClipY/1080.f);
    ViewW=Canvas->ClipX/UIScale;
    ViewH=1080.f;
    AOBPlayerController* PC=Cast<AOBPlayerController>(PlayerOwner);
    if (!PC) return;
    if (LastScreen!=PC->MenuScreen) { ResetFocus(); LastScreen=PC->MenuScreen; }
    Items.Reset();
    if (PC->IsMenuOpen()) DrawMenu(PC); else DrawGameplay(PC);
    FocusIndex=FMath::Clamp(FocusIndex,0,FMath::Max(0,Items.Num()-1));
    float MX=0,MY=0;
    if (PC->GetMousePosition(MX,MY))
    {
        const FVector2D Mouse(MX/UIScale,MY/UIScale);
        if (!Mouse.Equals(PreviousMouse,.5f))
        {
            for (int32 I=0;I<Items.Num();++I) if(Items[I].Bounds.IsInside(Mouse)) {FocusIndex=I;break;}
            PreviousMouse=Mouse;
        }
    }
}

void AOBHUD::Button(FName Action,const FString& Label,float X,float Y,float W,float H)
{
    const int32 Index=Items.Num();
    const bool bFocused=Index==FocusIndex;
    Rect(X,Y,W,H,bFocused?FLinearColor(.07f,.23f,.26f,.98f):FLinearColor(.075f,.11f,.15f,.98f));
    Rect(X,Y,bFocused?5.f:2.f,H,bFocused?OBPalette::Teal:FLinearColor(.2f,.31f,.36f,1));
    Line(X,Y+H,X+W,Y+H,FLinearColor(.17f,.25f,.29f,1));
    Text(Label,X+23,Y+(H-25)*.5f,25,bFocused?OBPalette::White:OBPalette::Muted);
    if(bFocused) Text(TEXT(">"),X+W-35,Y+(H-25)*.5f,25,OBPalette::Teal);
    FMenuItem Item; Item.Action=Action; Item.Label=Label; Item.Bounds=FBox2D(FVector2D(X,Y),FVector2D(X+W,Y+H));
    Items.Add(Item);
    AddHitBox(FVector2D(X,Y)*UIScale,FVector2D(W,H)*UIScale,Action,true,10);
}

void AOBHUD::SettingRow(FName Action,const FString& Label,const FString& Value,float Normalized,float X,float Y,float W,bool bSlider)
{
    const bool bFocused=Items.Num()==FocusIndex;
    Rect(X,Y,W,48,bFocused?FLinearColor(.065f,.20f,.23f,1):FLinearColor(.065f,.10f,.14f,1));
    if(bFocused) Rect(X,Y,4,48,OBPalette::Teal);
    Text(Label,X+20,Y+10,23,OBPalette::White);
    const float SX=X+W*.53f, SW=W*.29f;
    if(bSlider)
    {
        Rect(SX,Y+24,SW,5,FLinearColor(.18f,.26f,.30f,1));
        Rect(SX,Y+24,SW*FMath::Clamp(Normalized,0.f,1.f),5,OBPalette::Teal);
        Rect(SX+SW*FMath::Clamp(Normalized,0.f,1.f)-3,Y+17,6,19,OBPalette::White);
    }
    Text(Value,X+W-TextWidth(Value,23)-20,Y+10,23,bSlider?OBPalette::Muted:OBPalette::Teal);
    FMenuItem Item;Item.Action=Action;Item.Label=Label;Item.Bounds=FBox2D(FVector2D(X,Y),FVector2D(X+W,Y+48));Item.bSlider=bSlider;Item.SliderMinX=SX;Item.SliderWidth=SW;
    Items.Add(Item);
    AddHitBox(FVector2D(X,Y)*UIScale,FVector2D(W,48)*UIScale,Action,true,10);
}

void AOBHUD::DrawSettings(AOBPlayerController* PC,float X,float Y,float W)
{
    UOBGameInstance* GI=PC->GetOBGameInstance();
    if(!GI || !GI->Settings)return;
    UOBSettings* S=GI->Settings;
    auto Percent=[](float V){return FString::Printf(TEXT("%d%%"),FMath::RoundToInt(V*100));};
    SettingRow(TEXT("Sensitivity"),TEXT("Look sensitivity"),FString::Printf(TEXT("%.1f"),S->MouseSensitivity),(S->MouseSensitivity-.2f)/2.8f,X,Y,W,true);
    SettingRow(TEXT("Invert"),TEXT("Invert vertical look"),S->bInvertVerticalLook?TEXT("ON"):TEXT("OFF"),0,X,Y+52,W,false);
    SettingRow(TEXT("Master"),TEXT("Master volume"),Percent(S->MasterVolume),S->MasterVolume,X,Y+104,W,true);
    SettingRow(TEXT("Effects"),TEXT("Effects volume"),Percent(S->EffectsVolume),S->EffectsVolume,X,Y+156,W,true);
    SettingRow(TEXT("Ambience"),TEXT("Environment ambience"),Percent(S->AmbienceVolume),S->AmbienceVolume,X,Y+208,W,true);
    const TCHAR* Presets[]={TEXT("LOW"),TEXT("MEDIUM"),TEXT("HIGH")};
    SettingRow(TEXT("Graphics"),TEXT("Graphics quality"),Presets[FMath::Clamp(S->GraphicsPreset,0,2)],0,X,Y+260,W,false);
    SettingRow(TEXT("Shake"),TEXT("Camera shake"),Percent(S->CameraShakeScale),S->CameraShakeScale,X,Y+312,W,true);
    SettingRow(TEXT("Flashes"),TEXT("Reduced flashes"),S->bReducedFlashes?TEXT("ON"):TEXT("OFF"),0,X,Y+364,W,false);
    const TCHAR* Motions[] = {TEXT("OFF"),TEXT("REDUCED"),TEXT("FULL")};
    SettingRow(TEXT("ADSSensitivity"),TEXT("Aim sensitivity"),Percent(S->ADSSensitivity),(S->ADSSensitivity-.5f)*2.f,X,Y+416,W,true);
    SettingRow(TEXT("Motion"),TEXT("Traversal camera"),Motions[FMath::Clamp(S->TraversalMotion,0,2)],0,X,Y+468,W,false);
    SettingRow(TEXT("Roll"),TEXT("Wall-run tilt"),FString::Printf(TEXT("%.0f°"),S->WallRoll),S->WallRoll/8.f,X,Y+520,W,true);
    Button(TEXT("ResetSettings"),TEXT("Reset defaults"),X,Y+578,W*.48f);
    Button(TEXT("Back"),TEXT("Back"),X+W*.52f,Y+578,W*.48f);
    Text(TEXT("Click a slider or use Left / Right. Changes save automatically."),X,Y+650,20,OBPalette::Muted);
}

void AOBHUD::DrawControls(float X,float Y,float W)
{
    const TCHAR* Keys[]={TEXT("WASD / L STICK"),TEXT("MOUSE / R STICK"),TEXT("SHIFT / L3"),TEXT("SPACE / CROSS"),TEXT("R MOUSE / L2"),TEXT("L MOUSE / R2"),TEXT("R / SQUARE"),TEXT("Q / TRIANGLE"),TEXT("E / SQUARE"),TEXT("CTRL / R3"),TEXT("ESC / OPTIONS")};
    const TCHAR* Labels[]={TEXT("Move"),TEXT("Look"),TEXT("Sprint"),TEXT("Jump / mantle / wall-kick"),TEXT("Precision sights (hold)"),TEXT("Fire equipped weapon"),TEXT("Reload"),TEXT("Select / switch weapon"),TEXT("Interact (takes priority over reload)"),TEXT("Slide while sprinting"),TEXT("Pause and resume")};
    for(int32 I=0;I<UE_ARRAY_COUNT(Keys);++I)
    {
        Rect(X,Y+I*45,W,44,FLinearColor(.065f,.105f,.14f,1));
        Text(Keys[I],X+18,Y+I*45+10,17,OBPalette::Teal);
        Text(Labels[I],X+W*.40f,Y+I*45+10,22,OBPalette::White);
    }
    WrappedText(TEXT("Use cover to break line of sight. Strafe away from the Bull's charge lane and jump its shockwave. Q / Triangle switches weapons. Hold C / Circle to skip a chapter cinematic. Square interacts when a control is in reach; otherwise it reloads."),X,Y+515,W,23,OBPalette::Muted);
    Button(TEXT("Back"),TEXT("Back"),X,Y+640,W);
}

void AOBHUD::DrawMenu(AOBPlayerController* PC)
{
    using namespace OBPalette;
    UOBGameInstance* GI=PC->GetOBGameInstance();
    const FName Screen=PC->MenuScreen;
    const bool bChapter2=Cast<AOBLevel2Director>(PC->GetMissionDirector()) != nullptr;
    const bool bWide=Screen==TEXT("Settings") || Screen==TEXT("Controls");
    const float X=70, W=bWide?1020.f:710.f;
    Rect(0,0,ViewW,ViewH,FLinearColor(.015f,.03f,.045f,.66f));
    Rect(X,64,W,952,Panel);
    Rect(X,64,W,3,Teal);
    Text(bChapter2?TEXT("AURELIA FREIGHT NETWORK  /  SECTOR 07"):TEXT("ASTERION SYSTEMS  /  HELIOS ARK"),X+38,98,19,Muted);
    Text(bChapter2?TEXT("LEVEL 2 — DEAD SIGNAL"):TEXT("EMERGENCY DEPARTURE"),ViewW-450,64,20,Teal);
    Text(bChapter2?TEXT("UNDERGROUND TUNNELS  //  LOCAL SESSION"):TEXT("DETENTION DECK  //  LOCAL SESSION"),ViewW-450,94,16,Muted);
    if(Screen==TEXT("Main"))
    {
        Text(TEXT("ORBIT"),X+36,159,94,White);
        Text(TEXT("BREAKOUT"),X+36,246,94,White);
        Rect(X+40,357,66,4,Teal);
        Text(TEXT("ONE AGENT. ONE WAY OUT."),X+127,348,20,Muted);
        float Y=423;
        if(GI && GI->HasValidSave())
        {
            const bool bComplete=GI->MissionSave && GI->MissionSave->bMissionCompleted;
            const bool bL2Complete=bComplete && GI->MissionSave->ChapterId==2;
            Button(bL2Complete?TEXT("ReplayLevel2"):TEXT("Continue"),bL2Complete?TEXT("Replay Level 2"):bComplete?TEXT("Continue to Level 2"):TEXT("Continue"),X+40,Y,W-80);Y+=68;
        }
        Button(TEXT("NewMission"),TEXT("New campaign"),X+40,Y,W-80);Y+=68;
        Button(TEXT("PlayLevel2"),TEXT("Play Level 2 — separate replay"),X+40,Y,W-80);Y+=68;
        Button(TEXT("Settings"),TEXT("Settings"),X+40,Y,W-80);Y+=68;
        Button(TEXT("Controls"),TEXT("Controls"),X+40,Y,W-80);Y+=68;
        Button(TEXT("Quit"),TEXT("Quit"),X+40,Y,W-80);
        if(GI && !GI->SaveWarning.IsEmpty()) WrappedText(GI->SaveWarning,X+40,870,W-80,20,Amber);
        Text(TEXT("OFFLINE SINGLE PLAYER  /  MOUSE + KEYBOARD + CONTROLLER"),X+40,968,17,Muted);
    }
    else if(Screen==TEXT("Settings") || Screen==TEXT("Controls"))
    {
        Text(Screen==TEXT("Settings")?TEXT("SETTINGS"):TEXT("CONTROLS"),X+38,157,58,White);
        Text(Screen==TEXT("Settings")?TEXT("Make the mission feel right."):TEXT("Your controls for both chapters."),X+40,231,23,Muted);
        if(Screen==TEXT("Settings")) DrawSettings(PC,X+40,290,W-80);
        else DrawControls(X+40,283,W-80);
    }
    else if(Screen==TEXT("Confirm"))
    {
        Text(PC->ConfirmAction==TEXT("NewMission")?TEXT("START OVER?"):TEXT("LEAVE MISSION?"),X+38,167,47,White);
        WrappedText(PC->ConfirmationText,X+40,262,W-80,27,Muted);
        Button(TEXT("Confirm"),PC->ConfirmAction==TEXT("NewMission")?TEXT("Start new mission"):TEXT("Return to main menu"),X+40,438,W-80);
        Button(TEXT("Cancel"),TEXT("Cancel"),X+40,507,W-80);
    }
    else if(Screen==TEXT("Pause"))
    {
        Text(TEXT("MISSION PAUSED"),X+38,166,51,White);
        Text(bChapter2?TEXT("Your last checkpoint is ready when you are."):TEXT("Take a breath. The ship can wait."),X+40,244,23,Muted);
        float Y=326;
        Button(TEXT("Resume"),TEXT("Resume"),X+40,Y,W-80);Y+=68;
        Button(TEXT("Retry"),TEXT("Retry checkpoint"),X+40,Y,W-80);Y+=68;
        Button(TEXT("Restart"),bChapter2?TEXT("Replay Level 2"):TEXT("Restart mission"),X+40,Y,W-80);Y+=68;
        Button(TEXT("Settings"),TEXT("Settings"),X+40,Y,W-80);Y+=68;
        Button(TEXT("Controls"),TEXT("Controls"),X+40,Y,W-80);Y+=68;
        Button(TEXT("MainMenu"),TEXT("Main menu"),X+40,Y,W-80);
    }
    else if(Screen==TEXT("Dead"))
    {
        Rect(X,64,W,3,Red);
        Text(TEXT("MISSION"),X+38,168,65,White);
        Text(TEXT("INTERRUPTED"),X+38,239,65,Red);
        WrappedText(TEXT("Rook's signal was lost. Restore your last checkpoint and find another route through."),X+40,346,W-80,25,Muted);
        Button(TEXT("Retry"),TEXT("Retry checkpoint"),X+40,490,W-80);
        Button(TEXT("Restart"),bChapter2?TEXT("Replay Level 2"):TEXT("Restart mission"),X+40,558,W-80);
        Button(TEXT("MainMenu"),TEXT("Main menu"),X+40,626,W-80);
    }
    else if(Screen==TEXT("Complete"))
    {
        Rect(X,64,W,3,Green);
        Text(bChapter2?TEXT("LEVEL 2"):TEXT("MISSION"),X+38,168,76,White);
        Text(TEXT("COMPLETE"),X+38,246,76,Green);
        Text(bChapter2?TEXT("SIGNAL RECEIVED  //  RESCUE RESPONDING"):TEXT("ROOK  //  SIGNAL CONFIRMED"),X+40,364,22,Teal);
        WrappedText(bChapter2?TEXT("Your distress call reached the surface. Rescue has answered. Hold your position, Rook. They are coming."):TEXT("The Helios Ark falls behind you. Two stolen weapons. One emergency departure."),X+40,416,W-80,26,Muted);
        Button(bChapter2?TEXT("ReplayLevel2"):TEXT("Replay"),bChapter2?TEXT("Replay Level 2"):TEXT("Replay mission"),X+40,569,W-80);
        Button(TEXT("MainMenu"),TEXT("Main menu"),X+40,637,W-80);
        Button(TEXT("Quit"),TEXT("Quit"),X+40,705,W-80);
        if(GI && !GI->SaveWarning.IsEmpty()) WrappedText(GI->SaveWarning,X+40,850,W-80,20,Amber);
    }
    else if(Screen==TEXT("EndingPause"))
    {
        Text(bChapter2?TEXT("CINEMATIC PAUSED"):TEXT("DEPARTURE PAUSED"),X+38,165,46,White);
        Button(TEXT("Resume"),bChapter2?TEXT("Resume cinematic"):TEXT("Resume departure"),X+40,320,W-80);
        if(AOBMissionDirector* D=PC->GetMissionDirector())
        {
            if(bChapter2) Text(TEXT("Resume, then hold C / Circle for one second to skip."),X+40,404,20,Muted);
            else if(D->GetEndingElapsed()>=1.f) Button(TEXT("SkipEnding"),TEXT("Continue to Level 2"),X+40,388,W-80);
            else Text(TEXT("Resume for a moment to enable skip."),X+40,404,22,Muted);
        }
        Button(TEXT("Settings"),TEXT("Settings"),X+40,482,W-80);
        Button(TEXT("MainMenu"),TEXT("Main menu"),X+40,550,W-80);
    }
    Text(PC->IsUsingGamepad()?TEXT("LEFT STICK / D-PAD  NAVIGATE     CROSS  SELECT     CIRCLE  BACK"):TEXT("ARROWS / TAB  NAVIGATE     ENTER  SELECT"),X+40,bWide?987.f:931.f,17,Muted);

    // A restrained navigation motif keeps the ship view visible beside the menu.
    if(!bWide && ViewW>1450 && !bChapter2)
    {
        const float RX=ViewW-390, CY=405;
        const TCHAR* Names[]={TEXT("HOLDING CELLS"),TEXT("SECURITY"),TEXT("CARGO"),TEXT("TRANSIT CONTROL"),TEXT("REACTOR"),TEXT("ESCAPE POD")};
        Text(TEXT("PRISON DECK / ROUTE"),RX-28,CY-58,18,Muted);
        for(int I=0;I<6;++I)
        {
            if(I<5) Line(RX,CY+I*69+8,RX,CY+(I+1)*69, FLinearColor(.2f,.42f,.45f,.8f),2);
            Rect(RX-4,CY+I*69,9,9,I==5?Teal:Muted);
            Text(Names[I],RX+26,CY+I*69-5,19,I==5?Teal:Muted);
        }
    }
}

void AOBHUD::DrawGameplay(AOBPlayerController* PC)
{
    using namespace OBPalette;
    AOBPlayerCharacter* P=Cast<AOBPlayerCharacter>(PC->GetPawn());
    AOBMissionDirector* D=PC->GetMissionDirector();
    if (auto* M = Cast<AOBMech>(PC->GetPawn()))
    {
        const float CX = ViewW*.5f, CY = ViewH*.5f;
        Text(TEXT("WARDEN BAY  /  LOADER"),56,38,25,Teal);
        Text(D ? D->GetObjectiveText() : FString(),56,76,20,White);
        Rect(CX-9,CY-1,18,2,Teal); Rect(CX-1,CY-9,2,18,Teal);
        Text(FString::Printf(TEXT("ARMOR   %.0f"),M->Armor),56,ViewH-118,28,White);
        Text(FString::Printf(TEXT("PULSE HEAT   %.0f%%"),M->Heat),56,ViewH-78,24,M->bOverheated ? Amber : Teal);
        Text(PC->IsUsingGamepad()?TEXT("R2   PULSE     SQUARE   EXIT"):TEXT("LMB   PULSE     E   EXIT"),ViewW-435,ViewH-80,22,White);
        return;
    }
    if(!P || !D)return;
    AOBLevel2Director* L2=Cast<AOBLevel2Director>(D);
    if(L2 && L2->IsCinematicActive())
    {
        Rect(0,0,ViewW,96,Ink); Rect(0,ViewH-178,ViewW,178,Ink);
        Text(L2->GetChapterLabel(),64,34,25,Teal);
        Text(PC->IsUsingGamepad()?TEXT("OPTIONS  Pause"):TEXT("ESC  Pause"),ViewW-270,36,20,Muted);
        const FString Subtitle=L2->GetSubtitle();
        WrappedText(Subtitle,ViewW*.18f,ViewH-145,ViewW*.64f,26,White);
        if(L2->GetCinematicElapsed()>=2.f)
        {
            Text(PC->IsUsingGamepad()?TEXT("Hold CIRCLE to skip"):TEXT("Hold C to skip"),64,ViewH-53,20,Muted);
            Rect(64,ViewH-23,228,3,Panel);
            Rect(64,ViewH-23,228*FMath::Clamp(L2->GetSkipProgress(),0.f,1.f),3,Teal);
        }
        return;
    }
    if(D->IsBossIntroActive())
    {
        const float Fade=FMath::Clamp(D->GetBossIntroElapsed()/.35f,0.f,1.f);
        Rect(0,0,ViewW,80,FLinearColor(.008f,.012f,.02f,.94f));
        Rect(0,ViewH-168,ViewW,168,FLinearColor(.008f,.012f,.02f,.94f));
        Text(TEXT("REACTOR 04  /  CONTAINMENT BREACH"),64,27,20,FLinearColor(1.f,.7f,.33f,Fade));
        Text(TEXT("ESC / P  Pause"),ViewW-250,27,20,Muted);
        const FString Title(TEXT("BULL SENTINEL"));
        Text(Title,(ViewW-TextWidth(Title,48.f))*.5f,ViewH-137,48,FLinearColor(1.f,.88f,.68f,Fade));
        const FString Subtitle=D->IsBossIntroReturning()?TEXT("DODGE THE CHARGE. JUMP THE SHOCKWAVE."):TEXT("HEAVY RESPONSE UNIT  /  POWERING UP");
        Text(Subtitle,(ViewW-TextWidth(Subtitle,20.f))*.5f,ViewH-69,20,Muted);
        return;
    }
    if(D->Phase==EOBPhase::Ending)
    {
        Rect(0,0,ViewW,105,Ink);Rect(0,975,ViewW,105,Ink);
        Text(TEXT("EMERGENCY DEPARTURE"),68,40,25,Teal);
        Text(TEXT("ESC / P  Pause or skip departure"),68,1015,22,Muted);
        return;
    }
    const float Margin=64;
    Rect(Margin,Margin,540,136,Panel);Rect(Margin,Margin,4,136,Teal);
    Text(L2?L2->GetChapterLabel():TEXT("EMERGENCY DEPARTURE"),Margin+25,Margin+18,18,Teal);
    WrappedText(D->GetObjectiveText(),Margin+25,Margin+50,485,25,White);
    if(D->ActiveEncounter>0)
        Text(FString::Printf(TEXT("SECURITY UNITS: %02d"),D->GetRemainingEnemies()),Margin+25,Margin+105,17,Amber);
    Text(PC->IsUsingGamepad()?TEXT("OPTIONS  Pause"):TEXT("ESC / P  Pause"),ViewW-250,Margin+10,20,Muted);
    if(L2 && L2->IsUplinkActive())
    {
        const float UX=ViewW*.5f-265, UY=ViewW>=1740.f?64.f:310.f;
        Rect(UX,UY,530,83,Panel);
        Text(L2->IsUplinkPaused()?TEXT("UPLINK PAUSED — REPAIR STABILIZER"):TEXT("DISTRESS UPLINK"),UX+18,UY+12,20,L2->IsUplinkPaused()?Amber:Teal);
        const float Progress=FMath::Clamp(L2->GetUplinkProgress(),0.f,1.f);
        Text(FString::Printf(TEXT("%02d%%"),FMath::RoundToInt(Progress*100)),UX+448,UY+13,19,White);
        Rect(UX+18,UY+49,494,10,FLinearColor(.16f,.22f,.25f,1));
        Rect(UX+18,UY+49,494*Progress,10,L2->IsUplinkPaused()?Amber:Teal);
    }
    if(L2 && L2->MissionState==EOBLevel2State::DefeatWarden)
    {
        for(const AOBMachineEnemy* Machine:L2->Machines)
        {
            if(!IsValid(Machine) || Machine->Kind!=EOBMachineKind::Warden || !Machine->bActive || Machine->IsDead())continue;
            const float BX=ViewW*.5f-270.f,BY=ViewW>=1740.f?64.f:310.f;
            Rect(BX,BY,540,94,Panel);Rect(BX,BY,540,3,Amber);
            Text(TEXT("RELAY WARDEN"),BX+18,BY+13,19,Amber);
            Text(FString::Printf(TEXT("%d / %d"),FMath::CeilToInt(Machine->Health),FMath::CeilToInt(Machine->MaxHealth)),BX+396,BY+14,17,White);
            Rect(BX+18,BY+43,504,10,FLinearColor(.16f,.19f,.23f,1));
            Rect(BX+18,BY+43,504*Machine->GetHealthFraction(),10,Machine->IsCoreExposed()?Teal:Amber);
            Text(Machine->GetAttackCue(),BX+18,BY+65,17,White);
            break;
        }
    }
    if(L2 && !L2->GetObjectiveLocation().IsNearlyZero())
    {
        FVector2D ScreenPoint;
        const FVector Target=L2->GetObjectiveLocation()+FVector(0,0,130);
        if(PC->ProjectWorldLocationToScreen(Target,ScreenPoint,true))
        {
            const float MX=FMath::Clamp(ScreenPoint.X/UIScale,42.f,ViewW-95.f);
            const float MY=FMath::Clamp(ScreenPoint.Y/UIScale,230.f,ViewH-310.f);
            Rect(MX-5,MY-5,10,10,Teal);
            const int32 Meters=FMath::RoundToInt(FVector::Distance(P->GetActorLocation(),Target)/100.f);
            Text(FString::Printf(TEXT("%dm"),Meters),MX+14,MY-12,18,Teal);
        }
    }

    Rect(Margin,ViewH-152,340,88,Panel);
    const FLinearColor HealthColor=P->Health<=25?Red:Teal;
    Rect(Margin+21,ViewH-125,25,7,HealthColor);Rect(Margin+30,ViewH-134,7,25,HealthColor);
    Text(FString::Printf(TEXT("%03d"),FMath::CeilToInt(P->Health)),Margin+63,ViewH-140,35,White);
    Text(P->Health<=25?TEXT("LOW HEALTH"):TEXT("ROOK / VITALS"),Margin+142,ViewH-131,18,P->Health<=25?Red:Muted);
    Rect(Margin+22,ViewH-91,296,7,FLinearColor(.16f,.22f,.25f,1));
    Rect(Margin+22,ViewH-91,296*FMath::Clamp(P->Health/FMath::Max(1.f,P->MaxHealth),0.f,1.f),7,HealthColor);
    if(P->bWeaponOwned)
    {
        const float AX=ViewW-Margin-306;
        Rect(AX,ViewH-160,306,96,Panel);
        Text(P->GetWeaponName(),AX+22,ViewH-146,18,Teal);
        Text(FString::Printf(TEXT("%02d"),P->MagazineAmmo),AX+22,ViewH-123,45,White);
        Text(FString::Printf(TEXT("/ %03d"),P->ReserveAmmo),AX+93,ViewH-108,23,Muted);
        Text(P->bReloading?TEXT("RELOADING"):P->MagazineAmmo+P->ReserveAmmo==0?TEXT("FIND AMMO"):(PC->IsUsingGamepad()?TEXT("SQUARE"):TEXT("R  RELOAD")),AX+176,ViewH-107,17,P->MagazineAmmo+P->ReserveAmmo==0?Amber:Muted);
        if(P->bReloading)Rect(AX+22,ViewH-77,262*FMath::Clamp(P->ReloadProgress,0.f,1.f),4,Teal);
        if(P->bHelixOwned)Text(PC->IsUsingGamepad()?TEXT("TRIANGLE  SWITCH WEAPON"):TEXT("1  ARC-9     2  HELIX-12     Q  SWITCH"),AX,ViewH-49,16,Muted);
    }
    if(D->ActiveEncounter==5)
    {
        for(const AOBEnemyCharacter* Boss:D->Enemies)
        {
            if(!IsValid(Boss)||!Boss->bHeavy||!Boss->bActive||Boss->IsDead())continue;
            const float BX=ViewW*.5f-270.f,BY=ViewW>=1740.f?64.f:310.f;
            const FLinearColor BossColor=Boss->IsBossEnraged()?Red:Amber;
            Rect(BX,BY,540,94,Panel);Rect(BX,BY,540,3,BossColor);
            Text(Boss->IsBossEnraged()?TEXT("BULL SENTINEL  /  OVERDRIVE"):TEXT("BULL SENTINEL  /  CONTAINMENT"),BX+18,BY+13,19,BossColor);
            Text(FString::Printf(TEXT("%d / %d"),FMath::CeilToInt(Boss->Health),FMath::CeilToInt(Boss->SentinelHealth)),BX+396,BY+14,17,White);
            Rect(BX+18,BY+43,504,10,FLinearColor(.16,.19,.23,1));
            Rect(BX+18,BY+43,504*Boss->GetBossHealthFraction(),10,BossColor);
            Text(Boss->GetBossAttackCue(),BX+18,BY+65,17,White);
            break;
        }
    }
    const float CX=ViewW*.5f,CY=ViewH*.5f;
    const float Now=GetWorld()->GetTimeSeconds();
    const bool bHit=Now-P->LastHitTime<.15f;
    const float Gap=P->bAiming?7.f:12.f;
    if(P->bWeaponOwned && !P->bDead)
    {
        const FLinearColor Cross=bHit?Teal:White;
        if(bHit)
        {
            Line(CX-12,CY-12,CX-5,CY-5,Cross,2);Line(CX+5,CY+5,CX+12,CY+12,Cross,2);
            Line(CX+5,CY-5,CX+12,CY-12,Cross,2);Line(CX-12,CY+12,CX-5,CY+5,Cross,2);
        }
        else
        {
            Line(CX-Gap-7,CY,CX-Gap,CY,Cross,2);Line(CX+Gap,CY,CX+Gap+7,CY,Cross,2);
            Line(CX,CY-Gap-7,CX,CY-Gap,Cross,2);Line(CX,CY+Gap,CX,CY+Gap+7,Cross,2);
            Rect(CX-1,CY-1,2,2,Cross);
        }
    }
    if(!P->InteractionPrompt.IsEmpty() && !P->bDead)
    {
        FString Prompt=P->InteractionPrompt;
        if(PC->IsUsingGamepad())
        {
            if(Prompt.StartsWith(TEXT("Hold E "))) Prompt=TEXT("Hold SQUARE ")+Prompt.RightChop(7);
            else if(Prompt.StartsWith(TEXT("E "))) Prompt=TEXT("SQUARE ")+Prompt.RightChop(2);
        }
        const float PW=FMath::Clamp(TextWidth(Prompt,24)+70,380.f,880.f);
        Rect(CX-PW*.5f,ViewH-265,PW,74,Panel);
        Text(Prompt,CX-TextWidth(Prompt,24)*.5f,ViewH-246,24,White);
        if(P->InteractionProgress>0)Rect(CX-PW*.5f,ViewH-196,PW*P->InteractionProgress,5,Teal);
    }
    if(!D->SystemMessage.IsEmpty() && Now<D->SystemMessageUntil)
    {
        const float MW=FMath::Min(ViewW-140,FMath::Max(520.f,TextWidth(D->SystemMessage,24)+60));
        Rect(CX-MW*.5f,228,MW,65,Panel);
        Text(D->SystemMessage,CX-TextWidth(D->SystemMessage,24)*.5f,248,24,Green);
    }
    if(Now-P->LastDamageTime<.7f && !P->bDead)
    {
        const float Alpha=1.f-(Now-P->LastDamageTime)/.7f;
        const float Angle=FMath::DegreesToRadians(P->DamageDirection.Rotation().Yaw-PC->GetControlRotation().Yaw-90.f);
        const FVector2D Dir(FMath::Cos(Angle),FMath::Sin(Angle));
        const FVector2D Side(-Dir.Y,Dir.X);
        const FVector2D Tip=FVector2D(CX,CY)+Dir*71.f;
        const FVector2D Left=FVector2D(CX,CY)+Dir*86.f+Side*13.f;
        const FVector2D Right=FVector2D(CX,CY)+Dir*86.f-Side*13.f;
        FLinearColor DC=Red;DC.A=Alpha;
        Line(Tip.X,Tip.Y,Left.X,Left.Y,DC,4);Line(Tip.X,Tip.Y,Right.X,Right.Y,DC,4);
    }
    if(!L2 && D->Phase==EOBPhase::Cell)
        Text(PC->IsUsingGamepad()?TEXT("LEFT STICK  Move     RIGHT STICK  Look     SQUARE  Interact"):TEXT("WASD  Move     MOUSE  Look     E  Interact"),CX-300,ViewH-54,20,Muted);
    if(const UOBGameInstance* GI=PC->GetOBGameInstance())
        if(!GI->SaveWarning.IsEmpty()) WrappedText(GI->SaveWarning,Margin,ViewH-222,500,18,Amber);
}

void AOBHUD::MoveFocus(int32 Direction)
{
    if(Items.Num())FocusIndex=(FocusIndex+Direction+Items.Num())%Items.Num();
}

void AOBHUD::ActivateItem(int32 Index,bool bMouse)
{
    if(!Items.IsValidIndex(Index))return;
    const FMenuItem Item=Items[Index];
    AOBPlayerController* PC=Cast<AOBPlayerController>(PlayerOwner);
    if(!PC || !PC->IsMenuOpen())return;
    if(Item.bSlider && bMouse)
    {
        float X=0,Y=0;
        if(PC->GetMousePosition(X,Y))
            PC->AdjustSetting(Item.Action,1.f,FMath::Clamp((X/UIScale-Item.SliderMinX)/Item.SliderWidth,0.f,1.f));
    }
    else PC->PerformAction(Item.Action);
}

void AOBHUD::ClickAtCursor()
{
    float X=0,Y=0;
    if(!PlayerOwner || !PlayerOwner->GetMousePosition(X,Y))return;
    const FVector2D Point(X/UIScale,Y/UIScale);
    for(int32 I=0;I<Items.Num();++I)if(Items[I].Bounds.IsInside(Point)){FocusIndex=I;ActivateItem(I,true);return;}
}

void AOBHUD::ActivateFocused(){ActivateItem(FocusIndex,false);}

void AOBHUD::AdjustFocused(float Direction)
{
    if(!Items.IsValidIndex(FocusIndex))return;
    if(AOBPlayerController* PC=Cast<AOBPlayerController>(PlayerOwner))
        if(PC->MenuScreen==TEXT("Settings"))PC->AdjustSetting(Items[FocusIndex].Action,Direction);
}

void AOBHUD::NotifyHitBoxClick(FName BoxName)
{
    for(int32 I=0;I<Items.Num();++I)if(Items[I].Action==BoxName){ActivateItem(I,true);return;}
}

void AOBHUD::NotifyHitBoxBeginCursorOver(FName BoxName)
{
    for(int32 I=0;I<Items.Num();++I)if(Items[I].Action==BoxName){FocusIndex=I;return;}
}
