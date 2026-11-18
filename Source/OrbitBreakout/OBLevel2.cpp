#include "OBLevel2.h"
#include "OBMech.h"
#include "OBTraversalRoute.h"
#include "OBMachineEnemy.h"
#include "OBPlayerCharacter.h"
#include "OBUI.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/LightComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace OBDeadSignal
{
 static UStaticMesh* Cube() { return LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")); }
 static UMaterialInstanceDynamic* Tint(UStaticMeshComponent* Mesh,FLinearColor Color,bool bGlow=false)
 {
  UMaterialInterface* Material=LoadObject<UMaterialInterface>(nullptr,bGlow?TEXT("/Game/OrbitBreakout/Materials/M_Emissive.M_Emissive"):TEXT("/Game/OrbitBreakout/Materials/M_Metal.M_Metal"));
  if(!Material)Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
  if(!Material || !Mesh)return nullptr;
  UMaterialInstanceDynamic* MID=UMaterialInstanceDynamic::Create(Material,Mesh);
  MID->SetVectorParameterValue(TEXT("Color"),Color);MID->SetVectorParameterValue(TEXT("BaseColor"),Color);
  MID->SetScalarParameterValue(TEXT("EmissiveStrength"),bGlow?2.f:0.f);Mesh->SetMaterial(0,MID);return MID;
 }
 static const TCHAR* ActionName(EOBLevel2Action A)
 {
  switch(A){
   case EOBLevel2Action::Breaker:return TEXT("ISOLATE LIVE CABLE");
   case EOBLevel2Action::Pump:return TEXT("DRAIN JUNCTION");
   case EOBLevel2Action::Coupler:return TEXT("REPLACEMENT COUPLER");
   case EOBLevel2Action::Socket:return TEXT("INSERT POWER COUPLER");
   case EOBLevel2Action::Power:return TEXT("RESTART AUXILIARY NETWORK");
   case EOBLevel2Action::StabilizerA:return TEXT("SIGNAL STABILIZER A");
   case EOBLevel2Action::StabilizerB:return TEXT("SIGNAL STABILIZER B");
   case EOBLevel2Action::Transmitter:return TEXT("EMERGENCY TRANSMITTER");
   case EOBLevel2Action::Supply:return TEXT("EMERGENCY AMMUNITION");
   case EOBLevel2Action::Medical:return TEXT("MEDICAL INJECTOR");
   case EOBLevel2Action::Log:return TEXT("FREIGHT NETWORK LOG");
  }return TEXT("CONTROL");
 }
 static bool IsFinale(int32 Id){return Id>=210 && Id<=213;}
}

AOBLevel2Interactable::AOBLevel2Interactable()
{
 Kind=EOBInteractionKind::Terminal;
 Tags.Add(TEXT("OB_LEVEL2_CONTROL"));
}
void AOBLevel2Interactable::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 Body->SetRelativeScale3D(FVector(.6f,.85f,.95f));Body->SetRelativeLocation(FVector(0,0,48));
 Glow->SetRelativeLocation(FVector(-31,0,70));Glow->SetRelativeScale3D(FVector(.025f,.55f,.12f));
 Label->SetRelativeLocation(FVector(-34,0,128));Label->SetWorldSize(13.f);
 WeaponDisplay->SetVisibility(false);WeaponDisplayRail->SetVisibility(false);
 Label->SetText(FText::FromString(OBDeadSignal::ActionName(Action)));
 OBDeadSignal::Tint(Body,FLinearColor(.11f,.17f,.19f));OBDeadSignal::Tint(Glow,FLinearColor(.2f,.65f,.7f),true);
}
FString AOBLevel2Interactable::GetPrompt() const
{
 const AOBLevel2Director* D=Cast<AOBLevel2Director>(AOBMissionDirector::Get(this));
 if(bUsed && (Action==EOBLevel2Action::Medical || Action==EOBLevel2Action::Log))return TEXT("");
 const bool bFault=D && Action==EOBLevel2Action::StabilizerA && D->IsUplinkPaused();
 if(bFault)return TEXT("Hold E — Repair overloaded stabilizer (2s)");
 if(D && D->IsActionComplete(Action))return FString::Printf(TEXT("%s — ONLINE"),OBDeadSignal::ActionName(Action));
 if(D && !D->IsActionAvailable(Action))return FString::Printf(TEXT("%s — LOCKED / follow objective"),OBDeadSignal::ActionName(Action));
 return FString::Printf(TEXT("%s — %s"),HoldSeconds>0.f?TEXT("Hold E"):TEXT("E"),OBDeadSignal::ActionName(Action));
}
void AOBLevel2Interactable::SetStatus(bool bComplete,bool bAvailable,bool bFault)
{
 const FLinearColor Color=bFault?FLinearColor(1,.45f,.025f):bComplete?FLinearColor(.1f,.85f,.35f):bAvailable?FLinearColor(.1f,.75f,.85f):FLinearColor(.38f,.12f,.07f);
 OBDeadSignal::Tint(Glow,Color,true);
 Label->SetTextRenderColor(Color.ToFColor(true));
 Label->SetText(FText::FromString(FString(OBDeadSignal::ActionName(Action))+(bFault?TEXT("\nOVERLOAD — REPAIR"):bComplete?TEXT("\nONLINE"):TEXT(""))));
}

AOBLevel2Hazard::AOBLevel2Hazard()
{
 PrimaryActorTick.bCanEverTick=true;
 Area=CreateDefaultSubobject<UBoxComponent>(TEXT("HazardArea"));RootComponent=Area;
 Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);Area->SetCanEverAffectNavigation(false);
 Warning=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisibleWarning"));Warning->SetupAttachment(Area);
 Warning->SetStaticMesh(OBDeadSignal::Cube());Warning->SetCollisionEnabled(ECollisionEnabled::NoCollision);Warning->SetCanEverAffectNavigation(false);Warning->SetCastShadow(false);
}
void AOBLevel2Hazard::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);Area->SetBoxExtent(Extent);
 Warning->SetRelativeLocation(FVector(0,0,-Extent.Z+7));Warning->SetRelativeScale3D(FVector(Extent.X*2,Extent.Y*2,4)/100.f);
 WarningMID=OBDeadSignal::Tint(Warning,HazardKind==EOBLevel2HazardKind::LiveWater?FLinearColor(.06f,.35f,.9f):FLinearColor(1,.4f,.05f),true);
}
bool AOBLevel2Hazard::IsDangerous() const
{
 const AOBLevel2Director* D=Cast<AOBLevel2Director>(AOBMissionDirector::Get(this));
 if(!D || !D->IsGameplayActive())return false;
 if(HazardKind==EOBLevel2HazardKind::LiveWater)return !D->bJunctionIsolated;
 const float T=FMath::Fmod(GetWorld()->GetTimeSeconds()+CycleOffset,4.5f);
 return T>=1.f && T<2.5f;
}
void AOBLevel2Hazard::Tick(float DT)
{
 Super::Tick(DT);
 AOBLevel2Director* D=Cast<AOBLevel2Director>(AOBMissionDirector::Get(this));
 if(!D)return;
 const float Time=GetWorld()->GetTimeSeconds();
 const bool bWater=HazardKind==EOBLevel2HazardKind::LiveWater;
 const float Cycle=FMath::Fmod(Time+CycleOffset,4.5f);
 const bool bLive=IsDangerous();
 const bool bWarn=!bWater && D->IsGameplayActive() && Cycle<1.f;
 Warning->SetVisibility(bWater?!D->bJunctionDrained:bLive||bWarn);
 if(!WarningMID)WarningMID=OBDeadSignal::Tint(Warning,FLinearColor(.1f,.4f,.8f),true);
 if(WarningMID){WarningMID->SetVectorParameterValue(TEXT("Color"),bWater?(D->bJunctionIsolated?FLinearColor(.05f,.12f,.16f):FLinearColor(.06f,.35f,.8f)):bLive?FLinearColor(1.f,.22f,.035f):FLinearColor(.7f,.4f,.03f));WarningMID->SetScalarParameterValue(TEXT("EmissiveStrength"),bLive?2.1f:.7f);}
 if(bWarn && FMath::FloorToFloat((Time+CycleOffset)/4.5f)!=LastCueCycle){LastCueCycle=FMath::FloorToFloat((Time+CycleOffset)/4.5f);D->PlaySound(TEXT("Charge"),GetActorLocation(),.35f);}
 if(!bLive || !IsValid(D->Player))return;
 const FVector Local=GetActorTransform().InverseTransformPosition(D->Player->GetActorLocation()-FVector(0,0,75));
 if(FMath::Abs(Local.X)>Extent.X || FMath::Abs(Local.Y)>Extent.Y || FMath::Abs(Local.Z)>Extent.Z+35.f)return;
 const float Interval=bWater?.25f:1.f;
 if(Time-LastDamage>=Interval){LastDamage=Time;D->Player->DamagePlayer(bWater?3.f:15.f,GetActorLocation());}
}

AOBLevel2Director::AOBLevel2Director()
{
 StartLocation=FVector(0,0,110);PrimaryActorTick.bCanEverTick=true;
}
void AOBLevel2Director::BeginPlay()
{
 // Chapter one owns private initialization timers; intentionally initialize only AActor.
 AActor::BeginPlay();
 AOBTraversalRoute::Install(*this,true);
 for(TActorIterator<AOBMachineEnemy> It(GetWorld());It;++It){Machines.Add(*It);EncounterRoster.FindOrAdd(It->EncounterId).Add(It->StableId);}
 for(TActorIterator<AOBDoor> It(GetWorld());It;++It)Doors.Add(*It);
 for(TActorIterator<AOBLevel2Interactable> It(GetWorld());It;++It)Interactions.Add(*It);
 for(TActorIterator<AActor> It(GetWorld());It;++It){
  if(It->ActorHasTag(TEXT("L2Wreck"))){Wreck=*It;WreckFinalTransform=It->GetActorTransform();}
  if(It->ActorHasTag(TEXT("L2PowerLight")))PowerActors.Add(*It);
  if(It->ActorHasTag(TEXT("L2PowerFan")))PowerFans.Add(*It);
  if(It->ActorHasTag(TEXT("L2SpawnDoor1")) || It->ActorHasTag(TEXT("L2SpawnDoor2")) || It->ActorHasTag(TEXT("L2SpawnDoor3")))SpawnDoors.Add(*It);
 }
 for(const TCHAR* Name:{TEXT("Shot"),TEXT("Reload"),TEXT("DryFire"),TEXT("Damage"),TEXT("Pickup"),TEXT("Charge"),TEXT("EnemyShot"),TEXT("Impact"),TEXT("EnemyDamage"),TEXT("EnemyDeath"),TEXT("Door"),TEXT("Terminal"),TEXT("Checkpoint"),TEXT("Ambience"),TEXT("Alarm"),TEXT("Launch")}){
  const FString Path=FString::Printf(TEXT("/Game/OrbitBreakout/Audio/S_%s.S_%s"),Name,Name);
  if(USoundBase* Sound=LoadObject<USoundBase>(nullptr,*Path))ChapterSounds.Add(FName(Name),Sound);
 }
 GetWorldTimerManager().SetTimerForNextTick(this,&AOBLevel2Director::InitializeChapter);
}
void AOBLevel2Director::InitializeChapter()
{
 Player=Cast<AOBPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 UOBGameInstance* GI=GetGameInstance<UOBGameInstance>();
 if(!Player || !GI){UE_LOG(LogTemp,Error,TEXT("OB_LEVEL2_INIT_FAILED player or game instance missing"));return;}
 GI->BeginLevel2Session();
 FString Errors;if(!ValidateContent(Errors))UE_LOG(LogTemp,Error,TEXT("OB_LEVEL2_CONTENT_INVALID %s"),*Errors);
 Checkpoint=GI->MissionSave->CheckpointId;RestoreWorld(Checkpoint);bReady=true;
 if(AOBPlayerController* PC=Cast<AOBPlayerController>(Player->GetController()))PC->HideScreen();
 RefreshWorld();
 if(ChapterSounds.Contains(TEXT("Ambience")))ChapterAmbience=UGameplayStatics::SpawnSound2D(this,ChapterSounds[TEXT("Ambience")],GI->Settings?GI->Settings->MasterVolume*GI->Settings->AmbienceVolume*.35f:.15f,1,0,nullptr,false,false);
 if(!GI->MissionSave->bLevel2IntroComplete)BeginCinematic(false);
 else if(GI->MissionSave->bLevel2Completed){MissionState=EOBLevel2State::SignalReceived;CompleteMission();}
 else ShowMessage(TEXT("Checkpoint restored — health and ammunition ready."),4.f);
 UE_LOG(LogTemp,Display,TEXT("OB_LEVEL2_READY checkpoint=%d machines=%d controls=%d development=%d"),Checkpoint,Machines.Num(),Interactions.Num(),GI->bDevelopmentChapterSession);
}
void AOBLevel2Director::RestoreWorld(int32 Id)
{
 UOBGameInstance* GI=GetGameInstance<UOBGameInstance>();if(!Player || !GI || !GI->MissionSave)return;
 CleanupCameraBridge();bCrashPlayerRevealed=false;Player->SetCrashPresentation(false);
 const UOBMissionSave* S=GI->MissionSave;
 bDeadHandled=false;bCompletionSaved=false;DeathElapsed=0;
 Checkpoint=FMath::Clamp(Id,0,3);CurrentZone=Checkpoint==0?1:Checkpoint==1?3:Checkpoint==2?5:7;
 bJunctionIsolated=S->bJunctionIsolated;bJunctionDrained=S->bJunctionDrained;bCouplerOwned=S->bCouplerOwned;bCouplerInserted=S->bCouplerInserted;bAuxPowerRestored=S->bAuxPowerRestored;bWardenDefeated=S->bWardenDefeated;
 bStabilizerA=bStabilizerB=bOverloadTriggered=bOverloadRepaired=false;UplinkSeconds=0;FinaleSpawned=0;ActiveEncounter=0;
 ScheduledEncounters.Empty();ResolvedMachines.Empty();MachineAttackSlots.Empty();SpawnQueue.Empty();SpawnNotBefore.Empty();ClearedEncounters.Empty();
 for(AOBMachineEnemy* M:Machines)if(IsValid(M)){
  M->ResetToSpawn();
  const bool bCleared=(Checkpoint>=1 && M->EncounterId<=203)||(Checkpoint>=2 && M->EncounterId<=206)||(Checkpoint>=3 && M->EncounterId==207);
  if(bCleared){M->SetResolved();ScheduledEncounters.Add(M->EncounterId);ResolvedMachines.Add(M->StableId);ClearedEncounters.AddUnique(M->EncounterId);}
 }
 const FVector Position=Checkpoint==0?StartLocation:Checkpoint==1?JunctionCheckpoint:Checkpoint==2?PowerCheckpoint:FinaleCheckpoint;
 Player->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);Player->SetActorRotation(FRotator::ZeroRotator);Player->SetActorHiddenInGame(false);Player->SetActorEnableCollision(true);
 Player->RestoreCampaignLoadout(S->bArcOwned,S->ArcMagazine,S->ReserveAmmo,S->bHelixOwned,S->HelixMagazine,S->HelixReserveAmmo,static_cast<EOBWeaponType>(S->SelectedWeapon));
 if(APlayerController* PC=Cast<APlayerController>(Player->GetController())){PC->SetControlRotation(FRotator(-8,0,0));if(PC->IsLocalController() && PC->GetLocalPlayer())PC->SetViewTarget(Player);}
 MissionState=Checkpoint==0?EOBLevel2State::ReachJunction:Checkpoint==1?EOBLevel2State::CrossFreight:Checkpoint==2?EOBLevel2State::DefeatWarden:EOBLevel2State::ActivateStabilizers;
 Phase=EOBPhase::Corridor;SafeUntil=GetWorld()->GetTimeSeconds()+4.f;RefreshWorld();
}
bool AOBLevel2Director::IsGameplayActive() const
{
 if(!bReady || bCinematic || !IsValid(Player) || Player->bDead || MissionState==EOBLevel2State::SignalReceived || Phase==EOBPhase::Complete)return false;
 const UOBGameInstance* GI=GetGameInstance<UOBGameInstance>();if(GI && GI->bFrontEnd)return false;
 const AOBPlayerController* PC=Cast<AOBPlayerController>(Player->GetController());
 return !UGameplayStatics::IsGamePaused(this) && (!PC || !PC->IsMenuOpen());
}
bool AOBLevel2Director::IsCombatAllowed() const{return IsGameplayActive() && GetWorld()->GetTimeSeconds()>=SafeUntil;}
void AOBLevel2Director::Tick(float DT)
{
 AActor::Tick(DT);if(!bReady)return;
 if(ChapterAmbience){const UOBGameInstance* GI=GetGameInstance<UOBGameInstance>();if(GI && GI->Settings)ChapterAmbience->SetVolumeMultiplier(GI->Settings->MasterVolume*GI->Settings->AmbienceVolume*.35f);}
 if(bCinematic){
  if(UGameplayStatics::IsGamePaused(this))return;
  CinematicElapsed+=DT;
  if(!bRescueCinematic)UpdateCrashPresentation();
  if(bSkipHeld && CinematicElapsed>=2.f)SkipHeldSeconds+=DT;else SkipHeldSeconds=0;
  const int32 Beat=bRescueCinematic?(CinematicElapsed>=7?1:0):CinematicElapsed>=34?4:CinematicElapsed>=26?3:CinematicElapsed>=18?2:CinematicElapsed>=5?1:0;
  if(Beat!=LastCinematicBeat){LastCinematicBeat=Beat;if(!bRescueCinematic){if(Beat==1)PlaySound(TEXT("Alarm"),StartLocation,.45f);if(Beat==2)PlaySound(TEXT("Impact"),StartLocation,1.f);if(Beat==3)PlaySound(TEXT("Impact"),StartLocation,.7f);if(Beat==4)PlaySound(TEXT("Pickup"),StartLocation,.65f);}else if(Beat==1)PlaySound(TEXT("Terminal"),Player->GetActorLocation(),.8f);}
  if(CinematicElapsed>=GetCinematicDuration() || SkipHeldSeconds>=1.f)FinishCinematic();return;
 }
 if(bDeadHandled){DeathElapsed+=DT;if(DeathElapsed>=.75f)if(AOBPlayerController* PC=Cast<AOBPlayerController>(Player?Player->GetController():nullptr))if(!PC->IsMenuOpen())PC->ShowScreen(TEXT("Dead"));return;}
 if(!IsGameplayActive())return;
 if(Player->GetActorLocation().Z<-400){Player->EndSpawnGrace();Player->DamagePlayer(Player->MaxHealth+1,Player->GetActorLocation());return;}
 if(bAuxPowerRestored)for(AActor* Fan:PowerFans)if(IsValid(Fan))Fan->AddActorLocalRotation(FRotator(0,0,DT*70.f));
 PumpSpawnQueue(DT);UpdateProgression();
 if(MissionState==EOBLevel2State::TransmitSignal && !IsUplinkPaused()){
  const float Limit=bOverloadRepaired?75.f:50.f;UplinkSeconds=FMath::Min(Limit,UplinkSeconds+DT);
  if(UplinkSeconds>=30.f)QueueEncounter(212);
  if(UplinkSeconds>=50.f && !bOverloadTriggered){bOverloadTriggered=true;CancelHold();ShowMessage(TEXT("UPLINK OVERLOAD — repair stabilizer A. Progress held at 50 seconds."),6.f);PlaySound(TEXT("Alarm"),FindAction(EOBLevel2Action::StabilizerA)?FindAction(EOBLevel2Action::StabilizerA)->GetActorLocation():Player->GetActorLocation(),.8f);RefreshWorld();}
  if(UplinkSeconds>=75.f){MissionState=EOBLevel2State::ClearFinalHostiles;ShowMessage(TEXT("Signal established — clear remaining hostiles."),5.f);RefreshWorld();}
 }
 if(MissionState==EOBLevel2State::ClearFinalHostiles && SpawnQueue.IsEmpty() && AliveFinaleMachines()==0 && IsEncounterCleared(213))BeginCinematic(true);
}

void AOBLevel2Director::ClearProjectiles(){TArray<AActor*> Bolts;UGameplayStatics::GetAllActorsWithTag(this,TEXT("OB_PROJECTILE"),Bolts);for(AActor* Bolt:Bolts)if(IsValid(Bolt))Bolt->Destroy();}
void AOBLevel2Director::BeginCinematic(bool bRescue)
{
 if(bCinematic || !Player)return;
 CleanupCameraBridge();bCrashPlayerRevealed=false;
 CancelHold();Player->CancelActions();Player->ConsumeMovementInputVector();Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->DisableMovement();
 bCinematic=true;bRescueCinematic=bRescue;bSkipHeld=false;SkipHeldSeconds=0;CinematicElapsed=0;LastCinematicBeat=-1;MachineAttackSlots.Empty();ClearProjectiles();Phase=EOBPhase::Ending;
 Player->SetCrashPresentation(!bRescue);
 if(bRescue){Player->SetActorHiddenInGame(false);MissionState=EOBLevel2State::SignalReceived;SpawnQueue.Empty();SetCarrierColor(FLinearColor(.08f,1.f,.3f));if(AOBLevel2Interactable* Console=FindAction(EOBLevel2Action::Transmitter)){Player->SetActorLocation(Console->GetActorLocation()+FVector(-165,0,65),false,nullptr,ETeleportType::TeleportPhysics);Player->SetActorRotation(FRotator::ZeroRotator);}}
 else{MissionState=EOBLevel2State::CrashIntro;Player->SetActorLocation(StartLocation,false,nullptr,ETeleportType::TeleportPhysics);Player->SetActorHiddenInGame(true);Player->SetActorEnableCollision(false);}
 if(APlayerController* PC=Cast<APlayerController>(Player->GetController()))PC->FlushPressedKeys();
 ULevelSequence* Sequence=bRescue?RescueSequence.Get():CrashSequence.Get();
 if(Sequence){FMovieSceneSequencePlaybackSettings Settings;Settings.bAutoPlay=false;Settings.bPauseAtEnd=true;Settings.bDisableMovementInput=false;Settings.bDisableLookAtInput=false;Settings.bHidePlayer=false;
  ALevelSequenceActor* Created=nullptr;SequencePlayer=ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(),Sequence,Settings,Created);SequenceActor=Created;
  if(SequencePlayer)SequencePlayer->Play();
 }else UE_LOG(LogTemp,Error,TEXT("OB_LEVEL2_SEQUENCE_MISSING rescue=%d"),bRescue);
 UE_LOG(LogTemp,Display,TEXT("OB_LEVEL2_CINEMATIC_START rescue=%d"),bRescue);
}
void AOBLevel2Director::SetSkipHeld(bool bHeld){bSkipHeld=bHeld && bCinematic;if(!bSkipHeld)SkipHeldSeconds=0;}
void AOBLevel2Director::GroundCrashPlayer()
{
 if(!IsValid(Player))return;
 FVector Position=StartLocation;
 FHitResult Floor;FCollisionQueryParams Query(SCENE_QUERY_STAT(OBCrashRecoveryFloor),false,Player);
 if(GetWorld()->LineTraceSingleByObjectType(Floor,StartLocation+FVector(0,0,200),StartLocation-FVector(0,0,500),FCollisionObjectQueryParams(ECC_WorldStatic),Query) && Floor.ImpactNormal.Z>.65f)
  Position=Floor.ImpactPoint+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f);
 Player->GetCharacterMovement()->StopMovementImmediately();Player->ConsumeMovementInputVector();
 Player->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);Player->SetActorRotation(FRotator::ZeroRotator);
}
void AOBLevel2Director::UpdateCrashPresentation()
{
 if(!IsValid(Player) || bRescueCinematic)return;
 constexpr float RecoveryStart=34.f;
 if(CinematicElapsed>=RecoveryStart && !bCrashPlayerRevealed){
  GroundCrashPlayer();bCrashPlayerRevealed=true;Player->SetActorEnableCollision(true);Player->SetActorHiddenInGame(false);
 }
 Player->SetCrashPresentation(true,FMath::Max(0.f,CinematicElapsed-RecoveryStart));
}
void AOBLevel2Director::CleanupCameraBridge()
{
 GetWorldTimerManager().ClearTimer(CameraBridgeTimer);
 if(IsValid(CameraBridge)){
  if(IsValid(Player))if(APlayerController* PC=Cast<APlayerController>(Player->GetController()))
   if(PC->IsLocalController() && PC->GetLocalPlayer() && PC->GetViewTarget()==CameraBridge)PC->SetViewTarget(Player);
  CameraBridge->Destroy();
 }
 CameraBridge=nullptr;
}
void AOBLevel2Director::StopCinematic(bool bBlendToPlayer)
{
 const bool bWasCinematic=bCinematic || SequencePlayer!=nullptr || SequenceActor!=nullptr;
 APlayerController* PC=IsValid(Player)?Cast<APlayerController>(Player->GetController()):nullptr;
 const bool bLocalView=PC && PC->IsLocalController() && PC->GetLocalPlayer();
 FMinimalViewInfo Outgoing;bool bHaveOutgoing=false;
 // Sequence Stop restores camera tracks. Capture the actual outgoing view first.
 if(bBlendToPlayer && bWasCinematic && bLocalView){
  if(SequencePlayer && SequencePlayer->GetActiveCameraComponent()){SequencePlayer->GetActiveCameraComponent()->GetCameraView(0.f,Outgoing);bHaveOutgoing=true;}
  else if(PC->PlayerCameraManager){Outgoing=PC->PlayerCameraManager->GetCameraCacheView();bHaveOutgoing=true;}
 }
 CleanupCameraBridge();
 if(SequencePlayer){SequencePlayer->Stop();SequencePlayer=nullptr;}
 if(IsValid(SequenceActor))SequenceActor->Destroy();SequenceActor=nullptr;
 bCinematic=false;bSkipHeld=false;SkipHeldSeconds=0;
 if(IsValid(Wreck))Wreck->SetActorTransform(WreckFinalTransform,false,nullptr,ETeleportType::TeleportPhysics);
 if(IsValid(Player))Player->SetCrashPresentation(false);
 if(bWasCinematic && IsValid(Player)){Player->CancelActions();Player->SetActorHiddenInGame(false);Player->SetActorEnableCollision(true);Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  if(PC){PC->ResetIgnoreMoveInput();PC->ResetIgnoreLookInput();PC->FlushPressedKeys();
   if(bLocalView){
    // Refresh the spring arm after grounding/facing changes, before choosing the
    // blend target. A captured bridge cannot snap back to a restored cut track.
    Player->CameraBoom->TickComponent(0.f,LEVELTICK_All,nullptr);
    FMinimalViewInfo Gameplay;Player->FollowCamera->GetCameraView(0.f,Gameplay);
    const bool bAligned=bHaveOutgoing && Outgoing.Location.Equals(Gameplay.Location,12.f) && Outgoing.Rotation.Equals(Gameplay.Rotation,2.f) && FMath::Abs(Outgoing.FOV-Gameplay.FOV)<1.f;
    if(bHaveOutgoing && FVector::DistSquared(Outgoing.Location,Gameplay.Location)>FMath::Square(2000.f)){
     // An early skip can leave the camera in the exterior flight set. Mask the
     // cut instead of sweeping kilometres through the level in a short blend.
     PC->SetViewTarget(Player);
     if(PC->PlayerCameraManager)PC->PlayerCameraManager->StartCameraFade(1.f,0.f,.25f,FLinearColor::Black,false,false);
    }else if(bHaveOutgoing && !bAligned){
     FActorSpawnParameters Spawn;Spawn.ObjectFlags|=RF_Transient;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
     CameraBridge=GetWorld()->SpawnActor<ACameraActor>(Outgoing.Location,Outgoing.Rotation,Spawn);
     if(CameraBridge){
      UCameraComponent* Camera=CameraBridge->GetCameraComponent();Camera->SetFieldOfView(Outgoing.FOV);Camera->SetAspectRatio(Outgoing.AspectRatio);Camera->bConstrainAspectRatio=Outgoing.bConstrainAspectRatio;
      Camera->PostProcessSettings=Outgoing.PostProcessSettings;Camera->PostProcessBlendWeight=Outgoing.PostProcessBlendWeight;
      PC->SetViewTarget(CameraBridge);PC->SetViewTargetWithBlend(Player,.4f,VTBlend_Cubic,0.f,true);
      GetWorldTimerManager().SetTimer(CameraBridgeTimer,this,&AOBLevel2Director::CleanupCameraBridge,.6f,false);
     }else PC->SetViewTarget(Player);
    }else PC->SetViewTarget(Player);
   }
  }
 }
}
void AOBLevel2Director::FinishCinematic()
{
 if(!bCinematic)return;const bool bRescue=bRescueCinematic;
 if(!bRescue){GroundCrashPlayer();if(APlayerController* PC=Cast<APlayerController>(Player->GetController()))PC->SetControlRotation(FRotator(-8,0,0));}
 StopCinematic(!bRescue);
 if(bRescue){CompleteMission();return;}
 MissionState=EOBLevel2State::ReachJunction;Phase=EOBPhase::Corridor;
 Player->Health=Player->MaxHealth;
 SafeUntil=GetWorld()->GetTimeSeconds()+6.f;CommitCheckpoint(0);RefreshWorld();
 ShowMessage(TEXT("Emergency injector applied. Find the signal generator. Supplies are beside the wreck."),7.f);
 UE_LOG(LogTemp,Display,TEXT("OB_LEVEL2_CRASH_FINALIZED checkpoint=0"));
}
FString AOBLevel2Director::GetSubtitle() const
{
 if(bRescueCinematic){if(CinematicElapsed<6.5f)return TEXT("ROOK: Emergency channel. My ship is down. I'm beneath the Aurelia freight tunnels. Does anyone read?");if(CinematicElapsed<12.f)return TEXT("RESCUE: Rook, we read you. Your signal is weak. Hold your position. We're coming.");return TEXT("ROOK: Copy. I'll be here.");}
 if(CinematicElapsed<5.f)return TEXT("STAR SPARROW // ESCAPE VECTOR UNSTABLE");
 if(CinematicElapsed<11.f)return TEXT("ROOK: Thrusters aren't responding.");
 if(CinematicElapsed<18.f)return TEXT("AURELIA // DECOMMISSIONED FREIGHT PORTAL");
 if(CinematicElapsed<26.f)return TEXT("IMPACT IMMINENT");
 if(CinematicElapsed<34.f)return TEXT("EMERGENCY MEDICAL INJECTOR // VITALS STABILIZED");
 return TEXT("ROOK: No signal. There has to be a relay down here.");
}

bool AOBLevel2Director::TryAcquireAttackSlot(AActor* Enemy)
{
 if(!IsValid(Enemy) || !IsCombatAllowed())return false;
 for(auto It=MachineAttackSlots.CreateIterator();It;++It)if(!It->IsValid())It.RemoveCurrent();
 if(MachineAttackSlots.Contains(Enemy))return true;if(MachineAttackSlots.Num()>=4)return false;
 MachineAttackSlots.Add(Enemy);return true;
}
void AOBLevel2Director::ReleaseAttackSlot(AActor* Enemy){MachineAttackSlots.Remove(Enemy);}
int32 AOBLevel2Director::CountEncounter(int32 Id) const{int32 Count=0;for(const AOBMachineEnemy* M:Machines)if(IsValid(M) && M->EncounterId==Id && !M->IsDead())++Count;return Count;}
bool AOBLevel2Director::IsEncounterCleared(int32 Id) const
{
 const TSet<FName>* Roster=EncounterRoster.Find(Id);if(!Roster || Roster->IsEmpty() || !ScheduledEncounters.Contains(Id))return false;
 for(FName Name:*Roster)if(!ResolvedMachines.Contains(Name))return false;return true;
}
int32 AOBLevel2Director::AliveFinaleMachines() const{int32 Count=0;for(const AOBMachineEnemy* M:Machines)if(IsValid(M) && OBDeadSignal::IsFinale(M->EncounterId) && M->bActive && !M->IsDead())++Count;return Count;}
void AOBLevel2Director::QueueEncounter(int32 Id)
{
 if(ScheduledEncounters.Contains(Id) || bCinematic || bDeadHandled || MissionState==EOBLevel2State::SignalReceived || MissionState==EOBLevel2State::ClearFinalHostiles)return;
 const TSet<FName>* Roster=EncounterRoster.Find(Id);if(!Roster || Roster->IsEmpty()){UE_LOG(LogTemp,Error,TEXT("OB_LEVEL2_MISSING_ROSTER %d"),Id);return;}
 ScheduledEncounters.Add(Id);ActiveEncounter=Id;
 for(AOBMachineEnemy* M:Machines)if(IsValid(M) && M->EncounterId==Id && !M->IsDead()){
  SpawnQueue.AddUnique(M);SpawnNotBefore.Add(M,GetWorld()->GetTimeSeconds()+1.2f);PlaySound(TEXT("Door"),M->GetActorLocation(),.65f);
 }
 UE_LOG(LogTemp,Display,TEXT("OB_LEVEL2_ENCOUNTER_QUEUED id=%d count=%d"),Id,Roster->Num());
}
bool AOBLevel2Director::CanSpawnAt(AOBMachineEnemy* M) const
{
 if(!M || !Player)return false;
 if(M->EncounterId!=205 && !OBDeadSignal::IsFinale(M->EncounterId))return true;
 const FVector Location=M->GetActorLocation();if(FVector::DistSquared(Location,Player->GetActorLocation())<FMath::Square(700.f))return false;
 for(const AOBMachineEnemy* Other:Machines)if(IsValid(Other) && Other!=M && Other->bActive && !Other->IsDead() && FVector::DistSquared(Other->GetActorLocation(),Location)<FMath::Square(160.f))return false;
 const APlayerController* PC=Cast<APlayerController>(Player->GetController());if(!PC)return true;
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 if(FVector::DotProduct((Location-Eye).GetSafeNormal(),View.Vector())<.5f)return true;
 FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(OBLevel2SpawnVisibility),false,Player);Query.AddIgnoredActor(M);
 return GetWorld()->LineTraceSingleByChannel(Hit,Eye,Location+FVector(0,0,45),ECC_Visibility,Query);
}
void AOBLevel2Director::PumpSpawnQueue(float DT)
{
 if(!IsCombatAllowed())return;SpawnAccumulator+=DT;if(SpawnAccumulator<.3f)return;SpawnAccumulator=0;
 for(int32 I=0;I<SpawnQueue.Num();++I){
  AOBMachineEnemy* M=SpawnQueue[I];if(!IsValid(M) || M->IsDead()){SpawnQueue.RemoveAt(I--);continue;}
  if(GetWorld()->GetTimeSeconds()<SpawnNotBefore.FindRef(M))continue;
  if(OBDeadSignal::IsFinale(M->EncounterId) && AliveFinaleMachines()>=5)continue;
  if(!CanSpawnAt(M))continue;
  M->Activate();SpawnNotBefore.Remove(M);SpawnQueue.RemoveAt(I);
  if(OBDeadSignal::IsFinale(M->EncounterId))++FinaleSpawned;
  UE_LOG(LogTemp,Display,TEXT("OB_LEVEL2_MACHINE_ACTIVATED %s encounter=%d finaleTotal=%d"),*M->StableId.ToString(),M->EncounterId,FinaleSpawned);break;
 }
}
void AOBLevel2Director::ResolveEnemy(int32 Id,FName StableId)
{
 const TSet<FName>* Roster=EncounterRoster.Find(Id);
 if(!Roster || !Roster->Contains(StableId) || !ScheduledEncounters.Contains(Id) || ResolvedMachines.Contains(StableId))return;
 ResolvedMachines.Add(StableId);
 if(IsEncounterCleared(Id)){ClearedEncounters.AddUnique(Id);RefreshWorld();UE_LOG(LogTemp,Display,TEXT("OB_LEVEL2_ENCOUNTER_CLEARED %d"),Id);}
 UpdateProgression();
}
void AOBLevel2Director::EnterZone(int32 Zone,bool bCheckpoint)
{
 if(!IsGameplayActive() || Zone<1 || Zone>7 || Zone<=CurrentZone)return;
 if(Zone>=4 && !bJunctionDrained)return;
 if(Zone>=5 && (!IsEncounterCleared(204) || !IsEncounterCleared(205)))return;
 if(Zone>=6 && !bAuxPowerRestored)return;
 if(Zone>=7 && !bWardenDefeated)return;
 CurrentZone=FMath::Max(CurrentZone,Zone);
 if(Zone>=2 && !bJunctionDrained)QueueEncounter(201);
 if(Zone>=3 && !bJunctionDrained){MissionState=EOBLevel2State::RestoreJunction;QueueEncounter(202);QueueEncounter(203);}
 if(Zone>=4 && !bAuxPowerRestored){MissionState=EOBLevel2State::CrossFreight;QueueEncounter(204);}
 if(Zone>=5 && !bAuxPowerRestored){MissionState=EOBLevel2State::RestoreAuxPower;QueueEncounter(206);}
 if(Zone==7 && !IsUplinkActive()){MissionState=EOBLevel2State::ActivateStabilizers;}
 RefreshWorld();
}
void AOBLevel2Director::UpdateProgression()
{
 if(!bReady || bCinematic || bDeadHandled)return;
 if(IsEncounterCleared(201) && !ScheduledEncounters.Contains(202) && !bJunctionDrained)QueueEncounter(202);
 if(ScheduledEncounters.Contains(204) && !ScheduledEncounters.Contains(205)){
  int32 Deaths=0;for(FName Name:EncounterRoster.FindChecked(204))if(ResolvedMachines.Contains(Name))++Deaths;
  if(Deaths>=2){QueueEncounter(205);ShowMessage(TEXT("Armored security trooper inbound. Flank its shield."),5.f);}
 }
 if(IsEncounterCleared(204) && IsEncounterCleared(205) && !bAuxPowerRestored && MissionState==EOBLevel2State::CrossFreight){MissionState=EOBLevel2State::RestoreAuxPower;ShowMessage(TEXT("Freight gallery clear. Restore the auxiliary network."),4.f);RefreshWorld();}
 if(CurrentZone>=6 && bAuxPowerRestored && !bWardenDefeated && !ScheduledEncounters.Contains(207))
  for(AOBMachineEnemy* M:Machines)if(IsValid(M) && M->EncounterId==207 && FVector::DistSquared(Player->GetActorLocation(),M->GetActorLocation())<FMath::Square(2100.f)){QueueEncounter(207);break;}
 if(IsEncounterCleared(207) && !bWardenDefeated){bWardenDefeated=true;MissionState=EOBLevel2State::ActivateStabilizers;CommitCheckpoint(3);ShowMessage(TEXT("Warden access key acquired. Activate both signal stabilizers."),6.f);RefreshWorld();}
 if(GetRemainingEnemies()==0)ActiveEncounter=0;
}
int32 AOBLevel2Director::GetRemainingEnemies() const
{
 int32 Count=0;for(const AOBMachineEnemy* M:Machines)if(IsValid(M) && ScheduledEncounters.Contains(M->EncounterId) && !M->IsDead())++Count;return Count;
}

bool AOBLevel2Director::IsActionComplete(EOBLevel2Action A) const
{
 switch(A){
  case EOBLevel2Action::Breaker:return bJunctionIsolated;
  case EOBLevel2Action::Pump:return bJunctionDrained;
  case EOBLevel2Action::Coupler:return bCouplerOwned;
  case EOBLevel2Action::Socket:return bCouplerInserted;
  case EOBLevel2Action::Power:return bAuxPowerRestored;
  case EOBLevel2Action::StabilizerA:return bStabilizerA && !IsUplinkPaused();
  case EOBLevel2Action::StabilizerB:return bStabilizerB;
  case EOBLevel2Action::Transmitter:return IsUplinkActive() || MissionState==EOBLevel2State::SignalReceived;
  default:return false;
 }
}
bool AOBLevel2Director::IsActionAvailable(EOBLevel2Action A) const
{
 if(!IsGameplayActive())return false;
 switch(A){
  case EOBLevel2Action::Breaker:return CurrentZone>=3 && !bJunctionIsolated && IsEncounterCleared(201) && IsEncounterCleared(202) && IsEncounterCleared(203);
  case EOBLevel2Action::Pump:return bJunctionIsolated && !bJunctionDrained;
  case EOBLevel2Action::Coupler:return CurrentZone>=5 && !bCouplerOwned && IsEncounterCleared(206);
  case EOBLevel2Action::Socket:return bCouplerOwned && !bCouplerInserted;
  case EOBLevel2Action::Power:return bCouplerInserted && !bAuxPowerRestored && IsEncounterCleared(206);
  case EOBLevel2Action::StabilizerA:return bWardenDefeated && (IsUplinkPaused() || (MissionState==EOBLevel2State::ActivateStabilizers && !bStabilizerA));
  case EOBLevel2Action::StabilizerB:return bWardenDefeated && MissionState==EOBLevel2State::ActivateStabilizers && !bStabilizerB;
  case EOBLevel2Action::Transmitter:return MissionState==EOBLevel2State::ActivateStabilizers && bStabilizerA && bStabilizerB && IsEncounterCleared(210);
  case EOBLevel2Action::Supply:return Player && Player->bWeaponOwned;
  case EOBLevel2Action::Medical:return Player && Player->Health<Player->MaxHealth;
  case EOBLevel2Action::Log:return true;
 }
 return false;
}
bool AOBLevel2Director::CanReach(AOBPlayerCharacter* P,AOBInteractable* I) const
{
 if(!IsValid(P) || !IsValid(I) || !P->GetController() || !IsGameplayActive() || P->bReloading || P->GetCharacterMovement()->IsFalling())return false;
 const FVector Target=I->GetActorLocation()+FVector(0,0,65);
 if(FVector::DistSquared(P->GetActorLocation(),Target)>FMath::Square(250.f))return false;
 FVector Eye;FRotator View;P->GetController()->GetPlayerViewPoint(Eye,View);
 if(FVector::DotProduct((Target-Eye).GetSafeNormal(),View.Vector())<.25f)return false;
 FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(OBLevel2Control),false,P);Query.AddIgnoredActor(I);
 return !GetWorld()->LineTraceSingleByChannel(Hit,P->GetActorLocation()+FVector(0,0,35),Target,ECC_Visibility,Query);
}
AOBInteractable* AOBLevel2Director::FindInteraction(AOBPlayerCharacter* P) const
{
 AOBInteractable* Best=nullptr;float Closest=MAX_flt;
 for(AOBInteractable* I:Interactions)if(IsValid(I) && !I->bUsed && CanReach(P,I)){
  const float Dist=FVector::DistSquared(P->GetActorLocation(),I->GetActorLocation());if(Dist<Closest){Best=I;Closest=Dist;}
 }
 return Best;
}
bool AOBLevel2Director::TryInteract(AOBPlayerCharacter* P,AOBInteractable* I)
{
 AOBLevel2Interactable* Control=Cast<AOBLevel2Interactable>(I);
 if(!Control || Control->bUsed || !CanReach(P,I))return false;
 if(!IsActionAvailable(Control->Action)){
  if(Control->Action==EOBLevel2Action::Medical && P->Health>=P->MaxHealth)ShowMessage(TEXT("Health is full. Keep the injector for later."),3.f);
  else ShowMessage(GetObjectiveText(),3.f);return false;
 }
 if(GetWorld()->GetTimeSeconds()<Control->CooldownUntil)return false;
 CancelHold();
 const float Duration=Control->Action==EOBLevel2Action::StabilizerA && IsUplinkPaused()?2.f:Control->HoldSeconds;
 if(Duration<=0.f){CompleteAction(Control);return true;}
 HoldTarget=Control;HoldElapsed=0;HoldDamageTime=P->LastDamageTime;P->InteractionProgress=0;P->bInteractionActive=true;
 P->GetCharacterMovement()->StopMovementImmediately();return true;
}
void AOBLevel2Director::UpdateHold(AOBPlayerCharacter* P,AOBInteractable* Target,float DT)
{
 if(!HoldTarget)return;
 AOBLevel2Interactable* Control=Cast<AOBLevel2Interactable>(HoldTarget);
 if(!Control || !P || Target!=HoldTarget || !P->bInteractHeld || !CanReach(P,Control) || !IsActionAvailable(Control->Action) || P->LastDamageTime>HoldDamageTime){CancelHold();return;}
 const float Duration=Control->Action==EOBLevel2Action::StabilizerA && IsUplinkPaused()?2.f:FMath::Max(.05f,Control->HoldSeconds);
 HoldElapsed+=FMath::Max(0.f,DT);P->InteractionProgress=FMath::Clamp(HoldElapsed/Duration,0.f,1.f);
 if(HoldElapsed>=Duration){CancelHold();CompleteAction(Control);}
}
void AOBLevel2Director::CancelHold()
{
 HoldTarget=nullptr;HoldElapsed=0;
 if(Player){Player->InteractionProgress=0;Player->bInteractionActive=false;}
}
void AOBLevel2Director::ReplenishAmmo(int32 ArcReserve,int32 HelixReserve)
{
 if(!Player || !Player->bWeaponOwned)return;
 const EOBWeaponType Selected=Player->EquippedWeapon;
 Player->EquipWeapon(EOBWeaponType::Arc9);Player->AddAmmo(FMath::Max(0,ArcReserve-Player->ReserveAmmo));
 if(Player->bHelixOwned){Player->EquipWeapon(EOBWeaponType::Helix12);Player->AddAmmo(FMath::Max(0,HelixReserve-Player->ReserveAmmo));}
 Player->EquipWeapon(Selected);
}
void AOBLevel2Director::CompleteAction(AOBLevel2Interactable* Control)
{
 if(!IsValid(Control) || !IsActionAvailable(Control->Action))return;
 switch(Control->Action){
  case EOBLevel2Action::Breaker:bJunctionIsolated=true;ShowMessage(TEXT("Live cable isolated. Drain the junction at the pump control."),5.f);break;
  case EOBLevel2Action::Pump:bJunctionDrained=true;MissionState=EOBLevel2State::CrossFreight;CommitCheckpoint(1);ShowMessage(TEXT("Drainage restored. Freight access open."),4.f);break;
  case EOBLevel2Action::Coupler:bCouplerOwned=true;Control->bUsed=true;ShowMessage(TEXT("Power coupler secured. Insert it into the auxiliary socket."),4.f);break;
  case EOBLevel2Action::Socket:bCouplerInserted=true;ShowMessage(TEXT("Coupler seated. Hold the auxiliary restart control."),4.f);break;
  case EOBLevel2Action::Power:bAuxPowerRestored=true;MissionState=EOBLevel2State::DefeatWarden;CommitCheckpoint(2);ShowMessage(TEXT("Auxiliary network online. Surface antenna link found. Reach the relay."),6.f);break;
  case EOBLevel2Action::StabilizerA:
   if(IsUplinkPaused()){bOverloadRepaired=true;QueueEncounter(213);ShowMessage(TEXT("Stabilizer repaired. Transmission resuming from 50 seconds."),5.f);}
   else{bStabilizerA=true;QueueEncounter(210);ShowMessage(bStabilizerB?TEXT("Both stabilizers online. Clear security, then use the transmitter."):TEXT("Stabilizer A online. Activate B and clear incoming security."),5.f);}break;
  case EOBLevel2Action::StabilizerB:bStabilizerB=true;QueueEncounter(210);ShowMessage(bStabilizerA?TEXT("Both stabilizers online. Clear security, then use the transmitter."):TEXT("Stabilizer B online. Activate A and clear incoming security."),5.f);break;
  case EOBLevel2Action::Transmitter:MissionState=EOBLevel2State::TransmitSignal;UplinkSeconds=0;QueueEncounter(211);ShowMessage(TEXT("Distress uplink started. Stay mobile and defend the signal."),5.f);break;
  case EOBLevel2Action::Supply:ReplenishAmmo(96,108);Control->CooldownUntil=GetWorld()->GetTimeSeconds()+5.f;ShowMessage(TEXT("Reserve cells replenished. Reload when ready."),3.f);break;
  case EOBLevel2Action::Medical:if(Player->AddHealth(45.f)){Control->bUsed=true;Control->SetActorHiddenInGame(true);Control->SetActorEnableCollision(false);ShowMessage(TEXT("Medical injector applied: +45 health."),3.f);}break;
  case EOBLevel2Action::Log:Control->bUsed=true;ShowMessage(TEXT("MAINTENANCE LOG: Sector 07 antenna cable still reaches the surface. Auxiliary power required."),9.f);break;
 }
 PlaySound(Control->Action==EOBLevel2Action::Medical || Control->Action==EOBLevel2Action::Supply?TEXT("Pickup"):TEXT("Terminal"),Control->GetActorLocation(),.7f);
 RefreshWorld();UE_LOG(LogTemp,Display,TEXT("OB_LEVEL2_ACTION action=%d state=%d"),static_cast<int32>(Control->Action),static_cast<int32>(MissionState));
}
AOBLevel2Interactable* AOBLevel2Director::FindAction(EOBLevel2Action Action) const
{
 for(AOBInteractable* I:Interactions)if(AOBLevel2Interactable* C=Cast<AOBLevel2Interactable>(I))if(C->Action==Action)return C;return nullptr;
}
FString AOBLevel2Director::GetObjectiveText() const
{
 switch(MissionState){
  case EOBLevel2State::CrashIntro:return TEXT("Survive the crash.");
  case EOBLevel2State::ReachJunction:return TEXT("Find the emergency signal generator. Follow Sector 07 signs.");
  case EOBLevel2State::RestoreJunction:
   if(!IsEncounterCleared(201) || !IsEncounterCleared(202) || !IsEncounterCleared(203))return TEXT("Clear the junction's security troopers. Stay on the dry walkways.");
   return bJunctionIsolated?TEXT("Drain the junction and open the service gate."):TEXT("Isolate the damaged cable at the breaker.");
  case EOBLevel2State::CrossFreight:return TEXT("Clear the freight gallery. Flank the armored unit.");
  case EOBLevel2State::RestoreAuxPower:
   if(!IsEncounterCleared(206))return TEXT("Reach auxiliary power and clear its security troopers.");
   return !bCouplerOwned?TEXT("Recover the replacement power coupler."):!bCouplerInserted?TEXT("Insert the coupler into the auxiliary socket."):TEXT("Hold the restart control to restore auxiliary power.");
  case EOBLevel2State::DefeatWarden:return TEXT("Reach the relay. Avoid pressure jets and defeat the Relay Warden.");
  case EOBLevel2State::ActivateStabilizers:
   if(!bStabilizerA || !bStabilizerB)return TEXT("Activate both signal stabilizers. Either order works.");
   return IsEncounterCleared(210)?TEXT("Use the transmitter console to begin the distress uplink."):TEXT("Clear incoming security before starting the transmitter.");
  case EOBLevel2State::TransmitSignal:return IsUplinkPaused()?TEXT("Repair overloaded stabilizer A. Uplink held at 50 seconds."):TEXT("Defend the distress uplink. Keep moving and use cover.");
  case EOBLevel2State::ClearFinalHostiles:return TEXT("Signal established — clear remaining hostiles.");
  case EOBLevel2State::SignalReceived:return TEXT("Signal received. Rescue is responding.");
 }return TEXT("Reach the emergency transmitter.");
}
FVector AOBLevel2Director::GetObjectiveLocation() const
{
 EOBLevel2Action Action=EOBLevel2Action::Log;
 switch(MissionState){
  case EOBLevel2State::ReachJunction:return JunctionCheckpoint;
  case EOBLevel2State::RestoreJunction:Action=bJunctionIsolated?EOBLevel2Action::Pump:EOBLevel2Action::Breaker;break;
  case EOBLevel2State::CrossFreight:return PowerCheckpoint;
  case EOBLevel2State::RestoreAuxPower:Action=!bCouplerOwned?EOBLevel2Action::Coupler:!bCouplerInserted?EOBLevel2Action::Socket:EOBLevel2Action::Power;break;
  case EOBLevel2State::DefeatWarden:for(const AOBMachineEnemy* M:Machines)if(IsValid(M) && M->EncounterId==207 && !M->IsDead())return M->GetActorLocation();return FinaleCheckpoint;
  case EOBLevel2State::ActivateStabilizers:Action=!bStabilizerA?EOBLevel2Action::StabilizerA:!bStabilizerB?EOBLevel2Action::StabilizerB:EOBLevel2Action::Transmitter;break;
  case EOBLevel2State::TransmitSignal:Action=IsUplinkPaused()?EOBLevel2Action::StabilizerA:EOBLevel2Action::Transmitter;break;
  case EOBLevel2State::ClearFinalHostiles:for(const AOBMachineEnemy* M:Machines)if(IsValid(M) && M->bActive && !M->IsDead())return M->GetActorLocation();return FVector::ZeroVector;
  default:return FVector::ZeroVector;
 }
 if(const AOBLevel2Interactable* Control=FindAction(Action))return Control->GetActorLocation();return FVector::ZeroVector;
}
void AOBLevel2Director::RefreshWorld()
{
 for(AOBDoor* Door:Doors)if(IsValid(Door)){
  const bool bOpen=(Door->DoorId==203 && bJunctionDrained)||(Door->DoorId==204 && IsEncounterCleared(204) && IsEncounterCleared(205))||(Door->DoorId==206 && bAuxPowerRestored)||(Door->DoorId==207 && bWardenDefeated);
  if(bOpen)Door->Open(!bReady);
 }
 for(AOBInteractable* I:Interactions)if(AOBLevel2Interactable* C=Cast<AOBLevel2Interactable>(I)){
  const bool bFault=C->Action==EOBLevel2Action::StabilizerA && IsUplinkPaused();C->SetStatus(IsActionComplete(C->Action),IsActionAvailable(C->Action),bFault);
  if((C->Action==EOBLevel2Action::Medical || C->Action==EOBLevel2Action::Log) && C->Zone<(Checkpoint==3?7:Checkpoint==2?5:Checkpoint==1?3:1))C->bUsed=true;
  if(C->Action==EOBLevel2Action::Coupler && bCouplerOwned)C->bUsed=true;
  if(C->bUsed && C->Action==EOBLevel2Action::Medical){C->SetActorHiddenInGame(true);C->SetActorEnableCollision(false);}
 }
 for(AActor* Actor:PowerActors)if(IsValid(Actor)){
  TInlineComponentArray<ULightComponent*> Lights;Actor->GetComponents(Lights);for(ULightComponent* Light:Lights)Light->SetVisibility(bAuxPowerRestored);
 }
 for(TActorIterator<AActor> It(GetWorld());It;++It){
  if(It->ActorHasTag(TEXT("L2DrainWater")) || It->ActorHasTag(TEXT("L2FloodWater")))It->SetActorHiddenInGame(bJunctionDrained);
  if(It->ActorHasTag(TEXT("L2LiveCableLight")) || It->ActorHasTag(TEXT("L2ElectricalWarning"))){TInlineComponentArray<ULightComponent*> Lights;It->GetComponents(Lights);for(ULightComponent* Light:Lights)Light->SetVisibility(!bJunctionIsolated);}
  if(It->ActorHasTag(TEXT("L2PowerLabel"))){TInlineComponentArray<UTextRenderComponent*> Labels;It->GetComponents(Labels);for(UTextRenderComponent* Text:Labels){Text->SetText(FText::FromString(bAuxPowerRestored?TEXT("AUXILIARY NETWORK\nONLINE // ANTENNA CONNECTED"):TEXT("AUXILIARY NETWORK\nCOUPLER REQUIRED")));Text->SetTextRenderColor(bAuxPowerRestored?FColor(90,235,170):FColor(255,178,75));}}
 }
 if(IsValid(Wreck) && !bCinematic)Wreck->SetActorTransform(WreckFinalTransform,false,nullptr,ETeleportType::TeleportPhysics);
 if(bAuxPowerRestored)SetCarrierColor(MissionState==EOBLevel2State::SignalReceived?FLinearColor(.08f,1.f,.3f):IsUplinkPaused()?FLinearColor(1,.42f,.03f):FLinearColor(.1f,.6f,.75f));
}
void AOBLevel2Director::SetCarrierColor(FLinearColor Color)
{
 for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->ActorHasTag(TEXT("L2Generator"))){
  TInlineComponentArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);for(UStaticMeshComponent* Mesh:Meshes)OBDeadSignal::Tint(Mesh,Color,false);
 }
}
void AOBLevel2Director::SnapshotLoadout()
{
 UOBGameInstance* GI=GetGameInstance<UOBGameInstance>();if(!GI || !GI->MissionSave || !Player)return;
 UOBMissionSave* S=GI->MissionSave;S->bArcOwned=Player->bWeaponOwned;S->bHelixOwned=Player->bHelixOwned;S->ArcMagazine=Player->GetArcMagazineAmmo();S->ReserveAmmo=Player->GetArcReserveAmmo();S->HelixMagazine=Player->GetHelixMagazineAmmo();S->HelixReserveAmmo=Player->GetHelixReserveAmmo();S->SelectedWeapon=static_cast<uint8>(Player->EquippedWeapon);
}
void AOBLevel2Director::WriteSaveFlags()
{
 UOBGameInstance* GI=GetGameInstance<UOBGameInstance>();if(!GI || !GI->MissionSave)return;
 UOBMissionSave* S=GI->MissionSave;S->SchemaVersion=2;S->ChapterId=2;S->MissionId=TEXT("DeadSignal");S->SetCheckpointIdentity(Checkpoint);S->bPendingChapter2=false;S->bLevel2IntroComplete=true;
 S->bJunctionIsolated=bJunctionIsolated;S->bJunctionDrained=bJunctionDrained;S->bCouplerOwned=bCouplerOwned;S->bCouplerInserted=bCouplerInserted;S->bAuxPowerRestored=bAuxPowerRestored;S->bWardenDefeated=bWardenDefeated;
 // A checkpoint in the arena always restarts its complete finite defense.
 S->bStabilizerA=false;S->bStabilizerB=false;S->bMissionCompleted=false;S->bLevel2Completed=false;
}
void AOBLevel2Director::CommitCheckpoint(int32 Id)
{
 if(!Player || Player->bDead || bCinematic || Id<Checkpoint || Id>3 || Id<0)return;
 if((Id==0 && MissionState!=EOBLevel2State::ReachJunction)||(Id==1 && (!bJunctionDrained || !IsEncounterCleared(203)))||(Id==2 && (!bAuxPowerRestored || !IsEncounterCleared(206)))||(Id==3 && !bWardenDefeated))return;
 UOBGameInstance* GI=GetGameInstance<UOBGameInstance>();if(!GI || !GI->MissionSave)return;
 if (WardenMech && Id == 3) WardenMech->Dismount();
 CancelHold();Checkpoint=Id;
 if(Id>0){const EOBWeaponType Selected=Player->EquippedWeapon;Player->RestoreCampaignLoadout(true,Player->GetArcMagazineCapacity(),FMath::Max(96,Player->GetArcReserveAmmo()),Player->bHelixOwned,Player->bHelixOwned?36:0,Player->bHelixOwned?FMath::Max(108,Player->GetHelixReserveAmmo()):0,Selected);}
 else Player->Health=Player->MaxHealth;
 WriteSaveFlags();SnapshotLoadout();const bool bSaved=GI->SaveCheckpoint();
 ShowMessage(bSaved?TEXT("Checkpoint saved — health and ammunition ready."):GI->SaveWarning,5.f);PlaySound(TEXT("Checkpoint"),Player->GetActorLocation(),.65f);
 UE_LOG(LogTemp,Display,TEXT("OB_LEVEL2_CHECKPOINT id=%d disk=%d"),Id,bSaved);
}
void AOBLevel2Director::RetryCheckpoint()
{
 if (WardenMech) WardenMech->Dismount();
 StopCinematic();CancelHold();MachineAttackSlots.Empty();SpawnQueue.Empty();ClearProjectiles();
 if(UOBGameInstance* GI=GetGameInstance<UOBGameInstance>()){GI->bFrontEnd=false;GI->ReloadMission();}
}
void AOBLevel2Director::PlayerDied()
{
 if(bDeadHandled || bCinematic || MissionState==EOBLevel2State::SignalReceived)return;
 bDeadHandled=true;DeathElapsed=0;CancelHold();MachineAttackSlots.Empty();SpawnQueue.Empty();ClearProjectiles();
 if(Player)Player->CancelActions();UE_LOG(LogTemp,Display,TEXT("OB_LEVEL2_PLAYER_DIED checkpoint=%d"),Checkpoint);
}
void AOBLevel2Director::CompleteMission()
{
 if(bCompletionSaved || MissionState!=EOBLevel2State::SignalReceived)return;
 UOBGameInstance* GI=GetGameInstance<UOBGameInstance>();if(!GI || !GI->MissionSave)return;
 const bool bAlreadyCompleted=GI->MissionSave->bLevel2Completed;
 if(!bAlreadyCompleted && (UplinkSeconds<75.f || !IsEncounterCleared(210) || !IsEncounterCleared(211) || !IsEncounterCleared(212) || !IsEncounterCleared(213)))return;
 bCompletionSaved=true;SpawnQueue.Empty();MachineAttackSlots.Empty();CancelHold();ClearProjectiles();Checkpoint=3;WriteSaveFlags();SnapshotLoadout();
 GI->MissionSave->bStabilizerA=GI->MissionSave->bStabilizerB=true;GI->MissionSave->bLevel2Completed=true;GI->MissionSave->bMissionCompleted=true;
 const bool bSaved=GI->SaveCheckpoint();Phase=EOBPhase::Complete;RefreshWorld();
 if(AOBPlayerController* PC=Cast<AOBPlayerController>(Player?Player->GetController():nullptr))PC->ShowScreen(TEXT("Complete"));
 UE_LOG(LogTemp,Display,TEXT("OB_LEVEL2_COMPLETE disk=%d finaleSpawned=%d"),bSaved,FinaleSpawned);
}
void AOBLevel2Director::PlaySound(FName Cue,FVector Location,float Volume,bool bAmbience)
{
 const TObjectPtr<USoundBase>* Sound=ChapterSounds.Find(Cue);if(!Sound || !*Sound)return;
 const UOBGameInstance* GI=GetGameInstance<UOBGameInstance>();float Gain=Volume;
 if(GI && GI->Settings)Gain*=GI->Settings->MasterVolume*(bAmbience?GI->Settings->AmbienceVolume:GI->Settings->EffectsVolume);
 if(Player)Gain*=FMath::Clamp(1.f-FVector::Dist(Player->GetActorLocation(),Location)/6000.f,.03f,1.f);
 if(Gain>.001f)UGameplayStatics::PlaySoundAtLocation(this,*Sound,Location,Gain);
}
bool AOBLevel2Director::ValidateContent(FString& Errors) const
{
 const int32 Ids[]={201,202,203,204,205,206,207,210,211,212,213};const int32 Counts[]={2,2,3,5,1,3,1,4,4,3,2};TSet<FName> Unique;
 for(const AOBMachineEnemy* M:Machines)if(IsValid(M)){if(M->StableId.IsNone() || Unique.Contains(M->StableId))Errors+=TEXT("duplicate/missing machine identity; ");Unique.Add(M->StableId);}
 for(int32 I=0;I<UE_ARRAY_COUNT(Ids);++I){int32 Found=0;for(const AOBMachineEnemy* M:Machines)if(IsValid(M) && M->EncounterId==Ids[I])++Found;if(Found!=Counts[I])Errors+=FString::Printf(TEXT("encounter %d expected %d got %d; "),Ids[I],Counts[I],Found);}
 for(int32 Id:{203,204,206,207}){int32 Found=0;for(const AOBDoor* D:Doors)if(IsValid(D) && D->DoorId==Id)++Found;if(Found!=1)Errors+=FString::Printf(TEXT("gate %d expected once; "),Id);}
 for(EOBLevel2Action A:{EOBLevel2Action::Breaker,EOBLevel2Action::Pump,EOBLevel2Action::Coupler,EOBLevel2Action::Socket,EOBLevel2Action::Power,EOBLevel2Action::StabilizerA,EOBLevel2Action::StabilizerB,EOBLevel2Action::Transmitter})if(!FindAction(A))Errors+=TEXT("missing mission control; ");
 if(!CrashSequence || !RescueSequence)Errors+=TEXT("missing authored sequence; ");if(!Wreck)Errors+=TEXT("missing Star Sparrow wreck; ");
 return Errors.IsEmpty();
}
void AOBLevel2Director::EndPlay(const EEndPlayReason::Type Reason)
{
 GetWorldTimerManager().ClearAllTimersForObject(this);StopCinematic();CancelHold();MachineAttackSlots.Empty();SpawnQueue.Empty();
 if(ChapterAmbience){ChapterAmbience->Stop();ChapterAmbience=nullptr;}AActor::EndPlay(Reason);
}
