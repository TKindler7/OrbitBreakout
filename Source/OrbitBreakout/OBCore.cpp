#include "OBCore.h"
#include "OBTraversalRoute.h"
#include "OBPlayerCharacter.h"
#include "OBEnemyCharacter.h"
#include "OBUI.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/DamageEvents.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/PackageName.h"

namespace OB {
 static const FString MissionSlot(TEXT("OB_EmergencyDeparture"));
 static const FString SettingsSlot(TEXT("OB_Settings"));
 static const FName MissionMap(TEXT("/Game/OrbitBreakout/Maps/L_EmergencyDeparture"));
 static const FName Level2Map(TEXT("/Game/OrbitBreakout/Maps/L_DeadSignal"));
 static const FString DevelopmentSlot(TEXT("OB_DeadSignal_Development"));
 static UStaticMesh* Shape(const TCHAR* Name) { return LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"),Name,Name)); }
 static void Paint(UStaticMeshComponent* Mesh,FLinearColor Color,bool Glow=false) {
  auto* Material=LoadObject<UMaterialInterface>(nullptr,Glow?TEXT("/Game/OrbitBreakout/Materials/M_Emissive"):TEXT("/Game/OrbitBreakout/Materials/M_Metal"));
  if(!Material) Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
  if(Material) { auto* MID=UMaterialInstanceDynamic::Create(Material,Mesh); MID->SetVectorParameterValue(TEXT("Color"),Color); MID->SetVectorParameterValue(TEXT("BaseColor"),Color); MID->SetScalarParameterValue(TEXT("EmissiveStrength"),2.f); Mesh->SetMaterial(0,MID); }
 }
}
void UOBSettings::Clamp() {
 ADSSensitivity=FMath::Clamp(ADSSensitivity,.5f,1.f); TraversalMotion=FMath::Clamp(TraversalMotion,0,2); WallRoll=FMath::Clamp(WallRoll,0.f,8.f);
 MouseSensitivity=FMath::Clamp(MouseSensitivity,.2f,3.f); MasterVolume=FMath::Clamp(MasterVolume,0.f,1.f); EffectsVolume=FMath::Clamp(EffectsVolume,0.f,1.f); AmbienceVolume=FMath::Clamp(AmbienceVolume,0.f,1.f); GraphicsPreset=FMath::Clamp(GraphicsPreset,0,2); CameraShakeScale=FMath::Clamp(CameraShakeScale,0.f,1.f);
}
void UOBMissionSave::SetCheckpointIdentity(int32 Id) {
 CheckpointId=Id;
 static const FName L1Keys[]={TEXT("L1.Cell"),TEXT("L1.Cargo"),TEXT("L1.Reactor")};
 static const FName L2Keys[]={TEXT("L2.Crash"),TEXT("L2.Junction"),TEXT("L2.Power"),TEXT("L2.Finale")};
 CheckpointKey=ChapterId==2 && Id>=0 && Id<4?L2Keys[Id]:ChapterId==1 && Id>=0 && Id<3?L1Keys[Id]:NAME_None;
}
bool UOBMissionSave::IsValidSave() const {
 if(ReserveAmmo<0 || ReserveAmmo>144 || HelixReserveAmmo<0 || HelixReserveAmmo>180)return false;
 // Legacy fields are checked under their original Chapter 1 contract only.
 if(SchemaVersion==1)return MissionId==TEXT("EmergencyDeparture") && CheckpointId>=0 && CheckpointId<=2 && (!bHelixOwned || CheckpointId==2);
 if(SchemaVersion!=2 || ArcMagazine<0 || ArcMagazine>24 || HelixMagazine<0 || HelixMagazine>36 || SelectedWeapon>1)return false;
 if(SelectedWeapon==1 && !bHelixOwned)return false;
 static const FName L1Keys[]={TEXT("L1.Cell"),TEXT("L1.Cargo"),TEXT("L1.Reactor")};
 static const FName L2Keys[]={TEXT("L2.Crash"),TEXT("L2.Junction"),TEXT("L2.Power"),TEXT("L2.Finale")};
 if(ChapterId==1)return MissionId==TEXT("EmergencyDeparture") && CheckpointId>=0 && CheckpointId<3 && CheckpointKey==L1Keys[CheckpointId] && (!bHelixOwned || CheckpointId==2) && !bLevel2Completed;
 if(ChapterId!=2 || MissionId!=TEXT("DeadSignal") || CheckpointId<0 || CheckpointId>3 || CheckpointKey!=L2Keys[CheckpointId] || bPendingChapter2 || !bArcOwned)return false;
 if(CheckpointId>=1 && (!bLevel2IntroComplete || !bJunctionIsolated || !bJunctionDrained))return false;
 if(CheckpointId>=2 && (!bCouplerOwned || !bCouplerInserted || !bAuxPowerRestored))return false;
 if(CheckpointId>=3 && !bWardenDefeated)return false;
 if(bJunctionDrained && !bJunctionIsolated)return false;
 if(bCouplerInserted && !bCouplerOwned)return false;
 if(bAuxPowerRestored && !bCouplerInserted)return false;
 if(bWardenDefeated && !bAuxPowerRestored)return false;
 if(bLevel2Completed && (CheckpointId!=3 || !bMissionCompleted))return false;
 return true;
}
bool UOBMissionSave::MigrateLegacy() {
 if(SchemaVersion==2)return IsValidSave();
 if(SchemaVersion!=1 || !IsValidSave())return false;
 SchemaVersion=2;ChapterId=1;SetCheckpointIdentity(CheckpointId);bPendingChapter2=false;
 bArcOwned=CheckpointId>0;ArcMagazine=bArcOwned?24:0;HelixMagazine=bHelixOwned?36:0;SelectedWeapon=0;
 return IsValidSave();
}
void UOBGameInstance::Init() {
 Super::Init();
 if(GEngine)TravelFailureHandle=GEngine->OnTravelFailure().AddUObject(this,&UOBGameInstance::OnChapterTravelFailure);
 Settings=Cast<UOBSettings>(UGameplayStatics::LoadGameFromSlot(OB::SettingsSlot,0));
 if(!Settings || Settings->SchemaVersion!=1) Settings=NewObject<UOBSettings>(this);
 Settings->Clamp();
 if(UGameplayStatics::DoesSaveGameExist(OB::MissionSlot,0)) {
  MissionSave=Cast<UOBMissionSave>(UGameplayStatics::LoadGameFromSlot(OB::MissionSlot,0));
  bSaveValid=MissionSave && MissionSave->MigrateLegacy();
  if(!bSaveValid) SaveWarning=TEXT("Previous checkpoint could not be read. Start a New Mission to replace it.");
 }
 if(!MissionSave || !bSaveValid) MissionSave=NewObject<UOBMissionSave>(this);
 ApplySettings();
}
void UOBGameInstance::Shutdown() {
 if(GEngine && TravelFailureHandle.IsValid())GEngine->OnTravelFailure().Remove(TravelFailureHandle);
 Super::Shutdown();
}
bool UOBGameInstance::HasValidSave() const { return bSaveValid && MissionSave && MissionSave->IsValidSave(); }
void UOBGameInstance::RestoreCampaignSession() {
 if(!bDevelopmentChapterSession)return;
 MissionSave=CampaignSaveSnapshot?CampaignSaveSnapshot.Get():NewObject<UOBMissionSave>(this);
 bSaveValid=bCampaignSaveSnapshotValid && MissionSave->IsValidSave();
 CampaignSaveSnapshot=nullptr;bDevelopmentChapterSession=false;bChapterTravelRequested=false;
}
void UOBGameInstance::NewMission() {
 RestoreCampaignSession();MissionSave=NewObject<UOBMissionSave>(this);bSaveValid=true;bFrontEnd=false;bChapterTravelRequested=false;SaveCheckpoint();ReloadMission();
}
void UOBGameInstance::ContinueMission() {
 RestoreCampaignSession();if(!HasValidSave())return;
 if(MissionSave->ChapterId==2 && MissionSave->bLevel2Completed){ReplayLevel2();return;}
 // An old completed Chapter 1 save remains useful after the campaign upgrade.
 if(MissionSave->ChapterId==1 && MissionSave->bMissionCompleted){
  MissionSave->bPendingChapter2=true;MissionSave->bArcOwned=true;MissionSave->ArcMagazine=24;
 }
 bFrontEnd=false;ReloadMission();
}
void UOBGameInstance::ReturnToMenu() { RestoreCampaignSession();bFrontEnd=true;bChapterTravelRequested=false;ReloadMission(); }
void UOBGameInstance::ReloadMission() {
 if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetPause(false);
 FName Destination=OB::MissionMap;
 if(!bFrontEnd && MissionSave && (MissionSave->ChapterId==2 || MissionSave->bPendingChapter2))Destination=OB::Level2Map;
 if(Destination==OB::Level2Map && !FPackageName::DoesPackageExist(Destination.ToString())){
  SaveWarning=TEXT("Dead Signal map is unavailable. Your campaign checkpoint is preserved.");
  UE_LOG(LogTemp,Error,TEXT("OB_CHAPTER_MAP_MISSING %s"),*Destination.ToString());
  bChapterTravelRequested=false;bFrontEnd=true;Destination=OB::MissionMap;
 }
 else bChapterTravelRequested=Destination==OB::Level2Map;
 UGameplayStatics::OpenLevel(this,Destination);
}
bool UOBGameInstance::SaveCheckpoint() {
 if(!MissionSave || !MissionSave->IsValidSave()){
  SaveWarning=TEXT("Progress remains in this session; the checkpoint state could not be saved.");
  UE_LOG(LogTemp,Error,TEXT("OB_SAVE_REJECTED invalid chapter/checkpoint snapshot"));return false;
 }
 const FString& Slot=bDevelopmentChapterSession?OB::DevelopmentSlot:OB::MissionSlot;
 const bool OK=UGameplayStatics::SaveGameToSlot(MissionSave,Slot,0);
 bSaveValid=true;
 SaveWarning=OK?TEXT(""):TEXT("Checkpoint reached — disk save unavailable. Previous disk progress may remain.");
 if(!OK)UE_LOG(LogTemp,Error,TEXT("Orbit Breakout: mission disk save failed; in-memory checkpoint retained."));
 return OK;
}
bool UOBGameInstance::TransitionToLevel2(AOBPlayerCharacter* Player) {
 if(bChapterTravelRequested)return true;
 if(!Player || Player->bDead || !MissionSave || MissionSave->ChapterId!=1)return false;
 if(!FPackageName::DoesPackageExist(OB::Level2Map.ToString())){
  SaveWarning=TEXT("Escape reached. Dead Signal is unavailable; your last checkpoint is preserved.");
  UE_LOG(LogTemp,Error,TEXT("OB_CHAPTER_TRANSITION_BLOCKED missing DeadSignal map"));return false;
 }
 // Keep Chapter 1 and its stable checkpoint intact until the destination initializes.
 MissionSave->SchemaVersion=2;MissionSave->ChapterId=1;MissionSave->SetCheckpointIdentity(MissionSave->CheckpointId);
 MissionSave->bPendingChapter2=true;MissionSave->bMissionCompleted=false;
 MissionSave->bArcOwned=Player->bWeaponOwned;MissionSave->bHelixOwned=Player->bHelixOwned;
 MissionSave->ArcMagazine=Player->GetArcMagazineAmmo();MissionSave->ReserveAmmo=Player->GetArcReserveAmmo();
 MissionSave->HelixMagazine=Player->GetHelixMagazineAmmo();MissionSave->HelixReserveAmmo=Player->GetHelixReserveAmmo();
 MissionSave->SelectedWeapon=static_cast<uint8>(Player->EquippedWeapon);
 bSaveValid=true;bFrontEnd=false;bChapterTravelRequested=true;SaveCheckpoint();
 UE_LOG(LogTemp,Display,TEXT("OB_CHAPTER_TRANSITION_REQUESTED from=EmergencyDeparture to=DeadSignal"));
 ReloadMission();return true;
}
void UOBGameInstance::BeginLevel2Session() {
 const bool bCampaignHandoff=!bDevelopmentChapterSession && !bFrontEnd && MissionSave && MissionSave->ChapterId==1 && MissionSave->bPendingChapter2;
 const bool bExistingChapter=!bFrontEnd && MissionSave && MissionSave->ChapterId==2 && MissionSave->IsValidSave();
 if(bCampaignHandoff){
  UOBMissionSave* Next=DuplicateObject<UOBMissionSave>(MissionSave,this);
  Next->MissionId=TEXT("DeadSignal");Next->ChapterId=2;Next->SetCheckpointIdentity(0);Next->bPendingChapter2=false;Next->bMissionCompleted=false;
  Next->bLevel2IntroComplete=false;Next->bJunctionIsolated=false;Next->bJunctionDrained=false;Next->bCouplerOwned=false;Next->bCouplerInserted=false;
  Next->bAuxPowerRestored=false;Next->bWardenDefeated=false;Next->bStabilizerA=false;Next->bStabilizerB=false;Next->bLevel2Completed=false;
  MissionSave=Next;
 }else if(!bExistingChapter){
  if(!bDevelopmentChapterSession){CampaignSaveSnapshot=MissionSave;bCampaignSaveSnapshotValid=bSaveValid;}
  bDevelopmentChapterSession=true;MissionSave=NewObject<UOBMissionSave>(this);
  MissionSave->MissionId=TEXT("DeadSignal");MissionSave->ChapterId=2;MissionSave->SetCheckpointIdentity(0);
  MissionSave->bArcOwned=true;MissionSave->ArcMagazine=24;MissionSave->ReserveAmmo=72;
  MissionSave->bHelixOwned=true;MissionSave->HelixMagazine=36;MissionSave->HelixReserveAmmo=72;
 }
 bChapterTravelRequested=false;bFrontEnd=false;bSaveValid=MissionSave && MissionSave->IsValidSave();
 // A loaded world is a safe restart location even before the crash finishes.
 // Failed persistence leaves the original pending-transition slot on disk.
 if(bCampaignHandoff)SaveCheckpoint();
 UE_LOG(LogTemp,Display,TEXT("OB_LEVEL2_SESSION chapter=%d checkpoint=%d development=%d pendingConsumed=%d"),MissionSave->ChapterId,MissionSave->CheckpointId,bDevelopmentChapterSession,bCampaignHandoff);
}
void UOBGameInstance::ReplayLevel2() {
 if(!FPackageName::DoesPackageExist(OB::Level2Map.ToString())){SaveWarning=TEXT("Dead Signal map is unavailable. Your campaign checkpoint is preserved.");return;}
 if(!bDevelopmentChapterSession){CampaignSaveSnapshot=MissionSave;bCampaignSaveSnapshotValid=bSaveValid;}
 bDevelopmentChapterSession=true;bFrontEnd=false;MissionSave=NewObject<UOBMissionSave>(this);
 MissionSave->MissionId=TEXT("DeadSignal");MissionSave->ChapterId=2;MissionSave->SetCheckpointIdentity(0);
 MissionSave->bArcOwned=true;MissionSave->ArcMagazine=24;MissionSave->ReserveAmmo=72;
 MissionSave->bHelixOwned=true;MissionSave->HelixMagazine=36;MissionSave->HelixReserveAmmo=72;
 bSaveValid=true;ReloadMission();
}
void UOBGameInstance::OnChapterTravelFailure(UWorld* World,ETravelFailure::Type Failure,const FString& Error) {
 if(!bChapterTravelRequested || (World && World->GetGameInstance()!=this))return;
 bChapterTravelRequested=false;bFrontEnd=true;
 SaveWarning=TEXT("Chapter loading failed. Your checkpoint is preserved; Continue can retry.");
 UE_LOG(LogTemp,Error,TEXT("OB_CHAPTER_TRAVEL_FAILED type=%d %s"),static_cast<int32>(Failure),*Error);
 if(auto* PC=Cast<AOBPlayerController>(UGameplayStatics::GetPlayerController(this,0)))PC->ShowScreen(TEXT("Main"));
}
void UOBGameInstance::SaveSettings() { Settings->Clamp(); ApplySettings(); if(!UGameplayStatics::SaveGameToSlot(Settings,OB::SettingsSlot,0)) SaveWarning=TEXT("Settings apply this session; disk save unavailable."); }
void UOBGameInstance::ApplySettings() {
 if(!Settings) return; Settings->Clamp();
 if(GEngine && GEngine->GetGameUserSettings()) { auto* GS=GEngine->GetGameUserSettings(); GS->SetOverallScalabilityLevel(Settings->GraphicsPreset); GS->SetFrameRateLimit(60.f); GS->ApplyNonResolutionSettings(); }
}

AOBDoor::AOBDoor() {
 PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bStartWithTickEnabled=false;
 Panel=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SlidingPanel")); RootComponent=Panel;
 Panel->SetStaticMesh(OB::Shape(TEXT("Cube"))); Panel->SetMobility(EComponentMobility::Movable); Panel->SetCollisionProfileName(TEXT("BlockAll"));
}
void AOBDoor::OnConstruction(const FTransform& T) { Super::OnConstruction(T); Panel->SetRelativeScale3D(PanelSize/100.f); OB::Paint(Panel,FLinearColor(.12,.20,.25)); }
void AOBDoor::BeginPlay() { Super::BeginPlay(); OnConstruction(GetActorTransform()); ClosedPosition=GetActorLocation(); }
void AOBDoor::Open(bool Instant) {
 if(bOpen) return; bOpen=true;
 if(Instant) {OpenAlpha=1;SetActorLocation(ClosedPosition+FVector(0,0,OpenDistance));SetActorTickEnabled(false);} else {SetActorTickEnabled(true);if(auto* D=AOBMissionDirector::Get(this)) D->PlaySound(TEXT("Door"),GetActorLocation());}
}
void AOBDoor::Tick(float DT) { Super::Tick(DT); OpenAlpha=FMath::Min(1.f,OpenAlpha+DT/.6f); SetActorLocation(ClosedPosition+FVector(0,0,OpenDistance*FMath::SmoothStep(0.f,1.f,OpenAlpha))); if(OpenAlpha>=1) SetActorTickEnabled(false); }

AOBInteractable::AOBInteractable() {
 PrimaryActorTick.bCanEverTick=false;
 auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root")); RootComponent=Root;
 Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body")); Body->SetupAttachment(Root); Body->SetStaticMesh(OB::Shape(TEXT("Cube"))); Body->SetCollisionProfileName(TEXT("BlockAll"));
 Glow=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LightPanel")); Glow->SetupAttachment(Root); Glow->SetStaticMesh(OB::Shape(TEXT("Cube"))); Glow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 WeaponDisplay=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponDisplay"));WeaponDisplay->SetupAttachment(Root);WeaponDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);WeaponDisplay->SetCanEverAffectNavigation(false);
 WeaponDisplayRail=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponDisplayRail"));WeaponDisplayRail->SetupAttachment(Root);WeaponDisplayRail->SetStaticMesh(OB::Shape(TEXT("Cube")));WeaponDisplayRail->SetCollisionEnabled(ECollisionEnabled::NoCollision);WeaponDisplayRail->SetCanEverAffectNavigation(false);
 Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label")); Label->SetupAttachment(Root); Label->SetHorizontalAlignment(EHTA_Center); Label->SetWorldSize(14); Label->SetTextRenderColor(FColor(100,235,225));
}
void AOBInteractable::OnConstruction(const FTransform& T) {
 Super::OnConstruction(T);
 FVector Size(40,70,100),Color(.13,.22,.28); FString Text;
 switch(Kind) {
 case EOBInteractionKind::Weapon: Size=FVector(35,80,85);Text=TEXT("ARC-9 / SECURITY BLASTER");break;
 case EOBInteractionKind::HeavyWeapon:Size=FVector(48,120,115);Color=FVector(.05,.16,.19);Text=TEXT("HELIX-12 / PLASMA REPEATER");break;
 case EOBInteractionKind::AmmoStation:Size=FVector(42,90,130);Text=TEXT("SECURITY AMMUNITION");break;
 case EOBInteractionKind::Terminal:Size=FVector(70,115,115);Text=TEXT("TRANSIT CONTROL");break;
 case EOBInteractionKind::Pod:Size=FVector(45,75,95);Text=TEXT("STAR SPARROW / BOARDING");break;
 case EOBInteractionKind::Medical:Size=FVector(32,48,34);Text=TEXT("+  MEDICAL");break;
 case EOBInteractionKind::Ammo:Size=FVector(26,38,40);Text=TEXT("III  ENERGY CELLS");break;
 }
 Body->SetRelativeLocation(FVector(0,0,Size.Z*.5f)); Body->SetRelativeScale3D(Size/100.f);
 Glow->SetRelativeLocation(FVector(-Size.X*.5f-1,0,Size.Z*.7f));Glow->SetRelativeScale3D(FVector(.025,Size.Y*.007f,Size.Z*.0025f));
 Label->SetRelativeLocation(FVector(-Size.X*.5f-4,0,Size.Z+26));Label->SetRelativeRotation(FRotator(0,180,0));Label->SetText(FText::FromString(Text));
 OB::Paint(Body,FLinearColor(Color));OB::Paint(Glow,Kind==EOBInteractionKind::Ammo||Kind==EOBInteractionKind::AmmoStation?FLinearColor(1,.4,.08):FLinearColor(.05,.8,.73),true);
 const bool bWeaponRack=Kind==EOBInteractionKind::Weapon||Kind==EOBInteractionKind::HeavyWeapon;
 WeaponDisplay->SetVisibility(bWeaponRack);WeaponDisplayRail->SetVisibility(Kind==EOBInteractionKind::HeavyWeapon);
 if(bWeaponRack){
  WeaponDisplay->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Weapons/Rifle/Meshes/SM_Rifle.SM_Rifle")));
  WeaponDisplay->SetRelativeLocation(FVector(-Size.X*.5f-8,-25,Size.Z*.65f));WeaponDisplay->SetRelativeRotation(FRotator(0,0,0));
  WeaponDisplay->SetRelativeScale3D(FVector(1.f));
  if(auto* Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/OrbitBreakout/Materials/M_Arc9.M_Arc9")))WeaponDisplay->SetMaterial(0,Mat);
  WeaponDisplayRail->SetRelativeLocation(FVector(-Size.X*.5f-14,10,Size.Z*.65f+5));WeaponDisplayRail->SetRelativeScale3D(FVector(.025,.42,.025));
  OB::Paint(WeaponDisplayRail,FLinearColor(.08,1,.65),true);
 }
 if(Kind==EOBInteractionKind::Pod){
  auto* Ship=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/StarSparrow/Meshes/Examples/SM_StarSparrow01.SM_StarSparrow01"));
  if(Ship){
   WeaponDisplay->SetStaticMesh(Ship);WeaponDisplay->EmptyOverrideMaterials();WeaponDisplay->SetVisibility(true);
   const FVector Extent=Ship->GetBounds().BoxExtent;
   const float Scale=FMath::Min(720.f/FMath::Max(1.f,Extent.X*2),650.f/FMath::Max(1.f,Extent.Y*2));
   WeaponDisplay->SetRelativeScale3D(FVector(Scale));WeaponDisplay->SetRelativeRotation(FRotator::ZeroRotator);
   WeaponDisplay->SetRelativeLocation(FVector(490,0,100+Extent.Z*Scale)-Ship->GetBounds().Origin*Scale);
  }
 }
 Body->SetCollisionEnabled(Kind==EOBInteractionKind::Medical||Kind==EOBInteractionKind::Ammo?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);
}
void AOBInteractable::BeginPlay() {
 Super::BeginPlay();OnConstruction(GetActorTransform());
 if(Kind==EOBInteractionKind::Pod){
  TArray<AActor*> Children;GetAttachedActors(Children);
  for(auto* Child:Children){Child->SetActorHiddenInGame(true);Child->SetActorEnableCollision(false);}
 }
}
void AOBInteractable::SetUsed() {bUsed=true;if(Kind==EOBInteractionKind::Weapon||Kind==EOBInteractionKind::HeavyWeapon||Kind==EOBInteractionKind::Medical||Kind==EOBInteractionKind::Ammo){SetActorHiddenInGame(true);SetActorEnableCollision(false);}else if(Kind==EOBInteractionKind::Terminal){Label->SetText(FText::FromString(TEXT("OVERRIDE ACCEPTED")));}}
FString AOBInteractable::GetPrompt() const {
 switch(Kind) {
 case EOBInteractionKind::Weapon:return TEXT("E — Take ARC-9");
 case EOBInteractionKind::HeavyWeapon:return TEXT("E — Take HELIX-12 plasma repeater");
 case EOBInteractionKind::AmmoStation:return TEXT("E — Replenish ammunition");
 case EOBInteractionKind::Terminal:return bUsed?TEXT("Override accepted"):TEXT("Hold E — Bypass security (2 sec)");
 case EOBInteractionKind::Pod: {auto* D=AOBMissionDirector::Get(this);return D&&D->Phase==EOBPhase::BoardPod?TEXT("Hold E — Board Star Sparrow (1 sec)"):TEXT("Secure the bay before launch");}
 default:return TEXT("");
 }
}

AOBZoneTrigger::AOBZoneTrigger(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.1f;Box=CreateDefaultSubobject<UBoxComponent>(TEXT("Threshold"));RootComponent=Box;Box->SetCollisionProfileName(TEXT("Trigger"));}
void AOBZoneTrigger::OnConstruction(const FTransform& T){Super::OnConstruction(T);Box->SetBoxExtent(TriggerExtent);}
void AOBZoneTrigger::BeginPlay(){Super::BeginPlay();Director=AOBMissionDirector::Get(this);Box->OnComponentBeginOverlap.AddDynamic(this,&AOBZoneTrigger::Overlap);}
void AOBZoneTrigger::Tick(float DeltaSeconds){
 Super::Tick(DeltaSeconds);
 if(!Director.IsValid())Director=AOBMissionDirector::Get(this);
 // Teleporting a checkpoint pawn or pausing on a threshold can consume the
 // begin-overlap before gameplay is ready. Retry only while still in this box.
 if(Director.IsValid()&&Director->Player&&Box->IsOverlappingActor(Director->Player))Director->EnterZone(Zone,bCheckpoint);
}
void AOBZoneTrigger::Overlap(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,int32,bool,const FHitResult&){if(Cast<AOBPlayerCharacter>(Other)){if(!Director.IsValid())Director=AOBMissionDirector::Get(this);if(Director.IsValid())Director->EnterZone(Zone,bCheckpoint);}}

AOBMissionDirector::AOBMissionDirector(){PrimaryActorTick.bCanEverTick=true;}
AOBMissionDirector* AOBMissionDirector::Get(const UObject* Context){if(!Context||!Context->GetWorld())return nullptr;TActorIterator<AOBMissionDirector> It(Context->GetWorld());if(It)return *It;return nullptr;}
void AOBMissionDirector::BeginPlay(){
 Super::BeginPlay();
 AOBTraversalRoute::Install(*this,false);
 for(TActorIterator<AOBEnemyCharacter> It(GetWorld());It;++It){Enemies.Add(*It);ExpectedRoster.FindOrAdd(It->EncounterId).Add(It->StableId);}
 for(TActorIterator<AOBDoor> It(GetWorld());It;++It)Doors.Add(*It);
 for(TActorIterator<AOBInteractable> It(GetWorld());It;++It)Interactions.Add(*It);
 for(const TCHAR* Name:{TEXT("Shot"),TEXT("Reload"),TEXT("DryFire"),TEXT("Damage"),TEXT("Pickup"),TEXT("Charge"),TEXT("EnemyShot"),TEXT("Impact"),TEXT("EnemyDamage"),TEXT("EnemyDeath"),TEXT("Door"),TEXT("Terminal"),TEXT("Checkpoint"),TEXT("Ambience"),TEXT("Alarm"),TEXT("Launch")}) {
  const FString Path=FString::Printf(TEXT("/Game/OrbitBreakout/Audio/S_%s.S_%s"),Name,Name); if(auto* Sound=LoadObject<USoundBase>(nullptr,*Path))Sounds.Add(FName(Name),Sound);
 }
 GetWorldTimerManager().SetTimerForNextTick(this,&AOBMissionDirector::InitializeMission);
}
void AOBMissionDirector::InitializeMission(){
 Player=Cast<AOBPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)); if(!Player){UE_LOG(LogTemp,Error,TEXT("Orbit: player missing"));return;}
 auto* GI=GetGameInstance<UOBGameInstance>(); auto* PC=Cast<AOBPlayerController>(Player->GetController());
 FString Errors;if(!ValidateContent(Errors))UE_LOG(LogTemp,Error,TEXT("Orbit content validation: %s"),*Errors);
 int32 CP=GI&&GI->HasValidSave()&&!GI->bFrontEnd?GI->MissionSave->CheckpointId:0;
 FVector Spawn=CP==2?ReactorCheckpointLocation:CP==1?CargoCheckpointLocation:StartLocation;
 Player->SetActorLocation(Spawn,false,nullptr,ETeleportType::TeleportPhysics);Player->SetActorRotation(FRotator::ZeroRotator);if(PC)PC->SetControlRotation(FRotator(-8,0,0));
 Player->RestoreLoadout(CP>0,CP>0?Player->GetArcMagazineCapacity():0,CP>0?FMath::Max(96,GI->MissionSave->ReserveAmmo):0);
 if(CP==2 && GI->MissionSave->bHelixOwned)Player->RestoreHeavyWeapon(true,FMath::Max(108,GI->MissionSave->HelixReserveAmmo),true);
 if(CP>=1)ClearedEncounters.Add(1);
 if(CP==2){ClearedEncounters.Append({2,3,4});bTerminalComplete=true;Phase=EOBPhase::Reactor;}
 else Phase=CP==1?EOBPhase::Cargo:EOBPhase::Cell;
 for(auto E:Enemies)if(E&&ClearedEncounters.Contains(E->EncounterId))E->SetResolved();
 for(auto I:Interactions)if(I){if((CP>0&&I->Kind==EOBInteractionKind::Weapon)||(Player->bHelixOwned&&I->Kind==EOBInteractionKind::HeavyWeapon)||(CP==2&&I->Kind==EOBInteractionKind::Terminal)||((I->Kind==EOBInteractionKind::Medical||I->Kind==EOBInteractionKind::Ammo)&&I->Zone<(CP==2?5:CP==1?3:1)))I->SetUsed();}
 bCellDoorReleased=CP>0;CellReleaseTime=GetWorld()->GetTimeSeconds()+1.2f;
 RefreshDoors(true);bInitialized=true;
 if(GI&&GI->bFrontEnd){if(PC)PC->ShowScreen(TEXT("Main"));}
 else {if(PC)PC->HideScreen();ShowMessage(CP==0?TEXT("Containment power interrupted. Find the security blaster."):TEXT("Checkpoint restored — systems ready."),6);}
 if(Sounds.Contains(TEXT("Ambience"))){Ambience=UGameplayStatics::SpawnSound2D(this,Sounds[TEXT("Ambience")],GI&&GI->Settings?GI->Settings->MasterVolume*GI->Settings->AmbienceVolume*.3f:.1f,1,0,nullptr,false,false);}
 UE_LOG(LogTemp,Display,TEXT("OB_READY checkpoint=%d frontEnd=%d enemies=%d doors=%d interactions=%d"),CP,GI?GI->bFrontEnd:1,Enemies.Num(),Doors.Num(),Interactions.Num());
}
bool AOBMissionDirector::IsGameplayActive() const {
 if(!bInitialized||!Player||Player->bDead||bBossIntroActive||Phase==EOBPhase::Ending||Phase==EOBPhase::Complete)return false;
 auto* GI=GetGameInstance<UOBGameInstance>();if(GI&&GI->bFrontEnd)return false;
 auto* PC=Cast<AOBPlayerController>(Player->GetController());return !UGameplayStatics::IsGamePaused(this)&&(!PC||!PC->IsMenuOpen());
}
bool AOBMissionDirector::IsCombatAllowed() const{return IsGameplayActive();}

bool AOBMissionDirector::BeginBossIntro(AOBEnemyCharacter* Boss)
{
 if(!IsValid(Boss)||!Boss->bHeavy||!Boss->bActive||Boss->IsDead()||Boss->HasPlayedBossIntro()||!IsGameplayActive())return false;
 bBossIntroActive=true;bBossIntroReturning=false;BossIntroElapsed=BossIntroReturnElapsed=0.f;IntroBoss=Boss;
 CancelHold();AttackSlots.Empty();Player->CancelActions();Player->ConsumeMovementInputVector();
 UCharacterMovementComponent* Movement=Player->GetCharacterMovement();
 BossIntroPreviousMovement=static_cast<uint8>(Movement->MovementMode);
 BossIntroPreviousCustomMovement=Movement->CustomMovementMode;bBossIntroHeldMovement=true;
 Movement->StopMovementImmediately();Movement->DisableMovement();
 Boss->StartBossIntroAnimation();
 APlayerController* PC=Cast<APlayerController>(Player->GetController());BossIntroController=PC;
 if(PC)
 {
  PC->FlushPressedKeys();BossIntroPreviousView=PC->GetViewTarget();
  const FVector Focus=Boss->GetActorLocation()+FVector(0,0,35);
  FVector CameraPosition=Boss->GetActorLocation()+Boss->GetActorForwardVector()*510.f+Boss->GetActorRightVector()*140.f+FVector(0,0,80);
  FCollisionQueryParams Query(SCENE_QUERY_STAT(OBBossIntroCamera),false,Boss);Query.AddIgnoredActor(Player);
  FHitResult Hit;
  if(GetWorld()->SweepSingleByChannel(Hit,Focus,CameraPosition,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(18.f),Query))
   CameraPosition=Hit.Location+Hit.Normal*20.f;
  // If a nearby prop leaves no useful close-up, keep the existing player view;
  // the animation and input gate still play safely with no camera penetration.
  if(FVector::DistSquared(CameraPosition,Focus)>FMath::Square(220.f))
  {
   FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   ACameraActor* Camera=GetWorld()->SpawnActor<ACameraActor>(CameraPosition,(Focus-CameraPosition).Rotation(),Params);
   if(Camera)
   {
    Camera->GetCameraComponent()->SetFieldOfView(58.f);BossIntroCamera=Camera;
    PC->SetViewTargetWithBlend(Camera,.45f,VTBlend_Cubic,0.f,true);
   }
  }
 }
 UE_LOG(LogTemp,Display,TEXT("OB_BOSS_INTRO_STARTED %s seconds=%.2f"),*Boss->StableId.ToString(),FMath::Clamp(Boss->BossIntroSeconds,1.f,6.f));
 return true;
}

bool AOBMissionDirector::OwnsBossIntroView() const
{
 const APlayerController* PC=BossIntroController.Get();
 if(!PC||!IsValid(BossIntroCamera))return false;
 AActor* Target=PC->GetViewTarget();
 if(Target==BossIntroCamera)return true;
 AActor* ReturnTarget=BossIntroPreviousView.IsValid()?BossIntroPreviousView.Get():Player.Get();
 return bBossIntroReturning&&Target==ReturnTarget&&PC->PlayerCameraManager&&PC->PlayerCameraManager->ViewTarget.Target==BossIntroCamera;
}

void AOBMissionDirector::TickBossIntro(float DeltaSeconds)
{
 if(!bBossIntroActive)return;
 const UOBGameInstance* GI=GetGameInstance<UOBGameInstance>();
 if(!IntroBoss.IsValid()||IntroBoss->IsDead()||!Player||Player->bDead||Phase==EOBPhase::Ending||Phase==EOBPhase::Complete||(GI&&GI->bFrontEnd))
 {FinishBossIntro(false);return;}
 const AOBPlayerController* MenuPC=Cast<AOBPlayerController>(BossIntroController.Get());
 // Explicit pause/menu checks also protect editor and isolated test ticks.
 if(UGameplayStatics::IsGamePaused(this)||(MenuPC&&MenuPC->IsMenuOpen()))return;
 if(!bBossIntroReturning)
 {
  BossIntroElapsed+=FMath::Max(0.f,DeltaSeconds);
  if(BossIntroElapsed<FMath::Clamp(IntroBoss->BossIntroSeconds,1.f,6.f))return;
  IntroBoss->FinishBossIntroAnimation();
  bBossIntroReturning=true;BossIntroReturnElapsed=0.f;
  if(APlayerController* PC=BossIntroController.Get())
  {
   AActor* ReturnTarget=BossIntroPreviousView.IsValid()?BossIntroPreviousView.Get():Player.Get();
   if(OwnsBossIntroView()&&ReturnTarget)PC->SetViewTargetWithBlend(ReturnTarget,.65f,VTBlend_Cubic,0.f,true);
  }
  return;
 }
 BossIntroReturnElapsed+=FMath::Max(0.f,DeltaSeconds);
 if(BossIntroReturnElapsed<.65f)return;
 // Keep the close-up actor alive if the camera manager has not consumed the
 // last frame of its return blend yet. Never release combat mid-blend.
 if(APlayerController* PC=BossIntroController.Get())
  if(OwnsBossIntroView()&&PC->PlayerCameraManager&&PC->PlayerCameraManager->BlendTimeToGo>0.f)return;
 FinishBossIntro(true);
}

void AOBMissionDirector::FinishBossIntro(bool bResumeCombat)
{
 if(!bBossIntroActive&&!BossIntroCamera)return;
 const bool bRestoreMovement=bBossIntroHeldMovement&&IsValid(Player)&&!Player->bDead&&Phase!=EOBPhase::Ending&&Phase!=EOBPhase::Complete;
 float OutgoingCameraLifetime=0.f;
 if(APlayerController* PC=BossIntroController.Get())
 {
  AActor* ReturnTarget=BossIntroPreviousView.IsValid()?BossIntroPreviousView.Get():Player.Get();
  if(OwnsBossIntroView()&&ReturnTarget)
  {
   // UE ignores SetViewTarget when the target is already pending. Clear only
   // our owned transition first so cancellation completes the return NOW.
   if(PC->PlayerCameraManager){PC->PlayerCameraManager->PendingViewTarget.Target=nullptr;PC->PlayerCameraManager->BlendTimeToGo=0.f;}
   PC->SetViewTarget(ReturnTarget);
  }
  else if(PC->PlayerCameraManager&&PC->PlayerCameraManager->ViewTarget.Target==BossIntroCamera&&PC->PlayerCameraManager->PendingViewTarget.Target)
   OutgoingCameraLifetime=FMath::Max(.1f,PC->PlayerCameraManager->BlendTimeToGo+.1f);
  PC->FlushPressedKeys();
 }
 if(IntroBoss.IsValid())
 {
  IntroBoss->FinishBossIntroAnimation();
  if(bResumeCombat)IntroBoss->ResumeAfterBossIntro();
 }
 if(bRestoreMovement)
 {
  Player->CancelActions();Player->ConsumeMovementInputVector();
  Player->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(BossIntroPreviousMovement),BossIntroPreviousCustomMovement);
 }
 if(IsValid(BossIntroCamera))
 {
  if(OutgoingCameraLifetime>0.f)BossIntroCamera->SetLifeSpan(OutgoingCameraLifetime);
  else BossIntroCamera->Destroy();
 }
 BossIntroCamera=nullptr;BossIntroPreviousView.Reset();BossIntroController.Reset();IntroBoss.Reset();
 bBossIntroActive=false;bBossIntroReturning=false;bBossIntroHeldMovement=false;
 if(bResumeCombat)ShowMessage(TEXT("SENTINEL ACTIVE  /  Dodge the charge. Jump over shockwaves."),4.f);
 UE_LOG(LogTemp,Display,TEXT("OB_BOSS_INTRO_FINISHED resume=%d"),bResumeCombat);
}

void AOBMissionDirector::CancelBossIntro()
{
 FinishBossIntro(Phase!=EOBPhase::Ending&&Phase!=EOBPhase::Complete&&Player&&!Player->bDead);
}

void AOBMissionDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
 FinishBossIntro(false);
 Super::EndPlay(EndPlayReason);
}

void AOBMissionDirector::Tick(float DT){
 Super::Tick(DT);if(!bInitialized)return;
 if(Ambience){auto* GI=GetGameInstance<UOBGameInstance>();if(GI&&GI->Settings)Ambience->SetVolumeMultiplier(GI->Settings->MasterVolume*GI->Settings->AmbienceVolume*.3f);}
 if(bBossIntroActive){TickBossIntro(DT);return;}
 if(Phase==EOBPhase::Ending){
  EndingElapsed+=DT;
  if(IsValid(EndingPod)){
   EndingPod->AddActorWorldOffset(FVector(DT*450,0,DT*95));
   if(IsValid(EndingCamera)){
    const FVector PodPosition=EndingPod->GetActorLocation();
    const FVector Focus=PodPosition+FVector(400,0,240);
    EndingCamera->SetActorLocation(PodPosition+FVector(1100,850,500));
    EndingCamera->SetActorRotation((Focus-EndingCamera->GetActorLocation()).Rotation());
   }
  }
  if(EndingElapsed>=9.f)CompleteMission();return;
 }
 if(!IsGameplayActive())return;
 if(!bCellDoorReleased&&GetWorld()->GetTimeSeconds()>=CellReleaseTime){bCellDoorReleased=true;RefreshDoors();PlaySound(TEXT("Alarm"),Player->GetActorLocation(),.25f,true);}
 if(Player->GetActorLocation().Z < -500){Player->EndSpawnGrace();Player->TakeDamage(Player->MaxHealth+1,FDamageEvent(),nullptr,this);return;}
 PickupAccumulator+=DT;if(PickupAccumulator>=.12f){PickupAccumulator=0;CheckPickups();}
}
bool AOBMissionDirector::TryAcquireAttackSlot(AActor* E){
 if(!IsValid(E)||!IsCombatAllowed())return false;for(auto It=AttackSlots.CreateIterator();It;++It)if(!It->IsValid())It.RemoveCurrent();
 if(AttackSlots.Contains(E))return true;if(AttackSlots.Num()>=2)return false;AttackSlots.Add(E);return true;
}
void AOBMissionDirector::ReleaseAttackSlot(AActor* E){AttackSlots.Remove(E);}
void AOBMissionDirector::ActivateEncounter(int32 Id){
 if(!IsGameplayActive()||Activated.Contains(Id)||ClearedEncounters.Contains(Id))return;
 int32 Count=0;bool ValidIds=true;TSet<FName> Ids;
 for(auto E:Enemies)if(IsValid(E)&&E->EncounterId==Id){++Count;if(E->StableId.IsNone()||Ids.Contains(E->StableId))ValidIds=false;Ids.Add(E->StableId);}
 const int32 Expected[]={0,3,3,2,3,1,4};if(Id<1||Id>6||Count!=Expected[Id]||!ValidIds){UE_LOG(LogTemp,Error,TEXT("Invalid encounter %d: count=%d unique=%d"),Id,Count,ValidIds);ShowMessage(TEXT("Encounter data requires repair."));return;}
 Activated.Add(Id);ActiveEncounter=Id;Player->EndSpawnGrace();for(auto E:Enemies)if(E&&E->EncounterId==Id)E->Activate();
 UE_LOG(LogTemp,Display,TEXT("OB_ENCOUNTER_STARTED E%02d count=%d"),Id,Count);
}
void AOBMissionDirector::ResolveEnemy(int32 Id,FName StableId){
 if(!Activated.Contains(Id)||ClearedEncounters.Contains(Id))return;
 const auto* Roster=ExpectedRoster.Find(Id);if(!Roster)return;const int32 Expected=Roster->Num();
 if(!Roster->Contains(StableId)||Resolved.FindOrAdd(Id).Contains(StableId))return;Resolved[Id].Add(StableId);
 if(Expected==0||Resolved[Id].Num()!=Expected)return;
 ClearedEncounters.AddUnique(Id);ActiveEncounter=0;AttackSlots.Empty();
 UE_LOG(LogTemp,Display,TEXT("OB_ENCOUNTER_CLEARED E%02d"),Id);
 switch(Id){
 case 1:Phase=EOBPhase::Cargo;ShowMessage(TEXT("Security corridor clear. Cargo access granted."));break;
 case 2:ShowMessage(TEXT("Security reinforcements — service door opening."));GetWorldTimerManager().SetTimer(CargoWaveTimer,[this](){ActivateEncounter(3);},2.f,false);break;
 case 3:Phase=EOBPhase::Terminal;ShowMessage(TEXT("Forward transit locked. Use the security terminal."));break;
 case 4:Phase=EOBPhase::Reactor;ShowMessage(TEXT("Transit access restored. Proceed to reactor."));break;
 case 5:Phase=EOBPhase::PodBay;ShowMessage(TEXT("Evacuation route open. Reach the escape-pod bay."));PlaySound(TEXT("Alarm"),Player->GetActorLocation(),.45f,true);break;
 case 6:Phase=EOBPhase::BoardPod;ShowMessage(TEXT("Bay secured. Star Sparrow is ready for departure."));break;
 }
 RefreshDoors();PlaySound(TEXT("Checkpoint"),Player->GetActorLocation(),.5f);
}
void AOBMissionDirector::EnterZone(int32 Zone,bool Checkpoint){
 if(!IsGameplayActive()||!Player->bWeaponOwned)return;
 if(Checkpoint){if(Zone==3&&ClearedEncounters.Contains(1))CommitCheckpoint(1);if(Zone==5&&bTerminalComplete&&ClearedEncounters.Contains(4))CommitCheckpoint(2);return;}
 if(Zone==2&&Phase==EOBPhase::Corridor)ActivateEncounter(1);
 if(Zone==3&&Phase==EOBPhase::Cargo)ActivateEncounter(2);
 if(Zone==5&&Phase==EOBPhase::Reactor){
  if(!Player->bHelixOwned){if(GetWorld()->GetTimeSeconds()>=SystemMessageUntil)ShowMessage(TEXT("Arm yourself: take HELIX-12 from the entrance rack."),4);return;}
  ActivateEncounter(5);
 }
 if(Zone==6&&Phase==EOBPhase::PodBay)ActivateEncounter(6);
}
void AOBMissionDirector::CommitCheckpoint(int32 Id){
 if(!IsGameplayActive()||(Id!=1&&Id!=2)||ActiveEncounter!=0)return;
 auto* GI=GetGameInstance<UOBGameInstance>();if(!GI||!GI->MissionSave||Id<=GI->MissionSave->CheckpointId)return;
 if((Id==1&&(Phase!=EOBPhase::Cargo||!ClearedEncounters.Contains(1)))||(Id==2&&(Phase!=EOBPhase::Reactor||!bTerminalComplete||!ClearedEncounters.Contains(4))))return;
 const bool bHadHelix=Player->bHelixOwned;
 const bool bHadHelixEquipped=Player->EquippedWeapon==EOBWeaponType::Helix12;
 const int32 ArcReserve=FMath::Max(Player->GetArcReserveAmmo(),96),HelixReserve=FMath::Max(Player->GetHelixReserveAmmo(),108);
 CancelHold();Player->RestoreLoadout(true,Player->GetArcMagazineCapacity(),ArcReserve);
 if(bHadHelix)Player->RestoreHeavyWeapon(true,HelixReserve,bHadHelixEquipped);
 GI->MissionSave->SetCheckpointIdentity(Id);GI->MissionSave->ReserveAmmo=Player->GetArcReserveAmmo();
 GI->MissionSave->bArcOwned=Player->bWeaponOwned;GI->MissionSave->ArcMagazine=Player->GetArcMagazineAmmo();GI->MissionSave->HelixMagazine=Player->GetHelixMagazineAmmo();GI->MissionSave->SelectedWeapon=static_cast<uint8>(Player->EquippedWeapon);
 GI->MissionSave->bHelixOwned=bHadHelix;GI->MissionSave->HelixReserveAmmo=bHadHelix?Player->GetHelixReserveAmmo():0;GI->bSaveValid=true;
 bool Saved=GI->SaveCheckpoint();ShowMessage(Saved?TEXT("Checkpoint saved — health and ammunition restored."):GI->SaveWarning);PlaySound(TEXT("Checkpoint"),Player->GetActorLocation());
 UE_LOG(LogTemp,Display,TEXT("OB_CHECKPOINT CP%02d reserve=%d disk=%d"),Id,Player->ReserveAmmo,Saved);
}
void AOBMissionDirector::RefreshDoors(bool Instant){
 for(auto D:Doors)if(IsValid(D)){bool Unlock=(D->DoorId==0&&bCellDoorReleased)||(D->DoorId==1&&Player&&Player->bWeaponOwned)||(D->DoorId==2&&ClearedEncounters.Contains(1))||(D->DoorId==3&&ClearedEncounters.Contains(2)&&ClearedEncounters.Contains(3))||(D->DoorId==4&&bTerminalComplete&&ClearedEncounters.Contains(4))||(D->DoorId==5&&ClearedEncounters.Contains(5))||(D->DoorId==6&&ClearedEncounters.Contains(6));if(Unlock)D->Open(Instant);}
}
void AOBMissionDirector::WeaponAcquired(){if(Phase!=EOBPhase::Cell)return;Phase=EOBPhase::Corridor;RefreshDoors();ShowMessage(TEXT("ARC-9 acquired. Right mouse to aim. Left mouse to fire."),6);}
void AOBMissionDirector::RetryCheckpoint(){CancelBossIntro();CancelHold();AttackSlots.Empty();if(auto* GI=GetGameInstance<UOBGameInstance>()){GI->bFrontEnd=false;GI->ReloadMission();}}
void AOBMissionDirector::PlayerDied(){if(bDeathHandled||Phase==EOBPhase::Ending||Phase==EOBPhase::Complete)return;bDeathHandled=true;FinishBossIntro(false);CancelHold();AttackSlots.Empty();GetWorldTimerManager().ClearTimer(CargoWaveTimer);if(Player)Player->CancelActions();GetWorldTimerManager().SetTimer(DeathTimer,[this](){if(auto* PC=Cast<AOBPlayerController>(UGameplayStatics::GetPlayerController(this,0)))PC->ShowScreen(TEXT("Dead"));},.75f,false);}
FString AOBMissionDirector::GetObjectiveText() const {
 switch(Phase){
 case EOBPhase::Cell:return TEXT("Leave the cell. Take the security blaster.");case EOBPhase::Corridor:return TEXT("Clear the security corridor.");case EOBPhase::Cargo:return TEXT("Fight through cargo processing.");case EOBPhase::Terminal:return TEXT("Bypass the transit terminal.");case EOBPhase::Security:return TEXT("Clear the security response.");case EOBPhase::Reactor:return Player&&!Player->bHelixOwned?TEXT("Take HELIX-12 before entering containment."):TEXT("Defeat the Bull sentinel. Reach evacuation.");case EOBPhase::PodBay:return TEXT("Secure the escape-pod bay.");case EOBPhase::BoardPod:return TEXT("Board Star Sparrow and launch.");case EOBPhase::Ending:return TEXT("Emergency departure in progress.");case EOBPhase::Complete:return TEXT("Rook is clear. Mission complete.");}return TEXT("");
}
void AOBMissionDirector::ShowMessage(const FString& Text,float Seconds){SystemMessage=Text;SystemMessageUntil=GetWorld()->GetTimeSeconds()+Seconds;}
bool AOBMissionDirector::ValidInteraction(AOBPlayerCharacter* P,AOBInteractable* I) const {
 if(!IsValid(P)||!IsValid(I)||!P->GetController()||I->bUsed||!IsGameplayActive()||P->bReloading||P->GetCharacterMovement()->IsFalling())return false;
 FVector Target=I->GetActorLocation()+FVector(0,0,80);if(FVector::Dist(P->GetActorLocation(),Target)>200)return false;
 FVector Eye;FRotator Rot;P->GetController()->GetPlayerViewPoint(Eye,Rot);if(FVector::DotProduct((Target-Eye).GetSafeNormal(),Rot.Vector())<.3f)return false;
 FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(OBInteraction),false,P);Params.AddIgnoredActor(I);
 if(GetWorld()->LineTraceSingleByChannel(Hit,P->GetActorLocation()+FVector(0,0,40),Target,ECC_Visibility,Params))return false;
 return true;
}
AOBInteractable* AOBMissionDirector::FindInteraction(AOBPlayerCharacter* P) const {
 AOBInteractable* Best=nullptr;float Dist=MAX_flt;for(auto I:Interactions){if(!I||I->Kind==EOBInteractionKind::Medical||I->Kind==EOBInteractionKind::Ammo||!ValidInteraction(P,I))continue;float D=FVector::DistSquared(P->GetActorLocation(),I->GetActorLocation());if(D<Dist){Best=I;Dist=D;}}return Best;
}
bool AOBMissionDirector::TryInteract(AOBPlayerCharacter* P,AOBInteractable* I){
 if(!ValidInteraction(P,I))return false;
 switch(I->Kind){
 case EOBInteractionKind::Weapon:if(!P->bWeaponOwned&&Phase==EOBPhase::Cell){I->SetUsed();P->AcquireWeapon();return true;}break;
 case EOBInteractionKind::HeavyWeapon:
  if(P->bWeaponOwned&&!P->bHelixOwned&&Phase==EOBPhase::Reactor&&ActiveEncounter==0){
   P->AcquireHeavyWeapon();I->SetUsed();
   if(auto* GI=GetGameInstance<UOBGameInstance>())if(GI->MissionSave&&GI->MissionSave->CheckpointId==2){
    GI->MissionSave->bHelixOwned=true;GI->MissionSave->HelixReserveAmmo=P->GetHelixReserveAmmo();
    GI->MissionSave->ReserveAmmo=P->GetArcReserveAmmo();if(!GI->SaveCheckpoint())ShowMessage(TEXT("HELIX-12 acquired this session; checkpoint disk save unavailable."),6);
   }
   return true;
  }break;
 case EOBInteractionKind::AmmoStation:{if(!P->bWeaponOwned)return false;if(GetWorld()->TimeSeconds<I->CooldownUntil)return false;I->CooldownUntil=GetWorld()->TimeSeconds+3;const int32 Supply=FMath::Min(P->EquippedWeapon==EOBWeaponType::Helix12?108:72,P->MaxReserveAmmo);if(P->AddAmmo(FMath::Max(0,Supply-P->ReserveAmmo))){ShowMessage(TEXT("Reserve replenished. Press R to reload."));PlaySound(TEXT("Pickup"),I->GetActorLocation());}else ShowMessage(TEXT("Reserve ready — ammunition available."));return true;}
 case EOBInteractionKind::Terminal:if(Phase==EOBPhase::Terminal&&!bTerminalComplete){HoldTarget=I;P->InteractionProgress=0;P->bInteractionActive=true;P->GetCharacterMovement()->StopMovementImmediately();return true;}break;
 case EOBInteractionKind::Pod:if(Phase==EOBPhase::BoardPod){HoldTarget=I;P->InteractionProgress=0;P->bInteractionActive=true;P->GetCharacterMovement()->StopMovementImmediately();return true;}ShowMessage(TEXT("Secure the bay before launch."));break;
 default:break;
 }return false;
}
void AOBMissionDirector::UpdateHold(AOBPlayerCharacter* P,AOBInteractable* Target,float DT){
 if(UGameplayStatics::IsGamePaused(this))return;
 if(!HoldTarget)return;
 if(!IsValid(P)||Target!=HoldTarget||!P->bInteractHeld||!ValidInteraction(P,HoldTarget)){CancelHold();return;}
 const float Duration=HoldTarget->Kind==EOBInteractionKind::Terminal?2.f:1.f;
 P->InteractionProgress=FMath::Min(1.f,P->InteractionProgress+DT/Duration);if(P->InteractionProgress<1)return;
 auto* Done=HoldTarget.Get();CancelHold();
 if(Done->Kind==EOBInteractionKind::Terminal&&Phase==EOBPhase::Terminal&&!bTerminalComplete){bTerminalComplete=true;Done->SetUsed();Phase=EOBPhase::Security;ShowMessage(TEXT("Override accepted. Clear the security response."));PlaySound(TEXT("Terminal"),Done->GetActorLocation());ActivateEncounter(4);}
 else if(Done->Kind==EOBInteractionKind::Pod&&Phase==EOBPhase::BoardPod&&!P->bDead){Done->bUsed=true;BeginEnding();}
}
void AOBMissionDirector::CancelHold(){HoldTarget=nullptr;if(Player){Player->InteractionProgress=0;Player->bInteractionActive=false;}}
void AOBMissionDirector::CheckPickups(){
 for(auto I:Interactions){
  if(!IsValid(I)||I->bUsed||FVector::DistSquared(Player->GetActorLocation(),I->GetActorLocation()+FVector(0,0,75))>FMath::Square(115.f))continue;
  bool Got=false;
  if(I->Kind==EOBInteractionKind::Medical)Got=Player->AddHealth(35);
  else if(I->Kind==EOBInteractionKind::Ammo)Got=Player->AddAmmo(48);
  if(Got){I->SetUsed();PlaySound(TEXT("Pickup"),I->GetActorLocation());}
 }
}
void AOBMissionDirector::BeginEnding(){
 if(Phase!=EOBPhase::BoardPod||!Player||Player->bDead)return;
 CancelBossIntro();
 EndingPod=nullptr;for(auto I:Interactions)if(IsValid(I)&&I->Kind==EOBInteractionKind::Pod){EndingPod=I;break;}
 if(!EndingPod){UE_LOG(LogTemp,Error,TEXT("Orbit ending missing escape pod"));ShowMessage(TEXT("Escape-pod data requires repair."));return;}
 CancelHold();Player->CancelActions();Player->SetActorEnableCollision(false);Player->SetActorHiddenInGame(true);Player->GetCharacterMovement()->DisableMovement();Phase=EOBPhase::Ending;AttackSlots.Empty();EndingElapsed=0;
 EndingPod->SetActorEnableCollision(false);
 // Only transient projectiles carry this tag; level geometry is untouched.
 TArray<AActor*> Bolts;UGameplayStatics::GetAllActorsWithTag(this,TEXT("OB_PROJECTILE"),Bolts);for(auto* B:Bolts)B->Destroy();
 const FVector PodPosition=EndingPod->GetActorLocation();
 const FVector CameraPosition=PodPosition+FVector(1100,850,500);
 const FVector Focus=PodPosition+FVector(400,0,240);
 EndingCamera=GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),CameraPosition,(Focus-CameraPosition).Rotation());
 if(IsValid(EndingCamera))if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetViewTargetWithBlend(EndingCamera,.5f);
 PlaySound(TEXT("Launch"),Player->GetActorLocation());ShowMessage(TEXT("Star Sparrow released. You are clear of the Helios Ark."),9);
 UE_LOG(LogTemp,Display,TEXT("OB_ENDING_STARTED"));
}
void AOBMissionDirector::CompleteMission(){
 if(Phase!=EOBPhase::Ending)return;
 if(auto* GI=GetGameInstance<UOBGameInstance>()){
  const bool bRequested=GI->TransitionToLevel2(Player);Phase=EOBPhase::Complete;
  if(!bRequested){GI->bFrontEnd=true;ShowMessage(GI->SaveWarning,10.f);if(auto* PC=Cast<AOBPlayerController>(UGameplayStatics::GetPlayerController(this,0)))PC->ShowScreen(TEXT("Main"));}
  UE_LOG(LogTemp,Display,TEXT("OB_LEVEL1_ESCAPE_COMPLETE transition=%d"),bRequested);
 }
}
void AOBMissionDirector::PlaySound(FName Cue,FVector Location,float Volume,bool IsAmbience){
 auto* Sound=Sounds.Find(Cue);if(!Sound||!*Sound)return;auto* GI=GetGameInstance<UOBGameInstance>();float Gain=Volume;if(GI&&GI->Settings)Gain*=GI->Settings->MasterVolume*(IsAmbience?GI->Settings->AmbienceVolume:GI->Settings->EffectsVolume);
 if(Player)Gain*=FMath::Clamp(1.f-FVector::Dist(Player->GetActorLocation(),Location)/6000.f,.05f,1.f);
 if(Gain>.001f)UGameplayStatics::PlaySoundAtLocation(this,*Sound,Location,Gain);
}
int32 AOBMissionDirector::GetRemainingEnemies() const {int32 Count=0;for(auto E:Enemies)if(E&&E->EncounterId==ActiveEncounter&&!E->IsDead())Count++;return Count;}
bool AOBMissionDirector::ValidateContent(FString& Errors) const {
 TSet<FName> Ids;int32 Counts[7]={};int32 HeavyCount=0;for(auto E:Enemies){if(!IsValid(E))continue;if(E->StableId.IsNone()||Ids.Contains(E->StableId))Errors+=TEXT("duplicate enemy ID; ");Ids.Add(E->StableId);if(E->EncounterId>=1&&E->EncounterId<=6)Counts[E->EncounterId]++;else Errors+=TEXT("bad encounter ID; ");if(E->bHeavy){++HeavyCount;if(E->EncounterId!=5)Errors+=TEXT("sentinel outside reactor; ");}}
 const int32 Expect[]={0,3,3,2,3,1,4};for(int i=1;i<=6;i++)if(Counts[i]!=Expect[i])Errors+=FString::Printf(TEXT("E%d expected %d got %d; "),i,Expect[i],Counts[i]);
 if(HeavyCount!=1)Errors+=TEXT("expected one sentinel; ");
 TSet<int32> GateIds;for(auto D:Doors)if(IsValid(D)){if(GateIds.Contains(D->DoorId))Errors+=TEXT("duplicate gate; ");GateIds.Add(D->DoorId);}for(int32 Id=0;Id<7;++Id)if(!GateIds.Contains(Id))Errors+=FString::Printf(TEXT("missing gate D%02d; "),Id);if(GateIds.Num()!=7)Errors+=TEXT("expected seven gates; ");
 int32 Weapons=0,HeavyWeapons=0,Terminals=0,Pods=0;TSet<FName> InteractionIds;
 for(auto I:Interactions)if(IsValid(I)){
  if(I->StableId.IsNone()||InteractionIds.Contains(I->StableId))Errors+=TEXT("duplicate interaction ID; ");InteractionIds.Add(I->StableId);
  if(I->Kind==EOBInteractionKind::Weapon)++Weapons;else if(I->Kind==EOBInteractionKind::HeavyWeapon){++HeavyWeapons;if(I->Zone!=5)Errors+=TEXT("HELIX-12 outside reactor entrance; ");}else if(I->Kind==EOBInteractionKind::Terminal)++Terminals;else if(I->Kind==EOBInteractionKind::Pod)++Pods;
 }
 if(Weapons!=1||HeavyWeapons!=1||Terminals!=1||Pods!=1)Errors+=TEXT("expected ARC-9, HELIX-12, terminal, and escape pod; ");
 for(int Zone=2;Zone<=6;Zone++){bool Station=false;for(auto I:Interactions)if(I&&I->Zone==Zone&&I->Kind==EOBInteractionKind::AmmoStation)Station=true;if(!Station)Errors+=FString::Printf(TEXT("Z%d missing station; "),Zone);}
 return Errors.IsEmpty();
}
AOBGameMode::AOBGameMode(){DefaultPawnClass=AOBPlayerCharacter::StaticClass();PlayerControllerClass=AOBPlayerController::StaticClass();HUDClass=AOBHUD::StaticClass();}
