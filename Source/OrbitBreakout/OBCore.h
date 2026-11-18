#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/SaveGame.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Actor.h"
#include "OBCore.generated.h"

class AOBPlayerCharacter; class AOBEnemyCharacter; class AOBMissionDirector; class APlayerController; class UStaticMeshComponent; class UBoxComponent; class UTextRenderComponent; class USoundBase; class UAudioComponent;
UENUM(BlueprintType) enum class EOBPhase:uint8 { Cell, Corridor, Cargo, Terminal, Security, Reactor, PodBay, BoardPod, Ending, Complete };
UENUM(BlueprintType) enum class EOBInteractionKind:uint8 { Weapon, AmmoStation, Terminal, Pod, Medical, Ammo, HeavyWeapon };

UCLASS() class ORBITBREAKOUT_API UOBSettings:public USaveGame {
 GENERATED_BODY()
public:
 UPROPERTY() int32 SchemaVersion=1;
 UPROPERTY() float ADSSensitivity=.8f;
 UPROPERTY() int32 TraversalMotion=1;
 UPROPERTY() float WallRoll=6.f;
 UPROPERTY() float MouseSensitivity=1.f;
 UPROPERTY() bool bInvertVerticalLook=false;
 UPROPERTY() float MasterVolume=.8f;
 UPROPERTY() float EffectsVolume=.85f;
 UPROPERTY() float AmbienceVolume=.45f;
 UPROPERTY() int32 GraphicsPreset=1;
 UPROPERTY() float CameraShakeScale=.35f;
 UPROPERTY() bool bReducedFlashes=false;
 void Clamp();
};
UCLASS() class ORBITBREAKOUT_API UOBMissionSave:public USaveGame {
 GENERATED_BODY()
public:
 UPROPERTY() int32 SchemaVersion=2;
 UPROPERTY() FName MissionId="EmergencyDeparture";
 UPROPERTY() int32 CheckpointId=0;
 UPROPERTY() bool bMissionCompleted=false;
 UPROPERTY() int32 ReserveAmmo=0;
 // Additive schema-1 fields: older checkpoints deserialize to an unclaimed rack.
 UPROPERTY() bool bHelixOwned=false;
 UPROPERTY() int32 HelixReserveAmmo=0;
 // Schema 2 is additive: absent legacy fields migrate to Chapter 1 explicitly.
 UPROPERTY() int32 ChapterId=1;
 UPROPERTY() FName CheckpointKey="L1.Cell";
 UPROPERTY() bool bPendingChapter2=false;
 UPROPERTY() bool bArcOwned=false;
 UPROPERTY() int32 ArcMagazine=0;
 UPROPERTY() int32 HelixMagazine=0;
 UPROPERTY() uint8 SelectedWeapon=0;
 UPROPERTY() bool bLevel2IntroComplete=false;
 UPROPERTY() bool bJunctionIsolated=false;
 UPROPERTY() bool bJunctionDrained=false;
 UPROPERTY() bool bCouplerOwned=false;
 UPROPERTY() bool bCouplerInserted=false;
 UPROPERTY() bool bAuxPowerRestored=false;
 UPROPERTY() bool bWardenDefeated=false;
 UPROPERTY() bool bStabilizerA=false;
 UPROPERTY() bool bStabilizerB=false;
 UPROPERTY() bool bLevel2Completed=false;
 bool IsValidSave() const;
 bool MigrateLegacy();
 void SetCheckpointIdentity(int32 Id);

};
UCLASS() class ORBITBREAKOUT_API UOBGameInstance:public UGameInstance {
 GENERATED_BODY()
public:
 virtual void Init() override;
 virtual void Shutdown() override;
 UPROPERTY() TObjectPtr<UOBSettings> Settings;
 UPROPERTY() TObjectPtr<UOBMissionSave> MissionSave;
 UPROPERTY() bool bFrontEnd=true;
 UPROPERTY() bool bSaveValid=false;
 UPROPERTY() FString SaveWarning;
 UFUNCTION(BlueprintCallable) bool HasValidSave() const;
 UFUNCTION(BlueprintCallable) void NewMission();
 UFUNCTION(BlueprintCallable) void ContinueMission();
 UFUNCTION(BlueprintCallable) void ReturnToMenu();
 UFUNCTION(BlueprintCallable) void SaveSettings();
 UFUNCTION(BlueprintCallable) void ApplySettings();
 bool SaveCheckpoint();
 void ReloadMission();
 UFUNCTION(BlueprintCallable) bool TransitionToLevel2(AOBPlayerCharacter* Player);
 /** Called by the Level 2 director only after the destination world exists. */
 UFUNCTION(BlueprintCallable) void BeginLevel2Session();
 UFUNCTION(BlueprintCallable) void ReplayLevel2();
 UPROPERTY(BlueprintReadOnly) bool bDevelopmentChapterSession=false;
 UPROPERTY(BlueprintReadOnly) bool bChapterTravelRequested=false;
private:
 UPROPERTY(Transient) TObjectPtr<UOBMissionSave> CampaignSaveSnapshot;
 bool bCampaignSaveSnapshotValid=false;
 FDelegateHandle TravelFailureHandle;
 void RestoreCampaignSession();
 void OnChapterTravelFailure(UWorld* World,ETravelFailure::Type Failure,const FString& Error);

};

UCLASS() class ORBITBREAKOUT_API AOBDoor:public AActor {
 GENERATED_BODY()
public:
 AOBDoor();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Orbit") int32 DoorId=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Orbit") FVector PanelSize=FVector(60,300,320);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Orbit") float OpenDistance=340;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Panel;
 virtual void OnConstruction(const FTransform&) override;
 virtual void BeginPlay() override;
 virtual void Tick(float) override;
 void Open(bool bInstant=false);
 bool bOpen=false;
private: FVector ClosedPosition; float OpenAlpha=0;
};

UCLASS() class ORBITBREAKOUT_API AOBInteractable:public AActor {
 GENERATED_BODY()
public:
 AOBInteractable();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Orbit") EOBInteractionKind Kind=EOBInteractionKind::Weapon;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Orbit") FName StableId="Interaction";
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Orbit") int32 Zone=1;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Glow;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WeaponDisplay;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WeaponDisplayRail;
 virtual void OnConstruction(const FTransform&) override;
 virtual void BeginPlay() override;
 virtual FString GetPrompt() const;
 bool bUsed=false; float CooldownUntil=0;
 void SetUsed();
};

UCLASS() class ORBITBREAKOUT_API AOBZoneTrigger:public AActor {
 GENERATED_BODY()
public:
 AOBZoneTrigger();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Orbit") int32 Zone=2;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Orbit") FVector TriggerExtent=FVector(120,200,150);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Orbit") bool bCheckpoint=false;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Box;
 virtual void OnConstruction(const FTransform&) override;
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 UFUNCTION() void Overlap(UPrimitiveComponent* OverlappedComponent,AActor* OtherActor,UPrimitiveComponent* OtherComponent,int32 OtherBodyIndex,bool bFromSweep,const FHitResult& SweepResult);
private:
 UPROPERTY() TWeakObjectPtr<AOBMissionDirector> Director;
};

UCLASS() class ORBITBREAKOUT_API AOBMissionDirector:public AActor {
 GENERATED_BODY()
public:
 AOBMissionDirector();
 UPROPERTY(EditAnywhere,Category="Orbit|Traversal") bool bEnableParkour=true;
 static AOBMissionDirector* Get(const UObject* Context);
 virtual void BeginPlay() override;
 virtual void Tick(float) override;
 virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Orbit|Checkpoints") FVector StartLocation=FVector(-450,0,100);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Orbit|Checkpoints") FVector CargoCheckpointLocation=FVector(4900,0,100);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Orbit|Checkpoints") FVector ReactorCheckpointLocation=FVector(11900,0,100);
 UPROPERTY(BlueprintReadOnly) EOBPhase Phase=EOBPhase::Cell;
 UPROPERTY(BlueprintReadOnly) bool bTerminalComplete=false;
 UPROPERTY(BlueprintReadOnly) FString SystemMessage;
 UPROPERTY(BlueprintReadOnly) float SystemMessageUntil=0;
 UPROPERTY(BlueprintReadOnly) TArray<int32> ClearedEncounters;
 UPROPERTY(BlueprintReadOnly) int32 ActiveEncounter=0;
 UPROPERTY() TObjectPtr<AOBPlayerCharacter> Player;
 UPROPERTY() TArray<TObjectPtr<AOBEnemyCharacter>> Enemies;
 UPROPERTY() TArray<TObjectPtr<AOBDoor>> Doors;
 UPROPERTY() TArray<TObjectPtr<AOBInteractable>> Interactions;
 UPROPERTY() TObjectPtr<AOBInteractable> HoldTarget;
 virtual bool IsCombatAllowed() const;
 virtual bool IsGameplayActive() const;
 UFUNCTION(BlueprintPure, Category="Orbit|Boss Intro") bool IsBossIntroActive() const { return bBossIntroActive; }
 UFUNCTION(BlueprintPure, Category="Orbit|Boss Intro") bool IsBossIntroReturning() const { return bBossIntroReturning; }
 UFUNCTION(BlueprintPure, Category="Orbit|Boss Intro") float GetBossIntroElapsed() const { return BossIntroElapsed; }
 UFUNCTION(BlueprintCallable, Category="Orbit|Boss Intro") bool BeginBossIntro(AOBEnemyCharacter* Boss);
 UFUNCTION(BlueprintCallable, Category="Orbit|Boss Intro") void CancelBossIntro();
 virtual bool TryAcquireAttackSlot(AActor* Enemy);
 virtual void ReleaseAttackSlot(AActor* Enemy);
 virtual void ResolveEnemy(int32 EncounterId,FName StableId);
 void ActivateEncounter(int32 EncounterId);
 virtual void EnterZone(int32 Zone,bool bCheckpoint);
 virtual void CommitCheckpoint(int32 Id);
 virtual void RetryCheckpoint();
 virtual void PlayerDied();
 void WeaponAcquired();
 void BeginEnding();
 virtual void CompleteMission();
 virtual FString GetObjectiveText() const;
 void ShowMessage(const FString&,float Seconds=4);
 virtual AOBInteractable* FindInteraction(AOBPlayerCharacter*) const;
 virtual bool TryInteract(AOBPlayerCharacter*,AOBInteractable*);
 virtual void UpdateHold(AOBPlayerCharacter*,AOBInteractable*,float DeltaSeconds);
 virtual void CancelHold();
 virtual void PlaySound(FName Cue,FVector Location,float Volume=1.f,bool bAmbience=false);
 float GetEndingElapsed() const { return EndingElapsed; }
 virtual int32 GetRemainingEnemies() const;
 virtual bool ValidateContent(FString& Errors) const;
private:
 TSet<TWeakObjectPtr<AActor>> AttackSlots;
 TMap<int32,TSet<FName>> Resolved;
 TMap<int32,TSet<FName>> ExpectedRoster;
 TSet<int32> Activated;
 bool bInitialized=false; bool bDeathHandled=false; bool bCellDoorReleased=false;
 float EndingElapsed=0; float CellReleaseTime=0;
 FTimerHandle CargoWaveTimer, DeathTimer;
 UPROPERTY() TObjectPtr<AActor> EndingCamera;
 UPROPERTY(Transient) TObjectPtr<AActor> BossIntroCamera;
 UPROPERTY(Transient) TWeakObjectPtr<AActor> BossIntroPreviousView;
 UPROPERTY(Transient) TWeakObjectPtr<AOBEnemyCharacter> IntroBoss;
 UPROPERTY(Transient) TWeakObjectPtr<APlayerController> BossIntroController;
 bool bBossIntroActive=false;
 bool bBossIntroReturning=false;
 float BossIntroElapsed=0.f;
 float BossIntroReturnElapsed=0.f;
 uint8 BossIntroPreviousMovement=0;
 uint8 BossIntroPreviousCustomMovement=0;
 bool bBossIntroHeldMovement=false;
 UPROPERTY() TObjectPtr<AOBInteractable> EndingPod;
 UPROPERTY() TObjectPtr<UAudioComponent> Ambience;
 UPROPERTY() TMap<FName,TObjectPtr<USoundBase>> Sounds;
 void InitializeMission();
 void RefreshDoors(bool bInstant=false);
 bool ValidInteraction(AOBPlayerCharacter*,AOBInteractable*) const;
 void CheckPickups();
 float PickupAccumulator=0;
 void TickBossIntro(float DeltaSeconds);
 void FinishBossIntro(bool bResumeCombat);
 bool OwnsBossIntroView() const;
#if WITH_DEV_AUTOMATION_TESTS
    friend class FOBBossMechanicsTest;
 friend class FOBSoloBossGateTest;
 friend class FOBBossIntroTest;
#endif
};

UCLASS() class ORBITBREAKOUT_API AOBGameMode:public AGameModeBase {
 GENERATED_BODY()
public: AOBGameMode();
};
