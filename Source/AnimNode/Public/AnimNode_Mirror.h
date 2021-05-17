// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/InputScaleBias.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNode_Mirror.generated.h"


UENUM(BlueprintType)
enum class MirrorPlane : uint8
{
	XZ_Plane = 0,
	YZ_Plane = 1,
	XY_Plane = 2,
};

struct FMirrorFlippingRuleData
{
	FName MirrorBone;

	TArray<int> FlipAttrInfo;

	TArray<int> FlipValInfo;
};

USTRUCT(BlueprintInternalUseOnly)
struct ANIMNODE_API FAnimNode_Mirror : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink InPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings) //, meta = (PinHiddenByDefault))
	MirrorPlane MirPlane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings) //, meta = (PinHiddenByDefault))
	FString SearchReplaceKeyPair;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings) //, meta = (PinHiddenByDefault))
	FString SkipCheckKeyStr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta = (PinShownByDefault))
	bool bEnable;

public:
	FAnimNode_Mirror();

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Context) override;

private:
	TMap<FString, FString> SearchInfo;
	TMap<FName, FName> MirrorBoneInfo;
	TMap<FName, FMirrorFlippingRuleData> FlippingRule;
	TArray<FName> OperateBones;
	TMap<FName, FName> FlippingMorphTargetRule;

	TArray<int> TranslateFlippingValue;
	TArray<int> RotateFlippingValue;
	int ZeroPosId;

	TArray<FString> SearchKeys;
	TArray<FString> SkipCheckKeys;

	void GenerateInitialStatus();
	void GenerateSearchReplaceKey();
	void GenerateMirrorBoneInfo(const FAnimationInitializeContext& Context);
	void GenerateFlippingRule(const FAnimationInitializeContext& Context);

	bool CheckIsSkippedName(const FName& InBone);

	void SplitStringStr(const FString& InStr, const FString& InS, TArray<FString>& OutList);
	FName GetMirrorBone(const FName& InBone, const FReferenceSkeleton& RefSkel, int32& OutBoneID);
	void GenerateSingleBoneFlippingRule(const FAnimationInitializeContext& Context, const FName& ABone, const FName& BBone);//, TArray<int> (&FlipAttrInfo)[2], TArray<int> (&FlipValInfo)[2]);

	void GenerateBoneAxisRepInfo(const FAnimationInitializeContext& Context, const FName& InBone, TArray<int>& AxisRepInfo);
	void GenerateAxisRepInfoFromMatrix(const FMatrix& TM, TArray<int>& AxisRepInfo);

	void GenerateAlignAxisRepInfo(const TArray<int> (&AxisRepInfo)[2], TArray<int>(&AlignAxisRepInfo)[2]);
	void GenerateSingleBoneFlippingRuleDetail(const TArray<FName>& Objs, const TArray<int> (&AxisRepInfo)[2], const TArray<int> (&RotFlipVals)[2]);
	void GenerateRotationFlippingValues(const TArray<int>& AlignAxisRepInfo, TArray<int>& RotFlipVals);

	void OutputMirrorFlippingRuleLog();
	FString TArrayOutput(const TArray<int> InArray);
	//////////////////
	void GenerateMirrorMorphTargetInfo(const FAnimationInitializeContext& Context);
	FName GetMirrorAnimCurve(const FName& InCurve, const TArray<FName>& AllCurves);

	void DoMirrorBones(FPoseContext& Output);
	void DoMirrorMorphTargets(FPoseContext& Output);
};