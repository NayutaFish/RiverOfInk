// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeRewardManager.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Player/PlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "RoguelikeSystem/RoguelikeExitTrigger.h"
#include "RoguelikeSystem/RoguelikeRewardWidget.h"
#include "Player/Skill/SkillComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY(LogRoguelike);

ARoguelikeRewardManager::ARoguelikeRewardManager()
{
	PrimaryActorTick.bCanEverTick = false;
	ExitTriggerClass = ARoguelikeExitTrigger::StaticClass();

	// 关卡实例未手动赋值时，使用项目约定的奖励控件作为安全回退。
	static ConstructorHelpers::FClassFinder<URoguelikeRewardWidget> RewardWidgetAsset(
		TEXT("/Game/Blueprint/GameSystem/GameMode/Roguelike/WBP_RoguelikeReward"));
	if (RewardWidgetAsset.Succeeded())
	{
		RewardWidgetClass = RewardWidgetAsset.Class;
	}
}

void ARoguelikeRewardManager::BeginPlay()
{
	Super::BeginPlay();
	ResolvePlayer();
	EnsureExitTrigger();
}

void ARoguelikeRewardManager::EnsureExitTrigger()
{
	if (!bAutoCreateExitTrigger || IsValid(ActiveExitTrigger))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogRoguelike, Error, TEXT("Cannot create roguelike exit: World is unavailable."));
		return;
	}

	TArray<AActor*> ExistingExits;
	UGameplayStatics::GetAllActorsOfClass(World, ARoguelikeExitTrigger::StaticClass(), ExistingExits);
	if (ExistingExits.Num() > 0)
	{
		ActiveExitTrigger = Cast<ARoguelikeExitTrigger>(ExistingExits[0]);
		if (IsValid(ActiveExitTrigger) && !IsValid(ActiveExitTrigger->RewardManager))
		{
			ActiveExitTrigger->RewardManager = this;
		}
		return;
	}

	if (!ExitTriggerClass)
	{
		UE_LOG(LogRoguelike, Error, TEXT("Cannot create roguelike exit: ExitTriggerClass is not configured."));
		return;
	}

	const FVector SpawnLocation = GetActorLocation() + ExitTriggerSpawnOffset;
	const FTransform SpawnTransform(GetActorRotation(), SpawnLocation);
	ActiveExitTrigger = World->SpawnActorDeferred<ARoguelikeExitTrigger>(
		ExitTriggerClass,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn
	);
	if (!IsValid(ActiveExitTrigger))
	{
		UE_LOG(LogRoguelike, Error, TEXT("Failed to create roguelike exit trigger."));
		return;
	}

	ActiveExitTrigger->RewardManager = this;
	UGameplayStatics::FinishSpawningActor(ActiveExitTrigger, SpawnTransform);
	UE_LOG(LogRoguelike, Log, TEXT("Roguelike exit trigger ready: %s at %s."),
		*ActiveExitTrigger->GetName(), *SpawnLocation.ToString());
}

void ARoguelikeRewardManager::ShowRewardAfterRoomClear()
{
	if (bRewardShownForRoom || ActiveRewardWidget)
	{
		UE_LOG(LogRoguelike, Verbose, TEXT("Reward UI request ignored: already shown for this room."));
		return;
	}

	if (!ResolvePlayer())
	{
		UE_LOG(LogRoguelike, Error, TEXT("Reward UI request failed: player or skill component is unavailable."));
		return;
	}

	CurrentRewardOptions = GenerateRewardOptions();
	if (CurrentRewardOptions.IsEmpty())
	{
		UE_LOG(LogRoguelike, Warning, TEXT("Room clear produced no legal rewards."));
		return;
	}

	UE_LOG(LogRoguelike, Log, TEXT("Reward options generated: Count=%d."), CurrentRewardOptions.Num());

	if (!RewardWidgetClass)
	{
		UE_LOG(LogRoguelike, Error, TEXT("RewardWidgetClass is not configured. Create WBP_RoguelikeReward from RoguelikeRewardWidget and assign it."));
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController)
	{
		return;
	}

	ActiveRewardWidget = CreateWidget<URoguelikeRewardWidget>(PlayerController, RewardWidgetClass);
	if (!ActiveRewardWidget)
	{
		return;
	}

	ActiveRewardWidget->SetupRewardOptions(this, CurrentRewardOptions);
	ActiveRewardWidget->AddToViewport();
	// The base UUserWidget is non-focusable by default. Enable focus before
	// passing its Slate wrapper to UIOnly so keyboard navigation has a valid
	// focus target and the PlayerController does not log a focus error.
	ActiveRewardWidget->SetIsFocusable(true);
	bRewardShownForRoom = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(ActiveRewardWidget->TakeWidget());
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(true);
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);
	UE_LOG(LogRoguelike, Log, TEXT("Reward UI shown with %d option(s)."), CurrentRewardOptions.Num());
}

TArray<FRoguelikeRewardOption> ARoguelikeRewardManager::GenerateRewardOptions()
{
	TArray<FRoguelikeRewardOption> Candidates;
	if (!ResolvePlayer())
	{
		return Candidates;
	}

	if (!CachedSkillComponent->HasSkill(EPlayerSkillID::CircularSlash) && CachedSkillComponent->HasEmptySkillSlot())
	{
		Candidates.Add(MakeOption(ERoguelikeRewardType::GainSkill, EPlayerSkillID::CircularSlash, ESkillUpgradeType::None,
			FText::FromString(TEXT("Gain Circular Slash")), FText::FromString(TEXT("Release a damaging circle around the player."))));
	}
	if (CachedSkillComponent->CanApplyUpgrade(EPlayerSkillID::TripleProjectile, ESkillUpgradeType::Mechanic))
	{
		Candidates.Add(MakeOption(ERoguelikeRewardType::UpgradeSkill, EPlayerSkillID::TripleProjectile, ESkillUpgradeType::Mechanic,
			FText::FromString(TEXT("Projectile Barrage")), FText::FromString(TEXT("Triple Projectile fires 2 additional projectiles."))));
	}
	if (CachedSkillComponent->CanApplyUpgrade(EPlayerSkillID::TripleProjectile, ESkillUpgradeType::Cooldown))
	{
		Candidates.Add(MakeOption(ERoguelikeRewardType::UpgradeSkill, EPlayerSkillID::TripleProjectile, ESkillUpgradeType::Cooldown,
			FText::FromString(TEXT("Quick Reload")), FText::FromString(TEXT("Triple Projectile cooldown is reduced by 0.5 seconds."))));
	}
	if (CachedSkillComponent->CanApplyUpgrade(EPlayerSkillID::CircularSlash, ESkillUpgradeType::Mechanic))
	{
		Candidates.Add(MakeOption(ERoguelikeRewardType::UpgradeSkill, EPlayerSkillID::CircularSlash, ESkillUpgradeType::Mechanic,
			FText::FromString(TEXT("Expanded Slash")), FText::FromString(TEXT("Circular Slash radius is increased by 60."))));
	}
	if (CachedSkillComponent->CanApplyUpgrade(EPlayerSkillID::CircularSlash, ESkillUpgradeType::Cooldown))
	{
		Candidates.Add(MakeOption(ERoguelikeRewardType::UpgradeSkill, EPlayerSkillID::CircularSlash, ESkillUpgradeType::Cooldown,
			FText::FromString(TEXT("Swift Recovery")), FText::FromString(TEXT("Circular Slash cooldown is reduced by 0.4 seconds."))));
	}

	TArray<FRoguelikeRewardOption> Options;
	while (!Candidates.IsEmpty() && Options.Num() < 2)
	{
		const int32 CandidateIndex = FMath::RandRange(0, Candidates.Num() - 1);
		Options.Add(Candidates[CandidateIndex]);
		Candidates.RemoveAtSwap(CandidateIndex);
	}
	return Options;
}

void ARoguelikeRewardManager::SelectReward(int32 OptionIndex)
{
	if (bRewardSelectionInProgress || !CurrentRewardOptions.IsValidIndex(OptionIndex) || !ResolvePlayer())
	{
		return;
	}

	bRewardSelectionInProgress = true;
	const FRoguelikeRewardOption Reward = CurrentRewardOptions[OptionIndex];
	if (!ApplyReward(Reward))
	{
		UE_LOG(LogRoguelike, Warning, TEXT("Reward application failed: Index=%d Title=%s."),
			OptionIndex, *Reward.Title.ToString());
		bRewardSelectionInProgress = false;
		return;
	}

	UE_LOG(LogRoguelike, Log, TEXT("Player selected reward: Index=%d Title=%s."), OptionIndex, *Reward.Title.ToString());
	CloseRewardUI();
	UE_LOG(LogRoguelike, Log, TEXT("Reward UI closed; gameplay input restored."));
	OnRewardApplied.Broadcast(Reward);
	bRewardSelectionInProgress = false;
}

bool ARoguelikeRewardManager::ResolvePlayer()
{
	if (IsValid(CachedPlayer) && IsValid(CachedSkillComponent))
	{
		return true;
	}

	CachedPlayer = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	CachedSkillComponent = CachedPlayer ? CachedPlayer->SkillComponent : nullptr;
	if (!CachedSkillComponent)
	{
		UE_LOG(LogRoguelike, Verbose, TEXT("Hikari player is not ready while resolving reward owner."));
		return false;
	}
	return true;
}

bool ARoguelikeRewardManager::ApplyReward(const FRoguelikeRewardOption& Reward)
{
	if (!IsValid(CachedSkillComponent))
	{
		return false;
	}

	if (Reward.RewardType == ERoguelikeRewardType::GainSkill)
	{
		const bool bAdded = CachedSkillComponent->AddSkillToFirstEmptySlot(Reward.SkillID);
		if (!bAdded)
		{
			return false;
		}

		UE_LOG(LogRoguelike, Log, TEXT("Reward applied: GainSkill Skill=%d."), static_cast<int32>(Reward.SkillID));
	}
	else
	{
		if (!CachedSkillComponent->CanApplyUpgrade(Reward.SkillID, Reward.UpgradeType))
		{
			return false;
		}

		CachedSkillComponent->ApplySkillUpgrade(Reward.SkillID, Reward.UpgradeType);
		UE_LOG(LogRoguelike, Log, TEXT("Reward applied: Upgrade Skill=%d Type=%d."),
			static_cast<int32>(Reward.SkillID), static_cast<int32>(Reward.UpgradeType));
	}

	return true;
}

void ARoguelikeRewardManager::CloseRewardUI()
{
	if (ActiveRewardWidget)
	{
		ActiveRewardWidget->RemoveFromParent();
		ActiveRewardWidget = nullptr;
	}
	CurrentRewardOptions.Empty();

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		FInputModeGameOnly InputMode;
		// UIOnly releases the viewport capture. Preserve the first left-click
		// after switching back so it reaches Enhanced Input instead of being
		// consumed solely to recapture the viewport.
		InputMode.SetConsumeCaptureMouseDown(false);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetShowMouseCursor(false);
		PlayerController->SetIgnoreMoveInput(false);
		PlayerController->SetIgnoreLookInput(false);
	}
}

FRoguelikeRewardOption ARoguelikeRewardManager::MakeOption(ERoguelikeRewardType RewardType, EPlayerSkillID SkillID, ESkillUpgradeType UpgradeType, const FText& Title, const FText& Description) const
{
	FRoguelikeRewardOption Option;
	Option.RewardType = RewardType;
	Option.SkillID = SkillID;
	Option.UpgradeType = UpgradeType;
	Option.Title = Title;
	Option.Description = Description;
	return Option;
}
