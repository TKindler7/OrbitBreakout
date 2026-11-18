#include "../OBPlayerCharacter.h"
#include "../OBEnemyCharacter.h"
#include "../OBRifleAnimInstance.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOBFabPresentationTest,"OrbitBreakout.Presentation.WraithMovingRecoilAndTrooperSkeleton",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOBFabPresentationTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,MakeUniqueObjectName(GetTransientPackage(),UWorld::StaticClass(),TEXT("OBCastTest")),GetTransientPackage());
    if(!TestNotNull(TEXT("Test world"),World))return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto Spawn=[&]() {
        const FTransform T(FRotator::ZeroRotator,FVector(0,0,100));
        auto* P=World->SpawnActorDeferred<AOBPlayerCharacter>(AOBPlayerCharacter::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        P->AutoPossessAI=EAutoPossessAI::Disabled;P->AutoPossessPlayer=EAutoReceiveInput::Disabled;
        P->FinishSpawning(T);P->RestoreLoadout(true,24,96);P->RefreshWeaponAttachment();
        return P;
    };
    auto* Base=Spawn();auto* Firing=Spawn();
    auto* Mesh=Base->GetMesh();auto* FireMesh=Firing->GetMesh();
    TestEqual(TEXT("Player is Wraith"),Mesh->GetSkeletalMeshAsset()->GetName(),FString(TEXT("Wraith")));
    TestTrue(TEXT("Wraith uses his own pose evaluator"),Mesh->GetAnimInstance()->IsA<UOBWraithAnimInstance>());
    TestTrue(TEXT("Authored muzzle socket exists"),Mesh->DoesSocketExist(TEXT("Muzzle_01")));
    bool bLegsMatch=true,bFinite=true,bGunRecoils=false,bLegsMove=false;
    FVector FirstFoot=FVector::ZeroVector;
    for(int Frame=0;Frame<45;++Frame) {
        for(auto* P:{Base,Firing}) {
            P->GetCharacterMovement()->Velocity=FVector(350,120,0);
            P->LastShotTime=P==Firing?World->GetTimeSeconds()-.04f:-100.f;
            P->GetMesh()->TickAnimation(1.f/60.f,false);
            P->GetMesh()->RefreshBoneTransforms();
        }
        for(FName Bone:{FName(TEXT("pelvis")),FName(TEXT("thigh_l")),FName(TEXT("thigh_r")),FName(TEXT("foot_l")),FName(TEXT("foot_r"))}) {
            bLegsMatch &= Mesh->GetSocketTransform(Bone,RTS_Component).Equals(FireMesh->GetSocketTransform(Bone,RTS_Component),.01f);
        }
        const FVector Foot=Mesh->GetSocketTransform(TEXT("foot_l"),RTS_Component).GetLocation();
        if(Frame==0)FirstFoot=Foot;
        else bLegsMove |= FVector::Distance(FirstFoot,Foot)>1.f;
        bGunRecoils |= !Mesh->GetSocketTransform(TEXT("Muzzle_01"),RTS_Component).Equals(FireMesh->GetSocketTransform(TEXT("Muzzle_01"),RTS_Component),.01f);
        for(const auto& T:FireMesh->GetComponentSpaceTransforms())bFinite &= !T.ContainsNaN();
    }
    TestTrue(TEXT("Movement advances the leg animation"),bLegsMove);
    TestTrue(TEXT("Firing does not replace moving pelvis or legs"),bLegsMatch);
    TestTrue(TEXT("Recoil affects the upper body and held rifle"),bGunRecoils);
    TestTrue(TEXT("Every evaluated bone transform is finite"),bFinite);
    TestTrue(TEXT("Shot origin matches Wraith muzzle"),Base->GetMuzzleLocation().Equals(Mesh->GetSocketLocation(TEXT("Muzzle_01")),.01f));
    auto* TrooperMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/SciFITrooper_Man_03/SkeletalMesh/SK_SciFITrooper_Man_03"));
    auto* Rifle=GetDefault<UOBRifleAnimInstance>();
    auto Check=[&](UAnimSequence* A){return A && A->GetSkeleton()==TrooperMesh->GetSkeleton() && A->GetPlayLength()>0.f;};
    bool bCompatible=TrooperMesh && Check(Rifle->RifleIdle) && Check(Rifle->RifleFire) && Check(Rifle->Death);
    for(auto A:Rifle->RifleWalk)bCompatible &= Check(A);
    for(auto A:Rifle->RifleJog)bCompatible &= Check(A);
    TestTrue(TEXT("Trooper animations are retargeted to the actual trooper skeleton"),bCompatible);
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    return true;
}
#endif
