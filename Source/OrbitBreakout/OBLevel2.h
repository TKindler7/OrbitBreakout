#pragma once
#include "CoreMinimal.h"
#include "OBCore.h"
#include "OBLevel2.generated.h"

class AOBMachineEnemy; class ULevelSequence; class ULevelSequencePlayer; class ALevelSequenceActor;
class UMaterialInstanceDynamic; class UAudioComponent;
class ACameraActor;

UENUM(BlueprintType) enum class EOBLevel2State:uint8 {
 CrashIntro, ReachJunction, RestoreJunction, CrossFreight, RestoreAuxPower, DefeatWarden,
 ActivateStabilizers, TransmitSignal, ClearFinalHostiles, SignalReceived
};
UENUM(BlueprintType) enum class EOBLevel2Action:uint8 {
 Breaker, Pump, Coupler, Socket, Power, StabilizerA, StabilizerB, Transmitter, Supply, Medical, Log
};
UENUM(BlueprintType) enum class EOBLevel2HazardKind:uint8 { LiveWater, PressureVent };

UCLASS() class ORBITBREAKOUT_API AOBLevel2Interactable:public AOBInteractable {
 GENERATED_BODY()
public:
 AOBLevel2Interactable();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dead Signal") EOBLevel2Action Action=EOBLevel2Action::Breaker;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dead Signal") float HoldSeconds=2.f;
 virtual void OnConstruction(const FTransform&) override;
 virtual FString GetPrompt() const override;
 void SetStatus(bool bComplete,bool bAvailable,bool bFault=false);
};

UCLASS() class ORBITBREAKOUT_API AOBLevel2Hazard:public AActor {
 GENERATED_BODY()
public:
 AOBLevel2Hazard();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dead Signal") EOBLevel2HazardKind HazardKind=EOBLevel2HazardKind::LiveWater;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dead Signal") FVector Extent=FVector(250,250,40);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dead Signal") float CycleOffset=0;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Area;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Warning;
 virtual void OnConstruction(const FTransform&) override;
 virtual void Tick(float) override;
 bool IsDangerous() const;
private:
 float LastDamage=-100,LastCueCycle=-100;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WarningMID;
};

/** Chapter two owns its mission state; Level one's director and encounters remain intact. */
UCLASS() class ORBITBREAKOUT_API AOBLevel2Director:public AOBMissionDirector {
 GENERATED_BODY()
public:
 AOBLevel2Director();
 UPROPERTY(Transient) TObjectPtr<class AOBMech> WardenMech;
 virtual void BeginPlay() override;
 virtual void Tick(float) override;
 virtual void EndPlay(const EEndPlayReason::Type) override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dead Signal|Checkpoints") FVector JunctionCheckpoint=FVector(6500,3500,110);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dead Signal|Checkpoints") FVector PowerCheckpoint=FVector(13000,7000,110);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dead Signal|Checkpoints") FVector FinaleCheckpoint=FVector(18900,9500,110);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dead Signal|Cinematics") TObjectPtr<ULevelSequence> CrashSequence;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dead Signal|Cinematics") TObjectPtr<ULevelSequence> RescueSequence;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dead Signal|Cinematics") float CrashDuration=42.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dead Signal|Cinematics") float RescueDuration=15.f;
 UPROPERTY(BlueprintReadOnly) EOBLevel2State MissionState=EOBLevel2State::CrashIntro;
 UPROPERTY(BlueprintReadOnly) bool bJunctionIsolated=false;
 UPROPERTY(BlueprintReadOnly) bool bJunctionDrained=false;
 UPROPERTY(BlueprintReadOnly) bool bCouplerOwned=false;
 UPROPERTY(BlueprintReadOnly) bool bCouplerInserted=false;
 UPROPERTY(BlueprintReadOnly) bool bAuxPowerRestored=false;
 UPROPERTY(BlueprintReadOnly) bool bWardenDefeated=false;
 UPROPERTY(BlueprintReadOnly) bool bStabilizerA=false;
 UPROPERTY(BlueprintReadOnly) bool bStabilizerB=false;
 UPROPERTY(BlueprintReadOnly) bool bOverloadTriggered=false;
 UPROPERTY(BlueprintReadOnly) bool bOverloadRepaired=false;
 UPROPERTY(BlueprintReadOnly) float UplinkSeconds=0;
 UPROPERTY(BlueprintReadOnly) int32 CurrentZone=1;
 UPROPERTY(BlueprintReadOnly) int32 Checkpoint=0;
 UPROPERTY(BlueprintReadOnly) int32 FinaleSpawned=0;
 UPROPERTY(BlueprintReadOnly) TArray<TObjectPtr<AOBMachineEnemy>> Machines;
 virtual bool IsGameplayActive() const override;
 virtual bool IsCombatAllowed() const override;
 virtual bool TryAcquireAttackSlot(AActor*) override;
 virtual void ReleaseAttackSlot(AActor*) override;
 virtual void ResolveEnemy(int32,FName) override;
 UFUNCTION(BlueprintCallable) virtual void EnterZone(int32 Zone,bool bCheckpoint) override;
 UFUNCTION(BlueprintCallable) virtual void CommitCheckpoint(int32 Id) override;
 UFUNCTION(BlueprintCallable) virtual void RetryCheckpoint() override;
 virtual void PlayerDied() override;
 virtual void CompleteMission() override;
 virtual FString GetObjectiveText() const override;
 virtual AOBInteractable* FindInteraction(AOBPlayerCharacter*) const override;
 UFUNCTION(BlueprintCallable) virtual bool TryInteract(AOBPlayerCharacter* InPlayer,AOBInteractable* Interaction) override;
 UFUNCTION(BlueprintCallable) virtual void UpdateHold(AOBPlayerCharacter* InPlayer,AOBInteractable* Interaction,float DeltaSeconds) override;
 virtual void CancelHold() override;
 virtual int32 GetRemainingEnemies() const override;
 virtual bool ValidateContent(FString&) const override;
 virtual void PlaySound(FName,FVector,float Volume=1.f,bool bAmbience=false) override;
 UFUNCTION(BlueprintPure) bool IsCinematicActive() const { return bCinematic; }
 UFUNCTION(BlueprintPure) float GetCinematicElapsed() const { return CinematicElapsed; }
 UFUNCTION(BlueprintPure) float GetCinematicDuration() const { return bRescueCinematic?RescueDuration:CrashDuration; }
 UFUNCTION(BlueprintPure) FString GetSubtitle() const;
 UFUNCTION(BlueprintCallable) void SetSkipHeld(bool bHeld);
 UFUNCTION(BlueprintPure) float GetSkipProgress() const { return SkipHeldSeconds; }
 UFUNCTION(BlueprintPure) float GetUplinkProgress() const { return UplinkSeconds/75.f; }
 UFUNCTION(BlueprintPure) bool IsUplinkActive() const { return MissionState==EOBLevel2State::TransmitSignal || MissionState==EOBLevel2State::ClearFinalHostiles; }
 UFUNCTION(BlueprintPure) bool IsUplinkPaused() const { return bOverloadTriggered&&!bOverloadRepaired; }
 UFUNCTION(BlueprintPure) FVector GetObjectiveLocation() const;
 UFUNCTION(BlueprintPure) FString GetChapterLabel() const { return TEXT("LEVEL 2 — DEAD SIGNAL"); }
 UFUNCTION(BlueprintPure) bool IsActionAvailable(EOBLevel2Action Action) const;
 UFUNCTION(BlueprintPure) bool IsActionComplete(EOBLevel2Action Action) const;
 UFUNCTION(BlueprintCallable) void FinishCinematic();
 UFUNCTION(BlueprintPure) int32 CountEncounter(int32 Id) const;
 UFUNCTION(BlueprintPure) bool IsEncounterCleared(int32 Id) const;
private:
 bool bReady=false,bCinematic=false,bRescueCinematic=false,bSkipHeld=false,bDeadHandled=false,bCompletionSaved=false;
 float CinematicElapsed=0,SkipHeldSeconds=0,HoldElapsed=0,HoldDamageTime=-100,SafeUntil=0,SpawnAccumulator=0,DeathElapsed=0;
 int32 LastCinematicBeat=-1;
 TSet<int32> ScheduledEncounters;
 TSet<FName> ResolvedMachines;
 TSet<TWeakObjectPtr<AActor>> MachineAttackSlots;
 UPROPERTY() TArray<TObjectPtr<AOBMachineEnemy>> SpawnQueue;
 UPROPERTY() TObjectPtr<ULevelSequencePlayer> SequencePlayer;
 UPROPERTY() TObjectPtr<ALevelSequenceActor> SequenceActor;
 UPROPERTY(Transient) TObjectPtr<ACameraActor> CameraBridge;
 bool bCrashPlayerRevealed=false;
 UPROPERTY() TObjectPtr<AActor> Wreck;
 FTransform WreckFinalTransform;
 UPROPERTY() TMap<FName,TObjectPtr<USoundBase>> ChapterSounds;
 UPROPERTY() TObjectPtr<UAudioComponent> ChapterAmbience;
 UPROPERTY() TArray<TObjectPtr<AActor>> PowerActors;
 UPROPERTY() TArray<TObjectPtr<AActor>> PowerFans;
 UPROPERTY() TArray<TObjectPtr<AActor>> SpawnDoors;
 TMap<TWeakObjectPtr<AActor>,float> SpawnNotBefore;
 TMap<int32,TSet<FName>> EncounterRoster;
 FTimerHandle InitializeTimer;
 FTimerHandle CameraBridgeTimer;
 void InitializeChapter();
 void BeginCinematic(bool bRescue);
 void RestoreWorld(int32 CheckpointId);
 void QueueEncounter(int32 Id);
 void PumpSpawnQueue(float);
 void UpdateProgression();
 void CompleteAction(AOBLevel2Interactable*);
 void RefreshWorld();
 void WriteSaveFlags();
 void SnapshotLoadout();
 bool CanReach(AOBPlayerCharacter*,AOBInteractable*) const;
 AOBLevel2Interactable* FindAction(EOBLevel2Action) const;
 void ClearProjectiles();
 void StopCinematic(bool bBlendToPlayer=false);
 void UpdateCrashPresentation();
 void GroundCrashPlayer();
 void CleanupCameraBridge();
 void ReplenishAmmo(int32 ArcReserve,int32 HelixReserve);
 bool CanSpawnAt(AOBMachineEnemy* Machine) const;
 int32 AliveFinaleMachines() const;
 void SetCarrierColor(FLinearColor Color);
#if WITH_DEV_AUTOMATION_TESTS
 friend class FOBPilotUpgradeTest;
 friend class FOBLevel2MissionRulesTest;
 friend class FOBCrashPresentationTest;
#endif
};
