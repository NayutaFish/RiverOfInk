// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeEconomySubsystem.h"

#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Engine/GameInstance.h"
#include "RiverOfInk.h"
#include "RoguelikeSystem/RoguelikeRunFlowSubsystem.h"

void URoguelikeEconomySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Wallet = FPureInkWallet();
	ProcessedEnemyDrops.Reset();
	GrantedRoomResults.Reset();

	if (!bEnemyDeathSubscribed)
	{
		EnemyDeathDelegateHandle = FEventBus::Subscribe<FNonPlayerDiedEvent>(
			[this](const FNonPlayerDiedEvent& InEvent)
			{
				HandleEnemyDied(InEvent);
			});
		bEnemyDeathSubscribed = EnemyDeathDelegateHandle.IsValid();
	}

	UE_LOG(LogRoguelike, Log,
		TEXT("Pure Ink economy initialized. Balance=%d EnemyDropSubscription=%s."),
		Wallet.Balance,
		bEnemyDeathSubscribed ? TEXT("true") : TEXT("false"));
}

void URoguelikeEconomySubsystem::Deinitialize()
{
	if (bEnemyDeathSubscribed)
	{
		FEventBus::Unsubscribe<FNonPlayerDiedEvent>(EnemyDeathDelegateHandle);
		EnemyDeathDelegateHandle.Reset();
		bEnemyDeathSubscribed = false;
	}

	Wallet = FPureInkWallet();
	ProcessedEnemyDrops.Reset();
	GrantedRoomResults.Reset();

	Super::Deinitialize();
}

void URoguelikeEconomySubsystem::ResetForNewRun()
{
	const int32 PreviousBalance = Wallet.Balance;
	Wallet = FPureInkWallet();
	ProcessedEnemyDrops.Reset();
	GrantedRoomResults.Reset();

	if (PreviousBalance != 0)
	{
		OnPureInkChanged.Broadcast(
			PreviousBalance,
			Wallet.Balance,
			-PreviousBalance,
			EPureInkChangeReason::NewRunReset);
	}

	UE_LOG(LogRoguelike, Log,
		TEXT("Pure Ink reset for new run. PreviousBalance=%d NewBalance=%d."),
		PreviousBalance,
		Wallet.Balance);
}

bool URoguelikeEconomySubsystem::AddPureInk(int32 Amount, EPureInkChangeReason Reason)
{
	if (Amount <= 0)
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("Pure Ink add rejected: Amount=%d Reason=%d."),
			Amount,
			static_cast<int32>(Reason));
		return false;
	}

	return ApplyBalanceDelta(Amount, Reason);
}

bool URoguelikeEconomySubsystem::TrySpendPureInk(int32 Amount, EPureInkChangeReason Reason)
{
	if (Amount <= 0)
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("Pure Ink spend rejected: Amount=%d Reason=%d."),
			Amount,
			static_cast<int32>(Reason));
		return false;
	}

	if (Wallet.Balance < Amount)
	{
		UE_LOG(LogRoguelike, Log,
			TEXT("Pure Ink spend rejected: Cost=%d Balance=%d Reason=%d."),
			Amount,
			Wallet.Balance,
			static_cast<int32>(Reason));
		return false;
	}

	return ApplyBalanceDelta(-Amount, Reason);
}

bool URoguelikeEconomySubsystem::GrantRoomResult(
	int32 MajorStageIndex,
	int32 RoomIndex,
	int32 Amount)
{
	if (MajorStageIndex < 0 || RoomIndex < 0)
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("Room result Pure Ink rejected: invalid location MajorStage=%d RoomIndex=%d."),
			MajorStageIndex,
			RoomIndex);
		return false;
	}

	if (Amount <= 0)
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("Room result Pure Ink rejected: Amount=%d MajorStage=%d RoomIndex=%d."),
			Amount,
			MajorStageIndex,
			RoomIndex);
		return false;
	}

	const uint64 ResultKey =
		(static_cast<uint64>(static_cast<uint32>(MajorStageIndex)) << 32)
		| static_cast<uint32>(RoomIndex);
	if (GrantedRoomResults.Contains(ResultKey))
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("Room result Pure Ink ignored as duplicate: MajorStage=%d RoomIndex=%d."),
			MajorStageIndex,
			RoomIndex);
		return false;
	}

	if (!AddPureInk(Amount, EPureInkChangeReason::RoomResult))
	{
		return false;
	}

	GrantedRoomResults.Add(ResultKey);
	UE_LOG(LogRoguelike, Log,
		TEXT("Room result Pure Ink granted: MajorStage=%d RoomIndex=%d Amount=%d Balance=%d."),
		MajorStageIndex,
		RoomIndex,
		Amount,
		Wallet.Balance);
	return true;
}

void URoguelikeEconomySubsystem::HandleEnemyDied(const FNonPlayerDiedEvent& InEvent)
{
	AEnemyBase* Enemy = InEvent.Victim.Get();
	if (!IsValid(Enemy))
	{
		UE_LOG(LogRoguelike, Warning, TEXT("Enemy Pure Ink drop skipped: victim is invalid."));
		return;
	}

	if (ProcessedEnemyDrops.Contains(Enemy))
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("Enemy Pure Ink drop ignored as duplicate: Enemy=%s."),
			*Enemy->GetName());
		return;
	}
	ProcessedEnemyDrops.Add(Enemy);

	int32 MajorStageIndex = INDEX_NONE;
	int32 RoomIndex = INDEX_NONE;
	if (!IsCombatRoomActive(MajorStageIndex, RoomIndex))
	{
		UE_LOG(LogRoguelike, Verbose,
			TEXT("Enemy Pure Ink drop skipped outside active Combat Room: Enemy=%s."),
			*Enemy->GetName());
		return;
	}

	const int32 DropAmount = Enemy->GetPureInkDropAmount();
	if (DropAmount <= 0)
	{
		UE_LOG(LogRoguelike, Verbose,
			TEXT("Enemy Pure Ink drop skipped because amount is %d: Enemy=%s."),
			DropAmount,
			*Enemy->GetName());
		return;
	}

	if (AddPureInk(DropAmount, EPureInkChangeReason::EnemyDrop))
	{
		UE_LOG(LogRoguelike, Log,
			TEXT("Enemy Pure Ink drop: Enemy=%s MajorStage=%d RoomIndex=%d Amount=%d Balance=%d."),
			*Enemy->GetName(),
			MajorStageIndex,
			RoomIndex,
			DropAmount,
			Wallet.Balance);
	}
}

bool URoguelikeEconomySubsystem::ApplyBalanceDelta(int32 Delta, EPureInkChangeReason Reason)
{
	if (Delta == 0)
	{
		return true;
	}

	const int64 ProposedBalance = static_cast<int64>(Wallet.Balance) + Delta;
	if (ProposedBalance < 0 || ProposedBalance > MAX_int32)
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("Pure Ink balance change rejected: Balance=%d Delta=%d Reason=%d."),
			Wallet.Balance,
			Delta,
			static_cast<int32>(Reason));
		return false;
	}

	const int32 PreviousBalance = Wallet.Balance;
	Wallet.Balance = static_cast<int32>(ProposedBalance);
	OnPureInkChanged.Broadcast(PreviousBalance, Wallet.Balance, Delta, Reason);
	UE_LOG(LogRoguelike, Log,
		TEXT("Pure Ink changed: Old=%d New=%d Delta=%d Reason=%d."),
		PreviousBalance,
		Wallet.Balance,
		Delta,
		static_cast<int32>(Reason));
	return true;
}

bool URoguelikeEconomySubsystem::IsCombatRoomActive(
	int32& OutMajorStageIndex,
	int32& OutRoomIndex) const
{
	OutMajorStageIndex = INDEX_NONE;
	OutRoomIndex = INDEX_NONE;

	UGameInstance* GameInstance = GetGameInstance();
	const URoguelikeRunFlowSubsystem* RunFlow = GameInstance
		? GameInstance->GetSubsystem<URoguelikeRunFlowSubsystem>()
		: nullptr;
	if (!RunFlow || RunFlow->GetRunState() != ERoguelikeRunState::InRoom)
	{
		return false;
	}

	const FRoguelikeRoomDefinition RoomDefinition = RunFlow->GetCurrentRoomDefinition();
	if (RoomDefinition.RoomType != ERoguelikeRoomType::Combat)
	{
		return false;
	}

	OutMajorStageIndex = RunFlow->GetCurrentMajorStageIndex();
	OutRoomIndex = RunFlow->GetCurrentRoomIndex();
	return OutMajorStageIndex >= 0 && OutRoomIndex >= 0;
}
