// Copyright (c) Henry Cooney 2017

#include "rtik.h"
#include "IK.h"
#include "Components/SkeletalMeshComponent.h"

FVector FIKUtil::IKBoneAxisToVector(EIKBoneAxis InBoneAxis)
{
	switch (InBoneAxis) 
	{
	case EIKBoneAxis::IKBA_X: return FVector(1.0f, 0.0f, 0.0f);
	case EIKBoneAxis::IKBA_Y: return FVector(0.0f, 1.0f, 0.0f);
	case EIKBoneAxis::IKBA_Z: return FVector(0.0f, 0.0f, 1.0f);
	case EIKBoneAxis::IKBA_XNeg: return FVector(-1.0f, 0.0f, 0.0f);
	case EIKBoneAxis::IKBA_YNeg: return FVector(0.0f, -1.0f, 0.0f);
	case EIKBoneAxis::IKBA_ZNeg: return FVector(0.0f, 0.0f, -1.0f);		
	}
	
	return FVector(0.0f, 0.0f, 0.0f);
}

FVector FIKUtil::GetSkeletalMeshWorldAxis(const USkeletalMeshComponent& SkelComp, EIKBoneAxis InBoneAxis)
{
	FTransform ComponentTransform = SkelComp.GetComponentToWorld();
	switch (InBoneAxis)  
	{
	case EIKBoneAxis::IKBA_X: return ComponentTransform.GetUnitAxis(EAxis::X);
	case EIKBoneAxis::IKBA_Y: return ComponentTransform.GetUnitAxis(EAxis::Y);
	case EIKBoneAxis::IKBA_Z: return ComponentTransform.GetUnitAxis(EAxis::Z);
	case EIKBoneAxis::IKBA_XNeg: return -1 * ComponentTransform.GetUnitAxis(EAxis::X);
	case EIKBoneAxis::IKBA_YNeg: return -1 * ComponentTransform.GetUnitAxis(EAxis::Y);
	case EIKBoneAxis::IKBA_ZNeg: return -1 * ComponentTransform.GetUnitAxis(EAxis::Z);
	}

	return FVector(0.0f, 0.0f, 0.0f);
}


#pragma region FIKBone
bool FIKBone::InitIfInvalid(const FBoneContainer& RequiredBones)
{
	if (IsValid(RequiredBones))
	{
		return true;
	}
	
	Init(RequiredBones);
	bool bIsValid = Init(RequiredBones);
	return bIsValid;
}

// Initialize this IK Bone. Must be called before use.
bool FIKBone::Init(const FBoneContainer& RequiredBones)
{
	FIKBoneConstraint* IKConstraint = GetConstraint();

	if (IKConstraint != nullptr && !IKConstraint->Initialize())
	{
#if ENABLE_IK_DEBUG
		UE_LOG(LogRTIK, Warning, TEXT("FIKBone::Init -- Constraint for bone %s failed to initialize"),
			*BoneRef.BoneName.ToString());
#endif // ENABLE_IK_DEBUG
	}

	if (BoneRef.Initialize(RequiredBones))
	{
		BoneIndex = BoneRef.GetCompactPoseIndex(RequiredBones);
		return true;
	}
	else
	{
#if ENABLE_IK_DEBUG
		UE_LOG(LogRTIK, Warning, TEXT("FIKBone::Init -- IK Bone initialization failed for bone: %s"),
			*BoneRef.BoneName.ToString());
#endif // ENABLE_IK_DEBUG
		return false;
	}
}

bool FIKBone::IsValid(const FBoneContainer& RequiredBones)
{
	bool bValid = BoneRef.IsValidToEvaluate(RequiredBones);
	
#if ENABLE_IK_DEBUG_VERBOSE
	if (!bValid)
	{
		UE_LOG(LogRTIK, Warning, TEXT("FIKBone::IsValid -- IK Bone %s was invalid"),
			*BoneRef.BoneName.ToString());
	}
#endif // ENABLE_IK_DEBUG_VERBOSE
	return bValid;
}

FIKBoneConstraint* FIKBone::GetConstraint()
{
	if (Constraint == nullptr)
	{
		return nullptr;
	}

	return Constraint->GetConstraint();
}

#pragma endregion FIKBone

#pragma region UIKBoneWrapper
void UIKBoneWrapper::Initialize(FIKBone InBone)
{
	Bone = InBone;
	bInitialized = true;
}

bool UIKBoneWrapper::InitIfInvalid(const FBoneContainer& RequiredBones)
{
	if (!bInitialized)
	{
#if ENABLE_IK_DEBUG
		UE_LOG(LogRTIK, Warning, TEXT("IK Bone Wrapper was not initialized -- you must call Initialize in blueprint before use"));
#endif // ENABLE_IK_DEBUG
		return false;
	}
	
	return Bone.InitIfInvalid(RequiredBones);
}

bool UIKBoneWrapper::Init(const FBoneContainer& RequiredBones)
{
	if (!bInitialized)
	{
#if ENABLE_IK_DEBUG
		UE_LOG(LogRTIK, Warning, TEXT("IK Bone Wrapper was not initialized -- you must call Initialize in blueprint before use"));
#endif // ENABLE_IK_DEBUG
		return false;
	}
	
	return Bone.Init(RequiredBones);
}

bool UIKBoneWrapper::IsValid(const FBoneContainer& RequiredBones)
{
	if (!bInitialized)
		{
		return false;
	}
	
	return Bone.IsValid(RequiredBones);
}
#pragma endregion UIKBoneWrapper

#pragma region FIKModChain
bool FIKModChain::InitIfInvalid(const FBoneContainer& RequiredBones)
{
	if (IsValid(RequiredBones))
	{
		return true;
	}

	InitBoneReferences(RequiredBones);
	bool bValid = IsValid(RequiredBones);
	return bValid;
}

bool FIKModChain::InitBoneReferences(const FBoneContainer& RequiredBones)
{
	return false;
}

bool FIKModChain::IsValid(const FBoneContainer& RequiredBones)
{
	return false; 
}
#pragma endregion FIKModChain

#pragma region FRangeLimitedIKChain
bool FRangeLimitedIKChain::InitBoneReferences(const FBoneContainer & RequiredBones)
{
	bValid = true;

	size_t LargestBoneIndex = 0;
	for (size_t i = 0; i < BonesRootToEffector.Num(); ++i)
	{
		FIKBone& Bone = BonesRootToEffector[i];
		if (!Bone.Init(RequiredBones))
		{
			bValid = false;
		}

		if (i > 0)
		{ 
			FIKBone& PreviousBone = BonesRootToEffector[i - 1];
			if (PreviousBone.BoneIndex.GetInt() >= Bone.BoneIndex.GetInt())
			{
#if ENABLE_IK_DEBUG
				UE_LOG(LogRTIK, Warning, TEXT("Could not initialized range limited IK chain - bone named %s was not preceeded by a skeletal parent"),
					*(Bone.BoneRef.BoneName.ToString()));
#endif // ENABLE_IK_DEBUG
				bValid = false;
			}
			
			// Check that the bone has nonzero length
			FTransform BoneTransform = RequiredBones.GetRefPoseTransform(Bone.BoneIndex);
			FTransform ParentTransform = RequiredBones.GetRefPoseTransform(PreviousBone.BoneIndex);
			if (FVector::Dist(BoneTransform.GetLocation(), ParentTransform.GetLocation()) < KINDA_SMALL_NUMBER)
			{
#if ENABLE_IK_DEBUG
				UE_LOG(LogRTIK, Warning, TEXT("Could not initialized range limited IK chain - bone named %s has zero length"),
					*(Bone.BoneRef.BoneName.ToString()));
#endif // ENABLE_IK_DEBUG
				bValid = false;
			}
		}

		if (Bone.BoneIndex.GetInt() > LargestBoneIndex)
		{
			LargestBoneIndex = Bone.BoneIndex.GetInt();
		}
	}
	
	return bValid;
}

bool FRangeLimitedIKChain::IsValid(const FBoneContainer & RequiredBones)
{
	for (FIKBone& Bone : BonesRootToEffector)
	{
		bValid &= Bone.IsValid(RequiredBones);
	}
	return bValid;
}

FIKBone& FRangeLimitedIKChain::operator[](size_t i)
{
	return BonesRootToEffector[i];
}

FIKBone & FRangeLimitedIKChain::AccessFromRoot(size_t i)
{
	return BonesRootToEffector[i];
}

FIKBone & FRangeLimitedIKChain::AccessFromEffector(size_t i)
{
	size_t NumBones = BonesRootToEffector.Num();
	return BonesRootToEffector[NumBones - 1 - i];
}

size_t FRangeLimitedIKChain::Num()
{
	return BonesRootToEffector.Num();
}
#pragma endregion FRangeLimitedIKChain

#pragma region UIKChainWrapper
bool UIKChainWrapper::InitIfInvalid(const FBoneContainer& RequiredBones)
{
	return false;
}

// Initialize all bones used in this chain. Must be called before use.
bool UIKChainWrapper::InitBoneReferences(const FBoneContainer& RequiredBones)
{
	return false;
}

// Check whether this chain is valid to use. Should be called in the IsValid method of your animnode.
bool UIKChainWrapper::IsValid(const FBoneContainer& RequiredBones)
{
	return false;
}
#pragma endregion UIKChainWrapper

#pragma region URangedLimitedIKChainWrapper
void URangeLimitedIKChainWrapper::Initialize(FRangeLimitedIKChain InChain)
{
	Chain = InChain;
	bInitialized = true;
}

bool URangeLimitedIKChainWrapper::InitIfInvalid(const FBoneContainer& RequiredBones)
{
	if (!bInitialized)
	{
#if ENABLE_IK_DEBUG
		UE_LOG(LogRTIK, Warning, TEXT("Range limited IK chain wrapper was not initialized -- make sure you call Initialize function in blueprint before use"));
#endif // ENABLE_IK_DEBUG
		return false;
	}
	return Chain.InitIfInvalid(RequiredBones);
}

// Initialize all bones used in this chain. Must be called before use.
bool URangeLimitedIKChainWrapper::InitBoneReferences(const FBoneContainer& RequiredBones)
{
	if (!bInitialized)
	{
#if ENABLE_IK_DEBUG
		UE_LOG(LogRTIK, Warning, TEXT("Range limited IK chain wrapper was not initialized -- make sure you call Initialize function in blueprint before use"));
#endif // ENABLE_IK_DEBUG
		return false;
	}
	return Chain.InitIfInvalid(RequiredBones);
}

// Check whether this chain is valid to use. Should be called in the IsValid method of your animnode.
bool URangeLimitedIKChainWrapper::IsValid(const FBoneContainer& RequiredBones)
{
	if (!bInitialized)
	{
		return false;
	}
	return Chain.IsValid(RequiredBones);
}
#pragma endregion URangedLimitedIKChainWrapper
