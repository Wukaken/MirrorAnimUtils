// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimGraphNode_Mirror.h"

UAnimGraphNode_Mirror::UAnimGraphNode_Mirror(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{    
}

FString UAnimGraphNode_Mirror::GetNodeCategory() const
{
    return TEXT("Mirror Anim Pose");
}

FLinearColor UAnimGraphNode_Mirror::GetNodeTitleColor() const
{
    return FLinearColor(1.f, .55f, 0.f);
}

FText UAnimGraphNode_Mirror::GetTooltipText() const
{
    return FText::FromString("Mirror Animation");
}

FText UAnimGraphNode_Mirror::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    FString Result("Anim Mirror");
    Result += (TitleType == ENodeTitleType::ListView) ? TEXT("") : TEXT("\n");
    return FText::FromString(Result);
}

