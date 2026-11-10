#include "OBRifleAnimInstance.h"
#include "OBPlayerCharacter.h"
#include "OBEnemyCharacter.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
UAnimSequence* Sequence(const FString& Path)
{
    ConstructorHelpers::FObjectFinder<UAnimSequence> Asset(*Path);
    return Asset.Object;
}

// These assets share the mannequin skeleton and an authored rifle grip. Sampling
// them directly avoids an animation Blueprint that depends on a template pawn.
struct FOBRifleAnimProxy final : FAnimInstanceProxy
{
    explicit FOBRifleAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

    TArray<UAnimSequence*> Walk, Jog;
    UAnimSequence* Idle = nullptr;
    UAnimSequence* Reload = nullptr;
    UAnimSequence* Fire = nullptr;
    UAnimSequence* AimUp = nullptr;
    UAnimSequence* AimDown = nullptr;
    UAnimSequence* Fall = nullptr;
    UAnimSequence* Death = nullptr;
    float Speed = 0.f, Direction = 0.f, Phase = 0.f, Clock = 0.f;
    float MoveWeight = 0.f, JogWeight = 0.f, FallingWeight = 0.f;
    float Pitch = 0.f, ReloadPosition = 0.f, ReloadWeight = 0.f;
    float SinceShot = 100.f, DeathTime = 0.f;
    bool bArmed = false, bDead = false;
    FQuat FallbackAimRotation = FQuat::Identity;

    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
        auto* Anim = CastChecked<UOBRifleAnimInstance>(Instance);
        auto* Character = Cast<ACharacter>(Instance->TryGetPawnOwner());
        if (!Character) return;
        const FVector LocalVelocity = Character->GetActorTransform().InverseTransformVectorNoScale(Character->GetVelocity());
        Speed = LocalVelocity.Size2D();
        if (Speed > 3.f) Direction = FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X));
        float DesiredPitch = 0.f;
        bool Reloading = false;
        bool Dead = false;
        bArmed = true;
        SinceShot = 100.f;
        if (const auto* Player = Cast<AOBPlayerCharacter>(Character))
        {
            bArmed = Player->bWeaponOwned;
            Reloading = Player->bReloading;
            // Hold the final reload sample while the upper-body layer fades out.
            if (Reloading) ReloadPosition = Player->ReloadProgress;
            Dead = Player->bDead;
            SinceShot = Character->GetWorld()->GetTimeSeconds() - Player->LastShotTime;
            if (bArmed && Character->GetController() && (Player->bAiming || SinceShot < .35f))
                DesiredPitch = FRotator::NormalizeAxis(Character->GetControlRotation().Pitch);
        }
        else if (const auto* Enemy = Cast<AOBEnemyCharacter>(Character))
        {
            Dead = Enemy->IsDead();
            DesiredPitch = Enemy->GetVisualAimPitch();
            SinceShot = Character->GetWorld()->GetTimeSeconds() - Enemy->LastShotTime;
        }
        DeathTime = Dead ? (bDead ? DeathTime + DeltaSeconds : 0.f) : 0.f;
        bDead = Dead;
        Clock += DeltaSeconds;
        MoveWeight = FMath::FInterpTo(MoveWeight, FMath::Clamp(Speed / 90.f, 0.f, 1.f), DeltaSeconds, 12.f);
        JogWeight = FMath::FInterpTo(JogWeight, FMath::Clamp((Speed - 180.f) / 230.f, 0.f, 1.f), DeltaSeconds, 8.f);
        FallingWeight = FMath::FInterpTo(FallingWeight, Character->GetCharacterMovement()->IsFalling() ? 1.f : 0.f, DeltaSeconds, 12.f);
        ReloadWeight = FMath::FInterpTo(ReloadWeight, Reloading ? 1.f : 0.f, DeltaSeconds, 20.f);
        Pitch = FMath::FInterpTo(Pitch, FMath::Clamp(DesiredPitch, -65.f, 65.f), DeltaSeconds, 16.f);
        Idle = bArmed ? Anim->RifleIdle.Get() : Anim->UnarmedIdle.Get();
        Walk.Reset(); Jog.Reset();
        const auto& WalkAssets = bArmed ? Anim->RifleWalk : Anim->UnarmedWalk;
        const auto& JogAssets = bArmed ? Anim->RifleJog : Anim->UnarmedJog;
        for (const auto& Asset : WalkAssets) Walk.Add(Asset.Get());
        for (const auto& Asset : JogAssets) Jog.Add(Asset.Get());
        Reload = Anim->RifleReload; Fire = Anim->RifleFire;
        AimUp = Anim->RifleAimUp; AimDown = Anim->RifleAimDown;
        Fall = bArmed ? Anim->RifleFall.Get() : Anim->UnarmedFall.Get();
        Death = Anim->Death;
        const float WalkLength = Walk.Num() && Walk[0] ? Walk[0]->GetPlayLength() : 1.f;
        const float JogLength = Jog.Num() && Jog[0] ? Jog[0]->GetPlayLength() : .75f;
        const float Rate = FMath::Lerp(FMath::Clamp(Speed / 180.f, .5f, 1.5f) / FMath::Max(.1f, WalkLength),
            FMath::Clamp(Speed / 450.f, .5f, 1.5f) / FMath::Max(.1f, JogLength), JogWeight);
        if (Speed > 3.f) Phase = FMath::Fmod(Phase + DeltaSeconds * Rate, 1.f);
        const FQuat ActorRotation = Character->GetActorQuat();
        const FQuat WorldAim = ActorRotation * FRotator(Pitch, 0, 0).Quaternion() * ActorRotation.Inverse();
        const FQuat MeshRotation = Character->GetMesh()->GetComponentQuat();
        FallbackAimRotation = MeshRotation.Inverse() * WorldAim * MeshRotation;
        Anim->GroundSpeed = Speed; Anim->AimPitch = Pitch; Anim->bArmed = bArmed;
    }

    void Sample(UAnimSequence* Asset, float Position, bool Loop, FPoseContext& Output) const
    {
        Output.ResetToRefPose();
        if (!Asset) return;
        const float Length = FMath::Max(.001f, Asset->GetPlayLength());
        const float Time = Loop ? FMath::Fmod(FMath::Max(0.f, Position), Length) : FMath::Clamp(Position, 0.f, Length);
        FAnimationPoseData Data(Output);
        const FAnimExtractContext Extraction(Time, false, {}, Loop);
        if (Asset->IsValidAdditive() && !Output.ExpectsAdditivePose())
        {
            Asset->GetAdditiveBasePose(Data, Extraction);
            FPoseContext AdditivePose(Output, true);
            FAnimationPoseData AdditiveData(AdditivePose);
            Asset->GetAnimationPose(AdditiveData, Extraction);
            FAnimationRuntime::AccumulateAdditivePose(Data, AdditiveData, 1.f, Asset->GetAdditiveAnimType());
            Output.Pose.NormalizeRotations();
        }
        else Asset->GetAnimationPose(Data, Extraction);
    }

    void Blend(FPoseContext& A, FPoseContext& B, float WeightB, FPoseContext& Output) const
    {
        FAnimationPoseData AData(A), BData(B), Result(Output);
        FAnimationRuntime::BlendTwoPosesTogether(AData, BData, 1.f - WeightB, Result);
    }

    void Directional(const TArray<UAnimSequence*>& Assets, FPoseContext& Output)
    {
        if (Assets.Num() != 8) { Sample(Idle, Clock, true, Output); return; }
        const float Octant = FMath::Fmod(Direction + 360.f, 360.f) / 45.f;
        const int32 First = FMath::FloorToInt(Octant) % 8;
        const int32 Second = (First + 1) % 8;
        FPoseContext A(this), B(this);
        Sample(Assets[First], Assets[First] ? Phase * Assets[First]->GetPlayLength() : 0.f, true, A);
        Sample(Assets[Second], Assets[Second] ? Phase * Assets[Second]->GetPlayLength() : 0.f, true, B);
        Blend(A, B, Octant - FMath::FloorToFloat(Octant), Output);
    }

    void UpperBody(FPoseContext& Overlay, float Weight, FPoseContext& Output)
    {
        FPoseContext Base(Output);
        Base = Output;
        TArray<float> BoneWeights;
        BoneWeights.SetNumZeroed(Output.Pose.GetNumBones());
        const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
        const int32 Spine = Bones.GetPoseBoneIndexForBoneName(TEXT("spine_01"));
        if (Spine == INDEX_NONE) return;
        for (FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
        {
            int32 Parent = Bones.MakeMeshPoseIndex(Index).GetInt();
            while (Parent != INDEX_NONE && Parent != Spine) Parent = Bones.GetReferenceSkeleton().GetParentIndex(Parent);
            if (Parent == Spine) BoneWeights[Index.GetInt()] = Weight;
        }
        FAnimationPoseData BaseData(Base), OverlayData(Overlay), Result(Output);
        FAnimationRuntime::BlendTwoPosesTogetherPerBone(BaseData, OverlayData, BoneWeights, Result);
    }

    virtual bool Evaluate(FPoseContext& Output) override
    {
        if (bDead && Death)
        {
            Sample(Death, DeathTime, false, Output);
            return true;
        }
        FPoseContext IdlePose(this), WalkPose(this), JogPose(this), MovePose(this);
        Sample(Idle, Clock, true, IdlePose);
        Directional(Walk, WalkPose);
        Directional(Jog, JogPose);
        Blend(WalkPose, JogPose, JogWeight, MovePose);
        Blend(IdlePose, MovePose, MoveWeight, Output);
        if (FallingWeight > .001f && Fall)
        {
            FPoseContext Air(this), Ground(Output);
            Ground = Output;
            Sample(Fall, Clock, true, Air);
            Blend(Ground, Air, FallingWeight, Output);
        }
        if (bArmed && ReloadWeight > .001f && Reload)
        {
            FPoseContext ReloadPose(this);
            Sample(Reload, ReloadPosition * Reload->GetPlayLength(), false, ReloadPose);
            UpperBody(ReloadPose, ReloadWeight, Output);
        }
        else if (bArmed && Fire && SinceShot >= 0.f && SinceShot < FMath::Min(.22f, Fire->GetPlayLength()))
        {
            FPoseContext FirePose(this, Fire->IsValidAdditive());
            Sample(Fire, SinceShot, false, FirePose);
            if (Fire->IsValidAdditive())
            {
                FAnimationPoseData Base(Output), Additive(FirePose);
                FAnimationRuntime::AccumulateAdditivePose(Base, Additive, 1.f, Fire->GetAdditiveAnimType());
            }
            else UpperBody(FirePose, .6f * (1.f - FMath::Clamp(SinceShot / .22f, 0.f, 1.f)), Output);
        }
        if (bArmed && FMath::Abs(Pitch) > .05f && ReloadWeight < .99f)
        {
            UAnimSequence* Aim = Pitch >= 0.f ? AimUp : AimDown;
            if (Aim && Aim->IsValidAdditive())
            {
                FPoseContext AimPose(this, true);
                Sample(Aim, 0.f, false, AimPose);
                FAnimationPoseData Base(Output), Additive(AimPose);
                FAnimationRuntime::AccumulateAdditivePose(Base, Additive, FMath::Clamp(FMath::Abs(Pitch) / 90.f, 0.f, 1.f) * (1.f - ReloadWeight), Aim->GetAdditiveAnimType());
            }
            else
            {
                const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
                const int32 Spine = Bones.GetPoseBoneIndexForBoneName(TEXT("spine_03"));
                if (Spine != INDEX_NONE)
                {
                    const FCompactPoseBoneIndex Index = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Spine));
                    if (Index.GetInt() != INDEX_NONE)
                    {
                        FCSPose<FCompactPose> ComponentPose;
                        ComponentPose.InitPose(Output.Pose);
                        FTransform Transform = ComponentPose.GetComponentSpaceTransform(Index);
                        Transform.SetRotation(FQuat::Slerp(FQuat::Identity, FallbackAimRotation, 1.f - ReloadWeight) * Transform.GetRotation());
                        ComponentPose.SetComponentSpaceTransform(Index, Transform);
                        FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(ComponentPose, Output.Pose);
                    }
                }
            }
        }
        Output.Pose.NormalizeRotations();
        return true;
    }
};
}

UOBRifleAnimInstance::UOBRifleAnimInstance()
{
    const FString Root = TEXT("/Game/Characters/Mannequins/Anims/");
    UnarmedIdle = Sequence(Root + TEXT("Unarmed/MM_Idle"));
    RifleIdle = Sequence(Root + TEXT("Rifle/MF_Rifle_Idle_ADS"));
    const TCHAR* Directions[] = {TEXT("Fwd"), TEXT("Fwd_Right"), TEXT("Right"), TEXT("Bwd_Right"), TEXT("Bwd"), TEXT("Bwd_Left"), TEXT("Left"), TEXT("Fwd_Left")};
    for (const TCHAR* Direction : Directions)
    {
        UnarmedWalk.Add(Sequence(Root + FString::Printf(TEXT("Unarmed/Walk/MF_Unarmed_Walk_%s"), Direction)));
        UnarmedJog.Add(Sequence(Root + FString::Printf(TEXT("Unarmed/Jog/MF_Unarmed_Jog_%s"), Direction)));
        RifleWalk.Add(Sequence(Root + FString::Printf(TEXT("Rifle/Walk/MF_Rifle_Walk_%s"), Direction)));
        RifleJog.Add(Sequence(Root + FString::Printf(TEXT("Rifle/Jog/MF_Rifle_Jog_%s"), Direction)));
    }
    RifleReload = Sequence(Root + TEXT("Rifle/MM_Rifle_Reload"));
    RifleFire = Sequence(Root + TEXT("Rifle/MM_Rifle_Fire"));
    RifleAimUp = Sequence(Root + TEXT("Rifle/AIM/MM_Rifle_Idle_ADS_AO_CU"));
    RifleAimDown = Sequence(Root + TEXT("Rifle/AIM/MM_Rifle_Idle_ADS_AO_CD"));
    UnarmedFall = Sequence(Root + TEXT("Unarmed/Jump/MM_Fall_Loop"));
    RifleFall = Sequence(Root + TEXT("Rifle/Jump/MM_Rifle_Jump_Fall_Loop"));
    Death = Sequence(Root + TEXT("Death/MM_Death_Back_01"));
}

FAnimInstanceProxy* UOBRifleAnimInstance::CreateAnimInstanceProxy() { return new FOBRifleAnimProxy(this); }
void UOBRifleAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) { delete InProxy; }

FTransform UOBRifleAnimInstance::GetWeaponBoneInRightHand()
{
    // Authored weapon_r relative to hand_r in MF_Rifle_Idle_ADS, measured in
    // the animation skeleton rather than inferred from mesh socket names.
    return FTransform(FQuat(-.086687, -.122754, .986567, .064048).GetNormalized(),
        FVector(-7.218102, 3.417867, -.946828), FVector::OneVector);
}
