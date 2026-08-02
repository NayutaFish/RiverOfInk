// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/StateBase.h"

UStateBase::UStateBase()
{
}

void UStateBase::BeginPlay()
{
	Super::BeginPlay();
	// 需要组件就绪后的事件可放在这里
}

void UStateBase::OnEnter_Implementation()
{
	// 进入状态时的逻辑，子类可重写
}

void UStateBase::Update_Implementation(float DeltaTime)
{
	// 每帧更新逻辑，子类可重写
}

void UStateBase::OnExit_Implementation()
{
	// 退出状态时的逻辑，子类可重写
}

void UStateBase::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
