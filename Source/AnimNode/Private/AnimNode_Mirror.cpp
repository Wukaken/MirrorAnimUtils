// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNode_Mirror.h"
#include "AnimationRuntime.h"
#include "BoneContainer.h"
#include "Animation/AnimInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/Skeleton.h"
#include "ReferenceSkeleton.h"
#include "Kismet/KismetMathLibrary.h"

FAnimNode_Mirror::FAnimNode_Mirror()
    : MirPlane(MirrorPlane::YZ_Plane)
    , SearchReplaceKeyPair(FString("_l,_r,_lt,_rt,_left,_right,L_,R_,_L_,_R_,Left,Right"))
    , SkipCheckKeyStr(FString(""))
    , bEnable(true)
{
}

void FAnimNode_Mirror::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread);
    FAnimNode_Base::Initialize_AnyThread(Context);
    // Init the Inputs
    InPose.Initialize(Context);

    if(bEnable){
        GenerateInitialStatus();
        GenerateSearchReplaceKey();
        SplitStringStr(SkipCheckKeyStr, TEXT(","),  SkipCheckKeys);
        GenerateMirrorBoneInfo(Context);
        GenerateFlippingRule(Context);

        //OutputMirrorFlippingRuleLog();
        GenerateMirrorMorphTargetInfo(Context);
    }
}

void FAnimNode_Mirror::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread);
    InPose.CacheBones(Context);
}

void FAnimNode_Mirror::Update_AnyThread(const FAnimationUpdateContext& Context)
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread);
    InPose.Update(Context);
    GetEvaluateGraphExposedInputs().Execute(Context);
}

void FAnimNode_Mirror::Evaluate_AnyThread(FPoseContext& Output)
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread);
    if(bEnable){
        FPoseContext SourceData(Output);
        InPose.Evaluate(SourceData);
        Output = SourceData;

        DoMirrorBones(Output);
        DoMirrorMorphTargets(Output);
    }
    else
        InPose.Evaluate(Output);
}

void FAnimNode_Mirror::DoMirrorBones(FPoseContext& Output)
{
    TArray<float> Temp;
    Temp.Init(0, 3);

    const FBoneContainer& BoneContainer = Output.AnimInstanceProxy->GetRequiredBones();
    USkeleton* Skel = BoneContainer.GetSkeletonAsset();
    FReferenceSkeleton RefSkel = Skel->GetReferenceSkeleton();

    for(FName ABone : OperateBones){
        FMirrorFlippingRuleData AData = FlippingRule[ABone];

        FName BBone = AData.MirrorBone;
        TArray<FName> Objs = {ABone, BBone};
        int ObjNum = 2;
        if(ABone == BBone)
            ObjNum = 1;

        TArray<FTransform> NTrans;
        for(int i = 0; i < ObjNum; i++){
            FName BObj = Objs[(i + 1) % 2];

            FMirrorFlippingRuleData BData = FlippingRule[BBone];
            int32 BId = Output.AnimInstanceProxy->GetSkelMeshComponent()->GetBoneIndex(BObj);
            FCompactPoseBoneIndex CBId = Output.Pose.GetBoneContainer().MakeCompactPoseIndex(FMeshPoseBoneIndex(BId));

            FTransform BOTrans = RefSkel.GetRefBonePose()[BId];
            FTransform BNTrans = Output.Pose[CBId];
            FTransform BTrans = BNTrans.GetRelativeTransform(BOTrans);

            FVector BLoc = BTrans.GetTranslation();
            FRotator BRot = BTrans.Rotator();

            // UE_LOG(LogTemp, Warning, TEXT("Bone: %s, Mirror Bone: %s"), *(Objs[i].ToString()), *BObj.ToString());
            // UE_LOG(LogTemp, Warning, TEXT("    Ori: %s, %s"), *BOTrans.ToString(), *BNTrans.ToString());
            // UE_LOG(LogTemp, Warning, TEXT("   Tran: %s"), *BTrans.ToString());

            Temp[BData.FlipAttrInfo[0]] = BLoc.X * BData.FlipValInfo[0];
            Temp[BData.FlipAttrInfo[1]] = BLoc.Y * BData.FlipValInfo[1];
            Temp[BData.FlipAttrInfo[2]] = BLoc.Z * BData.FlipValInfo[2];

            FVector NALoc(Temp[0], Temp[1], Temp[2]);

            Temp[BData.FlipAttrInfo[3] - 3] = BRot.Roll * BData.FlipValInfo[3];
            Temp[BData.FlipAttrInfo[4] - 3] = BRot.Pitch * BData.FlipValInfo[4];
            Temp[BData.FlipAttrInfo[5] - 3] = BRot.Yaw * BData.FlipValInfo[5];

            FRotator NARot(Temp[1], Temp[2], Temp[0]);
            FMatrix NM = FRotationTranslationMatrix(NARot, NALoc);
            FTransform NTran(NM);
            NTrans.Emplace(NTran);
        }

        for(int i = 0; i < ObjNum; i++){
            FName AObj = Objs[i];
            int32 AId = Output.AnimInstanceProxy->GetSkelMeshComponent()->GetBoneIndex(AObj);
            FCompactPoseBoneIndex CAId = Output.Pose.GetBoneContainer().MakeCompactPoseIndex(FMeshPoseBoneIndex(AId));

            FTransform AOTrans = RefSkel.GetRefBonePose()[AId];
            Output.Pose[CAId] = NTrans[i] * AOTrans;
        }        
    }
}

void FAnimNode_Mirror::DoMirrorMorphTargets(FPoseContext& Output)
{
    TMap<SmartName::UID_Type, float> MirMorTarValInfo;
    USkeleton* Skel = Output.AnimInstanceProxy->GetSkeleton();
    for(const TPair<FName, FName>& KVP : FlippingMorphTargetRule){
        FName ACurve = KVP.Key;
        FName BCurve = KVP.Value;
        SmartName::UID_Type AUID = Skel->GetUIDByName(USkeleton::AnimCurveMappingName, ACurve);
        if(MirMorTarValInfo.Contains(AUID))
            continue;
        
        SmartName::UID_Type BUID = Skel->GetUIDByName(USkeleton::AnimCurveMappingName, BCurve);
        float AVal = Output.Curve.Get(AUID);
        float BVal = Output.Curve.Get(BUID);
        MirMorTarValInfo.Emplace(AUID, BVal);
        MirMorTarValInfo.Emplace(BUID, AVal);

        //UE_LOG(LogTemp, Warning, TEXT("Curve: %s Mir: %s Values: %f %f"), *ACurve.ToString(), *BCurve.ToString(), AVal, BVal);
    }
    
    //UE_LOG(LogTemp, Warning, TEXT("MirMor Num: %d"), MirMorTarValInfo.Num());
    for(const TPair<SmartName::UID_Type, float>& KVP : MirMorTarValInfo)
        Output.Curve.Set(KVP.Key, KVP.Value);
}

void FAnimNode_Mirror::SplitStringStr(const FString& InStr, const FString& InS, TArray<FString>& OutList)
{
    OutList.Empty();

    FString ChkStr = InStr.Replace(TEXT(" "), TEXT(""));
    FString LtS, RtS;

    OutList.Add(ChkStr);
    bool bIsSplited = true;
    while(bIsSplited){
        bIsSplited = ChkStr.Split(InS, &LtS, &RtS);
        if(bIsSplited){
            OutList[OutList.Num() - 1] = LtS;
            if(RtS != TEXT(""))
                OutList.Add(RtS);
            
            ChkStr = RtS;
        }
    }
}

bool FAnimNode_Mirror::CheckIsSkippedName(const FName& InName)
{
    bool bIsSkip = false;
    FString InNameStr = InName.ToString();
    if(SkipCheckKeys.Num() > 0){
        for(FString SkipKey : SkipCheckKeys){
            if(InNameStr.Contains(SkipKey))
                bIsSkip = true;
        }
    }
    return bIsSkip;
}

void FAnimNode_Mirror::GenerateInitialStatus()
{
    //UE_LOG(LogTemp, Warning, TEXT("Renew Status: %d"), MirPlane);
    TranslateFlippingValue.Empty();
    RotateFlippingValue.Empty();
    ZeroPosId = 0;
    TranslateFlippingValue.Init(1, 3);
    RotateFlippingValue.Init(1, 3);
    if(MirPlane == MirrorPlane::XZ_Plane){
        TranslateFlippingValue[1] = -1;
        RotateFlippingValue[0] = -1;
        RotateFlippingValue[2] = -1;
        ZeroPosId = 1; //13;
    }
    else if(MirPlane == MirrorPlane::YZ_Plane){
        TranslateFlippingValue[0] = -1;
        RotateFlippingValue[1] = -1;
        RotateFlippingValue[2] = -1;
        ZeroPosId = 0; //12;
    }
    else{
        TranslateFlippingValue[2] = -1;
        RotateFlippingValue[0] = -1;
        RotateFlippingValue[1] = -1;
        ZeroPosId = 2; //14;
    }
}

void FAnimNode_Mirror::GenerateSearchReplaceKey()
{
    SplitStringStr(SearchReplaceKeyPair, TEXT(","),  SearchKeys);
    int Num = SearchKeys.Num();
    if(Num % 2 == 1)
        SearchKeys.RemoveAt(Num - 1);
    
    SearchInfo.Empty();
    for(int i = 0; i < SearchKeys.Num(); i += 2){
        SearchInfo.Emplace(SearchKeys[i], SearchKeys[i + 1]);
        SearchInfo.Emplace(SearchKeys[i + 1], SearchKeys[i]);
    }

    SearchKeys.Sort(
        [](const FString& A, const FString& B){
            return A.Len() > B.Len();
        }
    );
}

FName FAnimNode_Mirror::GetMirrorBone(const FName& InBone, const FReferenceSkeleton& RefSkel, int32& OutBoneID)
{
    FString InBoneStr = InBone.ToString();
    FString OutBoneStr, ReplaceKey;
    FName OutBone;
    for(FString SearchKey : SearchKeys){
        if(InBoneStr.Contains(SearchKey)){
            ReplaceKey = SearchInfo[SearchKey];
            OutBoneStr = InBoneStr.Replace(*SearchKey, *ReplaceKey);
            int32 BoneID = RefSkel.FindBoneIndex(FName(OutBoneStr));
            if(BoneID != INDEX_NONE){
                OutBone = FName(*OutBoneStr);
                OutBoneID = BoneID;
                break;
            }
        }
    }
    return OutBone;
}

void FAnimNode_Mirror::GenerateMirrorBoneInfo(const FAnimationInitializeContext& Context)
{
    if(SearchInfo.Num() == 0)
        return;

    MirrorBoneInfo.Empty();

    const FBoneContainer& BoneContainer = Context.AnimInstanceProxy->GetRequiredBones();
    USkeleton* Skel = BoneContainer.GetSkeletonAsset();
    FReferenceSkeleton RefSkel = Skel->GetReferenceSkeleton();
    TArray<FMeshBoneInfo> BoneInfo = RefSkel.GetRawRefBoneInfo();
    int32 BoneNum = RefSkel.GetNum();
    for(int32 i = 0; i < BoneNum; i++){ 
        FName BoneName = BoneInfo[i].Name;
        if(MirrorBoneInfo.Contains(BoneName))
            continue;

        if(CheckIsSkippedName(BoneName))
            continue;

        int32 MirrorID;
        FName MirrorBoneName = GetMirrorBone(BoneName, RefSkel, MirrorID);
        if(MirrorBoneName.IsNone()){
            FTransform Trans = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkel, i);
            FMatrix WorldTM = Trans.ToMatrixWithScale();
            if(UKismetMathLibrary::Abs(WorldTM.M[3][ZeroPosId]) < 0.001){
                MirrorBoneName = BoneName;
                MirrorID = i;
                MirrorBoneInfo.Emplace(BoneName, MirrorBoneName);
            }
        }
        if(MirrorBoneName.IsNone())
            continue;
        
        MirrorBoneInfo.Emplace(BoneName, MirrorBoneName);
        MirrorBoneInfo.Emplace(MirrorBoneName, BoneName);
    }
}

void FAnimNode_Mirror::GenerateFlippingRule(const FAnimationInitializeContext& Context)
{
    FlippingRule.Empty();
    OperateBones.Empty();
    if(MirrorBoneInfo.Num() == 0)
        return;

    for(TPair<FName, FName>& KVP : MirrorBoneInfo){
        FName ABone = KVP.Key;
        FName BBone = KVP.Value;

        if(FlippingRule.Contains(BBone))
            continue;

        OperateBones.Add(ABone);
        GenerateSingleBoneFlippingRule(Context, ABone, BBone);
    }
}

void FAnimNode_Mirror::GenerateSingleBoneFlippingRule(const FAnimationInitializeContext& Context, const FName& ABone, const FName& BBone)
{
    TArray<int> AxisRepInfo[2];
    TArray<FName> Objs = {ABone, BBone};
    for(int i = 0; i < Objs.Num(); i++)
        GenerateBoneAxisRepInfo(Context, Objs[i], AxisRepInfo[i]);

    TArray<int> AlignAxisRepInfo[2];
    GenerateAlignAxisRepInfo(AxisRepInfo, AlignAxisRepInfo);

    TArray<int> RotFlipVals[2];
    for(int i = 0; i < 2; i++)
        GenerateRotationFlippingValues(AlignAxisRepInfo[i], RotFlipVals[i]);
        
    GenerateSingleBoneFlippingRuleDetail(Objs, AxisRepInfo, RotFlipVals);
}

void FAnimNode_Mirror::GenerateBoneAxisRepInfo(const FAnimationInitializeContext& Context, const FName& InBone, TArray<int>& AxisRepInfo)
{
    const FBoneContainer& BoneContainer = Context.AnimInstanceProxy->GetRequiredBones();
    USkeleton* Skel = BoneContainer.GetSkeletonAsset();
    FReferenceSkeleton RefSkel = Skel->GetReferenceSkeleton();
    
    int32 BoneId = RefSkel.FindBoneIndex(InBone);
    FTransform Trans = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkel, BoneId);
    FMatrix WorldTM = Trans.ToMatrixWithScale();
    
    GenerateAxisRepInfoFromMatrix(WorldTM, AxisRepInfo);
}

void FAnimNode_Mirror::GenerateAxisRepInfoFromMatrix(const FMatrix& TM, TArray<int>& AxisRepInfo)
{
    TArray<int> CheckList;
    TArray<int> InvalidIds;
    for(int j = 0; j < 3; j++){
        float AxisMaxVal = 0;
        int AxisMaxId = 0;
        int AbsAxisMaxId = 0;
        for(int k = 0; k < 3; k++){
            float Val = TM.M[k][j];
            float AbsVal = UKismetMathLibrary::Abs(Val);
            if(AbsVal > AxisMaxVal){
                AxisMaxVal = AbsVal;
                AxisMaxId = (k + 1) * int(Val / AbsVal);
            }
        }

        AxisRepInfo.Add(AxisMaxId);
        AbsAxisMaxId = UKismetMathLibrary::Abs(AxisMaxId);
        if(CheckList.Num() > 0 && CheckList.Contains(AbsAxisMaxId))
            InvalidIds.Add(AbsAxisMaxId);
        
        CheckList.Add(AbsAxisMaxId);
    }

    if(InvalidIds.Num() > 0){
        TArray<int> KeepIds;
        TArray<int> ValidVals;
        for(int InvalidId : InvalidIds){
            int AxisMaxVal = 0;
            int KeepId = -1;
            int ValidVal = -1;
            for(int j = 0; j < 3; j++){
                if(KeepIds.Contains(j))
                    continue;

                int CurAbsAxisRepId = UKismetMathLibrary::Abs(AxisRepInfo[j]);
                if(CurAbsAxisRepId == InvalidId){
                    float AbsVal = UKismetMathLibrary::Abs(TM.M[InvalidId - 1][j]);
                    if(AbsVal > AxisMaxVal){
                        AxisMaxVal = AbsVal;
                        KeepId = j;
                        ValidVal = CurAbsAxisRepId;
                    }
                }
                else{
                    KeepIds.Add(j);
                    ValidVals.Add(CurAbsAxisRepId);
                }
            }

            KeepIds.Add(KeepId);
            if(ValidVals.Contains(UKismetMathLibrary::Abs(AxisRepInfo[KeepId]))){
                int k = 1;
                for(; k < 4; k++){
                    if(!ValidVals.Contains(k))
                        break;
                }

                float Val = TM.M[k - 1][KeepId];
                AxisRepInfo[KeepId] = int(Val / UKismetMathLibrary::Abs(Val)) * k;
            }

            ValidVals.Add(UKismetMathLibrary::Abs(AxisRepInfo[KeepId]));
        }

        for(int j = 0; j < 3; j++){
            if(!KeepIds.Contains(j)){
                int k = 1;
                for(; k < 4; k++){
                    if(!ValidVals.Contains(k))
                        break;
                }
                float Val = TM.M[k - 1][j];
                AxisRepInfo[j] = int(Val / UKismetMathLibrary::Abs(Val)) * k;
            }
        }
    }
}

void FAnimNode_Mirror::GenerateAlignAxisRepInfo(const TArray<int> (&AxisRepInfo)[2], TArray<int>(&AlignAxisRepInfo)[2])
{
    for(int i = 0; i < 2; i++){
        for(int Val : AxisRepInfo[i])
            AlignAxisRepInfo[i].Add(Val);
    }

    if(AlignAxisRepInfo[0][0] * AlignAxisRepInfo[1][0] < 0){
        AlignAxisRepInfo[1][0] *= -1;
        AlignAxisRepInfo[1][1] *= -1;
        AlignAxisRepInfo[1][2] *= -1;
    } 
}

void FAnimNode_Mirror::GenerateRotationFlippingValues(const TArray<int>& AlignAxisRepInfo, TArray<int>& RotFlipVals)
{
    for(int j = 0; j < 3; j++){
        int CVal = 1;

        int CId = AlignAxisRepInfo[j];
        int OriNId = (int)(UKismetMathLibrary::Abs(CId) - 1 + 1) % 3 + 1;
        int OriNnId = (int)(UKismetMathLibrary::Abs(CId) - 1 + 2) % 3 + 1;

        int NId = AlignAxisRepInfo[(j + 1) % 3];
        int NnId = AlignAxisRepInfo[(j + 2) % 3];

        if(OriNId == UKismetMathLibrary::Abs(NId)){
            if(OriNId == -NId)
                CVal *= -1;
            if(OriNnId == -NnId)
                CVal *= -1;
        }
        else{
            CVal *= -1;
            if(OriNId * NId < 0)
                CVal *= -1;
            if(OriNnId * NnId < 0)
                CVal *= -1;
        }

        RotFlipVals.Add(CVal);
    }
}

void FAnimNode_Mirror::GenerateSingleBoneFlippingRuleDetail(const TArray<FName>& Objs, const TArray<int> (&AxisRepInfo)[2], const TArray<int> (&RotFlipVals)[2])
{
    TArray<int> OutTranslateFlipVals;
    TArray<int> OutRotateFlipVals;
    for(int i = 0; i < 3; i++){
        int TVal = AxisRepInfo[0][i] * AxisRepInfo[1][i] * TranslateFlippingValue[i];
        int RVal = RotFlipVals[0][i] * RotFlipVals[1][i] * RotateFlippingValue[i];
        OutTranslateFlipVals.Add(TVal / (int)UKismetMathLibrary::Abs(TVal));
        OutRotateFlipVals.Add(RVal);
    }

    for(int i = 0; i < Objs.Num(); i++){
        FName AObj = Objs[i];
        FName BObj = Objs[(i + 1) % 2];
        
        FMirrorFlippingRuleData Data;
        Data.MirrorBone = BObj;
        for(int j = 0; j < 6; j++){
            Data.FlipAttrInfo.Add(j);
            Data.FlipValInfo.Add(1);
        }

        for(int j = 0; j < 3; j++){
            int OriTAttrId = UKismetMathLibrary::Abs(AxisRepInfo[i][j]) - 1;
            int OriRAttrId = UKismetMathLibrary::Abs(AxisRepInfo[i][j]) - 1 + 3;
            int ToTAttrId = UKismetMathLibrary::Abs(AxisRepInfo[(i + 1) % 2][j]) - 1;
            int ToRAttrId = UKismetMathLibrary::Abs(AxisRepInfo[(i + 1) % 2][j]) - 1 + 3;
            Data.FlipValInfo[OriTAttrId] = OutTranslateFlipVals[j];
            Data.FlipValInfo[OriRAttrId] = OutRotateFlipVals[j];
            Data.FlipAttrInfo[OriTAttrId] = ToTAttrId;
            Data.FlipAttrInfo[OriRAttrId] = ToRAttrId;
        }

        FlippingRule.Emplace(AObj, Data);
    }
}

void FAnimNode_Mirror::OutputMirrorFlippingRuleLog()
{
    for(TPair<FName, FMirrorFlippingRuleData>& KVP : FlippingRule){
        FName ABone = KVP.Key;
        FMirrorFlippingRuleData Data = KVP.Value;
        
        UE_LOG(LogTemp, Warning, TEXT("Bone: %s Mirror: %s "), *ABone.ToString(), *(Data.MirrorBone).ToString());
        UE_LOG(LogTemp, Warning, TEXT("    FlipAttrs: %s"), *TArrayOutput(Data.FlipAttrInfo));
        UE_LOG(LogTemp, Warning, TEXT("    FlipVals:  %s"), *TArrayOutput(Data.FlipValInfo));
    }
}

FString FAnimNode_Mirror::TArrayOutput(const TArray<int> InArray)
{
    FString Out = TEXT("Array: ");
    for(int Val : InArray)
        Out += FString::Printf(TEXT("%d "), Val);

    return Out;
}

void FAnimNode_Mirror::GenerateMirrorMorphTargetInfo(const FAnimationInitializeContext& Context)
{
    FlippingMorphTargetRule.Empty();

    USkeleton* Skel = Context.AnimInstanceProxy->GetSkeleton();
    const FSmartNameMapping* Mapping = Skel->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
    TArray<FName> AllCurves;
    if(Mapping)
        Mapping->FillNameArray(AllCurves);
    
    for(FName ACurve : AllCurves){
        if(FlippingMorphTargetRule.Contains(ACurve))
            continue;

        if(CheckIsSkippedName(ACurve))
            continue;
        
        FName BCurve = GetMirrorAnimCurve(ACurve, AllCurves);
        if(!BCurve.IsNone()){
            FlippingMorphTargetRule.Emplace(ACurve, BCurve);
            FlippingMorphTargetRule.Emplace(BCurve, ACurve);
        }
    }
}

FName FAnimNode_Mirror::GetMirrorAnimCurve(const FName& InCurve, const TArray<FName>& AllCurves)
{
    FString InStr = InCurve.ToString();
    FString OutStr, ReplaceKey;
    FName OutCurve;
    for(FString SearchKey : SearchKeys){
        if(InStr.Contains(SearchKey)){
            ReplaceKey = SearchInfo[SearchKey];
            OutStr = InStr.Replace(*SearchKey, *ReplaceKey);
            if(AllCurves.Contains(FName(*OutStr))){
                OutCurve = FName(*OutStr);
                break;
            }
        }
    }
    return OutCurve;
}