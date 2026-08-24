// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeRewardManager.h"

#include "Blueprint/UserWidget.h"
#include "Common/HealthComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Player/PlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "RoguelikeSystem/RoguelikeExitTrigger.h"
#include "RoguelikeSystem/RoguelikeEconomySubsystem.h"
#include "RoguelikeSystem/RoguelikeRewardWidget.h"
#include "Player/Skill/SkillComponent.h"
#include "UObject/ConstructorHelpers.h"

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
	if (ExitVisualClass)
	{
		// 生成前把视觉蓝图类传给出口，BeginPlay 时按此类生成传送门视觉
		ActiveExitTrigger->ExitVisualClass = ExitVisualClass;
	}
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

	bRewardSelectionInProgress = false;
	PendingSelectedReward = FRoguelikeRewardOption();

	if (!ResolvePlayer())
	{
		UE_LOG(LogRoguelike, Error, TEXT("Reward UI request failed: player or skill component is unavailable."));
		return;
	}

	if (DebugRewardOverrideOptions.IsEmpty())
	{
		CurrentRewardOptions = GenerateRewardOptions();
	}
	else
	{
		CurrentRewardOptions = MoveTemp(DebugRewardOverrideOptions);
		UE_LOG(LogRoguelike, Log,
			TEXT("Debug reward override consumed: Count=%d."),
			CurrentRewardOptions.Num());
	}
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

	// The base UUserWidget is non-focusable by default. Enable focus before
	// passing its Slate wrapper to UIOnly so keyboard navigation has a valid
	// focus target and the PlayerController does not log a focus error.
	ActiveRewardWidget->SetIsFocusable(true);
	ActiveRewardWidget->SetVisibility(ESlateVisibility::Visible);
	ActiveRewardWidget->SetRenderOpacity(1.0f);
	// Construct the Slate widget before populating its bindings. This makes the
	// subsequent layout prepass include the icon brushes and text blocks.
	// Reward selection is a modal screen. Put it above the display-only health
	// and skill HUD layers so mouse hit testing reaches the two buttons first.
	ActiveRewardWidget->AddToViewport(100);
	ActiveRewardWidget->SetupRewardOptions(this, CurrentRewardOptions);
	ActiveRewardWidget->ForceLayoutPrepass();
	bRewardShownForRoom = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(ActiveRewardWidget->TakeWidget());
	PlayerController->SetInputMode(InputMode);
	ActiveRewardWidget->FocusFirstOption();
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

	// Immediate rewards are part of the same pool as skill-build rewards. They
	// reuse the existing economy/health owners and do not introduce a second
	// reward system.
	Candidates.Add(MakeCurrencyOption(
		120,
		FText::FromString(TEXT("纯墨")),
		FText::FromString(TEXT("获得 120 纯墨，用于后续构筑。"))));

	if (CachedPlayer && CachedPlayer->GetHealthComponent())
	{
		const UHealthComponent* HealthComponent = CachedPlayer->GetHealthComponent();
		const float CurrentHealth = HealthComponent->GetCurrentHealth();
		const float MaxHealth = HealthComponent->GetMaxHealth();
		if (MaxHealth > CurrentHealth + KINDA_SMALL_NUMBER)
		{
			const float RecoveryAmount = FMath::Min(MaxHealth - CurrentHealth, MaxHealth * 0.35f);
			Candidates.Add(MakeHealthOption(
				RecoveryAmount,
				FText::FromString(TEXT("生命恢复")),
				FText::FromString(FString::Printf(TEXT("恢复 %.0f 点生命值。"), RecoveryAmount))));
		}
	}

	// The player always owns Q/E. The reward pool now only emits legal
	// modifiers; old UpgradeSkill/ChangeSkillForm values remain supported by
	// ApplyReward for backwards-compatible Blueprint or snapshot data.
	const auto AddModifierCandidate = [this, &Candidates](
		EPlayerSkillID SkillID,
		ESkillModifierID ModifierID,
		const TCHAR* Title,
		const TCHAR* Description)
	{
		if (CachedSkillComponent->CanApplyModifier(SkillID, ModifierID, 1))
		{
			Candidates.Add(MakeModifierOption(
				SkillID,
				ModifierID,
				1,
				FText::FromString(Title),
				FText::FromString(Description)));
		}
	};

	// Q build candidates. Extra Explosion is gated by Ink Grenade inside
	// CanApplyModifier, so it cannot be offered as a dead-end card.
	AddModifierCandidate(
		EPlayerSkillID::TripleProjectile,
		ESkillModifierID::AddProjectile,
		TEXT("投射物增幅"),
		TEXT("Q 增加一枚投射物，墨雷形态同样生效。"));
	AddModifierCandidate(
		EPlayerSkillID::TripleProjectile,
		ESkillModifierID::InkGrenade,
		TEXT("墨雷形态"),
		TEXT("Q 投射物变为延迟爆炸的墨雷。"));
	AddModifierCandidate(
		EPlayerSkillID::TripleProjectile,
		ESkillModifierID::ExtraExplosion,
		TEXT("余烬连爆"),
		TEXT("每枚 Q 墨雷在原地追加一次爆炸。"));
	AddModifierCandidate(
		EPlayerSkillID::TripleProjectile,
		ESkillModifierID::CooldownDown,
		TEXT("疾速回转"),
		TEXT("Q 冷却时间缩短 0.5 秒。"));
	AddModifierCandidate(
		EPlayerSkillID::TripleProjectile,
		ESkillModifierID::ProjectileHoming,
		TEXT("引墨"),
		TEXT("右键命中的目标获得标记，Q 三枚弹幕在飞行中修正方向追踪该目标。"));

	// E build candidates intentionally do not form an exclusive group. Twin
	// Slash and Null Ring can coexist, so the resolver can produce two hits
	// where both attack areas also erase enemy projectiles.
	AddModifierCandidate(
		EPlayerSkillID::CircularSlash,
		ESkillModifierID::TwinSlash,
		TEXT("双重环斩"),
		TEXT("E 在短暂延迟后追加一次斜向斩击，造成 80% 伤害。"));
	AddModifierCandidate(
		EPlayerSkillID::CircularSlash,
		ESkillModifierID::NullRing,
		TEXT("净墨环"),
		TEXT("E 斩击区域会抹除其中的敌方投射物。"));
	AddModifierCandidate(
		EPlayerSkillID::CircularSlash,
		ESkillModifierID::RadiusUp,
		TEXT("扩展环斩"),
		TEXT("E 斩击半径增加 60。"));
	AddModifierCandidate(
		EPlayerSkillID::CircularSlash,
		ESkillModifierID::CooldownDown,
		TEXT("回锋"),
		TEXT("E 冷却时间缩短 0.4 秒。"));

	TArray<FRoguelikeRewardOption> Options;
	const int32 DesiredOptionCount = FMath::Clamp(RewardOptionCount, 2, 3);
	while (!Candidates.IsEmpty() && Options.Num() < DesiredOptionCount)
	{
		const int32 CandidateIndex = FMath::RandRange(0, Candidates.Num() - 1);
		Options.Add(Candidates[CandidateIndex]);
		Candidates.RemoveAtSwap(CandidateIndex);
	}
	for (const FRoguelikeRewardOption& Option : Options)
	{
		UE_LOG(LogRoguelike, Log,
			TEXT("Reward generated: Title=%s Skill=%d Type=%d Modifier=%d Stack=%d Upgrade=%d Before=%.2f After=%.2f."),
			*Option.Title.ToString(),
			static_cast<int32>(Option.SkillID),
			static_cast<int32>(Option.RewardType),
			static_cast<int32>(Option.ModifierID),
			Option.StackDelta,
			static_cast<int32>(Option.UpgradeType),
			Option.BeforeValue,
			Option.AfterValue);
	}
	return Options;
}

void ARoguelikeRewardManager::SelectReward(int32 OptionIndex)
{
	if (bRewardSelectionInProgress || !CurrentRewardOptions.IsValidIndex(OptionIndex))
	{
		return;
	}
	if (!ResolvePlayer())
	{
		if (ActiveRewardWidget)
		{
			ActiveRewardWidget->SetSelectionLocked(false);
		}
		return;
	}

	bRewardSelectionInProgress = true;
	PendingSelectedReward = CurrentRewardOptions[OptionIndex];
	if (!ApplyReward(PendingSelectedReward))
	{
		UE_LOG(LogRoguelike, Warning, TEXT("Reward application failed: Index=%d Title=%s."),
			OptionIndex, *PendingSelectedReward.Title.ToString());
		if (ActiveRewardWidget)
		{
			ActiveRewardWidget->SetSelectionLocked(false);
		}
		PendingSelectedReward = FRoguelikeRewardOption();
		bRewardSelectionInProgress = false;
		return;
	}

	UE_LOG(LogRoguelike, Log, TEXT("Player selected reward: Index=%d Title=%s. Waiting for selection feedback."),
		OptionIndex, *PendingSelectedReward.Title.ToString());
	if (ActiveRewardWidget)
	{
		ActiveRewardWidget->SetSelectionFinishedCallback(
			FSimpleDelegate::CreateUObject(this, &ARoguelikeRewardManager::FinishRewardSelection));
		if (!ActiveRewardWidget->PlaySelectionFeedback(OptionIndex))
		{
			FinishRewardSelection();
		}
	}
	else
	{
		FinishRewardSelection();
	}
}

bool ARoguelikeRewardManager::DebugShowSpecificReward(const FString& RewardIdentifier)
{
	if (RewardIdentifier.TrimStartAndEnd().IsEmpty())
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("DebugShowSpecificReward requires an identifier, e.g. ProjectileHoming or 引墨."));
		return false;
	}

	if (bRewardShownForRoom || ActiveRewardWidget)
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("DebugShowSpecificReward ignored: reward UI already shown for this room."));
		return false;
	}

	if (!ResolvePlayer())
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("DebugShowSpecificReward failed: player or skill component is unavailable."));
		return false;
	}

	FRoguelikeRewardOption DebugOption;
	if (!TryBuildDebugRewardOption(RewardIdentifier, DebugOption))
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("DebugShowSpecificReward rejected unknown identifier '%s'."),
			*RewardIdentifier);
		return false;
	}

	if (DebugOption.RewardType == ERoguelikeRewardType::Modifier
		&& !CachedSkillComponent->CanApplyModifier(
			DebugOption.SkillID,
			DebugOption.ModifierID,
			DebugOption.StackDelta))
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("DebugShowSpecificReward rejected illegal modifier: Skill=%d Modifier=%d."),
			static_cast<int32>(DebugOption.SkillID),
			static_cast<int32>(DebugOption.ModifierID));
		return false;
	}

	DebugRewardOverrideOptions.Reset();
	DebugRewardOverrideOptions.Add(MoveTemp(DebugOption));
	ShowRewardAfterRoomClear();
	return bRewardShownForRoom && IsValid(ActiveRewardWidget);
}

bool ARoguelikeRewardManager::DebugSelectSpecificReward(const FString& RewardIdentifier)
{
	const FString Identifier = RewardIdentifier.TrimStartAndEnd();
	if (Identifier.IsEmpty())
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("DebugSelectSpecificReward requires an identifier, e.g. ProjectileHoming or 引墨."));
		return false;
	}

	FRoguelikeRewardOption RequestedOption;
	if (!TryBuildDebugRewardOption(Identifier, RequestedOption))
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("DebugSelectSpecificReward rejected unknown identifier '%s'."),
			*Identifier);
		return false;
	}

	if (!ResolvePlayer())
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("DebugSelectSpecificReward failed: player or skill component is unavailable."));
		return false;
	}

	if (RequestedOption.RewardType == ERoguelikeRewardType::Modifier
		&& !CachedSkillComponent->CanApplyModifier(
			RequestedOption.SkillID,
			RequestedOption.ModifierID,
			RequestedOption.StackDelta))
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("DebugSelectSpecificReward rejected illegal modifier: Skill=%d Modifier=%d."),
			static_cast<int32>(RequestedOption.SkillID),
			static_cast<int32>(RequestedOption.ModifierID));
		return false;
	}

	// If a debug card is already open, only select it when it is the exact
	// requested modifier. This keeps the command deterministic and prevents a
	// typo from silently selecting a different reward card.
	if (bRewardShownForRoom || ActiveRewardWidget)
	{
		const bool bMatchesVisibleOption = CurrentRewardOptions.Num() == 1
			&& CurrentRewardOptions[0].RewardType == RequestedOption.RewardType
			&& CurrentRewardOptions[0].SkillID == RequestedOption.SkillID
			&& CurrentRewardOptions[0].ModifierID == RequestedOption.ModifierID
			&& CurrentRewardOptions[0].StackDelta == RequestedOption.StackDelta;
		if (!bMatchesVisibleOption)
		{
			UE_LOG(LogRoguelike, Warning,
				TEXT("DebugSelectSpecificReward rejected: another reward UI is already shown."));
			return false;
		}

		SelectReward(0);
		return true;
	}

	DebugRewardOverrideOptions.Reset();
	DebugRewardOverrideOptions.Add(MoveTemp(RequestedOption));
	ShowRewardAfterRoomClear();
	if (CurrentRewardOptions.Num() != 1 || !IsValid(ActiveRewardWidget))
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("DebugSelectSpecificReward failed: reward UI could not be opened."));
		return false;
	}

	SelectReward(0);
	return true;
}

void ARoguelikeRewardManager::FinishRewardSelection()
{
	if (!bRewardSelectionInProgress)
	{
		return;
	}

	const FRoguelikeRewardOption CompletedReward = PendingSelectedReward;
	CloseRewardUI();
	UE_LOG(LogRoguelike, Log, TEXT("Reward UI closed after selection feedback; gameplay input restored."));
	OnRewardApplied.Broadcast(CompletedReward);
	PendingSelectedReward = FRoguelikeRewardOption();
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
	if (!IsValid(CachedSkillComponent)
		&& Reward.RewardType != ERoguelikeRewardType::Currency
		&& Reward.RewardType != ERoguelikeRewardType::Health)
	{
		return false;
	}

	switch (Reward.RewardType)
	{
	case ERoguelikeRewardType::GainSkill:
	{
		const bool bAdded = CachedSkillComponent->AddSkillToFirstEmptySlot(Reward.SkillID);
		if (!bAdded)
		{
			return false;
		}

		UE_LOG(LogRoguelike, Log, TEXT("Reward applied: GainSkill Skill=%d."), static_cast<int32>(Reward.SkillID));
		return true;
	}
	case ERoguelikeRewardType::UpgradeSkill:
	{
		if (!CachedSkillComponent->CanApplyUpgrade(Reward.SkillID, Reward.UpgradeType))
		{
			return false;
		}

		CachedSkillComponent->ApplySkillUpgrade(Reward.SkillID, Reward.UpgradeType);
		UE_LOG(LogRoguelike, Log, TEXT("Reward applied: Upgrade Skill=%d Type=%d."),
			static_cast<int32>(Reward.SkillID), static_cast<int32>(Reward.UpgradeType));
		return true;
	}
	case ERoguelikeRewardType::Modifier:
	{
		if (!CachedSkillComponent->CanApplyModifier(Reward.SkillID, Reward.ModifierID, Reward.StackDelta))
		{
			UE_LOG(LogRoguelike, Warning,
				TEXT("Modifier reward rejected: Skill=%d Modifier=%d StackDelta=%d."),
				static_cast<int32>(Reward.SkillID),
				static_cast<int32>(Reward.ModifierID),
				Reward.StackDelta);
			return false;
		}

		const int32 BeforeStack = CachedSkillComponent->GetModifierStack(Reward.SkillID, Reward.ModifierID);
		const bool bApplied = CachedSkillComponent->ApplyModifier(
			Reward.SkillID,
			Reward.ModifierID,
			Reward.StackDelta);
		if (!bApplied)
		{
			return false;
		}

		const int32 AfterStack = CachedSkillComponent->GetModifierStack(Reward.SkillID, Reward.ModifierID);
		const FResolvedSkillSpec ResolvedSpec = CachedSkillComponent->ResolveSkillSpec(Reward.SkillID);
		UE_LOG(LogRoguelike, Log,
			TEXT("Reward applied: Modifier Skill=%d Modifier=%d Stack=%d->%d Resolved(Projectiles=%d Explosions=%d Hits=%d Radius=%.0f Cooldown=%.2f)."),
			static_cast<int32>(Reward.SkillID),
			static_cast<int32>(Reward.ModifierID),
			BeforeStack,
			AfterStack,
			ResolvedSpec.ProjectileCount,
			ResolvedSpec.ExplosionCount,
			ResolvedSpec.HitCount,
			ResolvedSpec.Radius,
			ResolvedSpec.Cooldown);
		return true;
	}
	case ERoguelikeRewardType::ChangeSkillForm:
	{
		if (!CachedSkillComponent->ApplySkillForm(Reward.SkillID, Reward.TargetSkillForm))
		{
			return false;
		}

		UE_LOG(LogRoguelike, Log, TEXT("Reward applied: ChangeSkillForm Skill=%d From=%d To=%d."),
			static_cast<int32>(Reward.SkillID),
			static_cast<int32>(Reward.CurrentSkillForm),
			static_cast<int32>(Reward.TargetSkillForm));
		return true;
	}
	case ERoguelikeRewardType::Currency:
	{
		UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		URoguelikeEconomySubsystem* Economy = GameInstance
			? GameInstance->GetSubsystem<URoguelikeEconomySubsystem>()
			: nullptr;
		const int32 Amount = Reward.CurrencyAmount > 0 ? Reward.CurrencyAmount : Reward.StackDelta;
		if (!Economy || Amount <= 0 || !Economy->AddPureInk(Amount, EPureInkChangeReason::RewardChoice))
		{
			return false;
		}

		UE_LOG(LogRoguelike, Log, TEXT("Reward applied: Currency Amount=%d."), Amount);
		return true;
	}
	case ERoguelikeRewardType::Health:
	{
		UHealthComponent* HealthComponent = CachedPlayer ? CachedPlayer->GetHealthComponent() : nullptr;
		if (!HealthComponent)
		{
			return false;
		}

		const float BeforeHealth = HealthComponent->GetCurrentHealth();
		const float RestoreAmount = Reward.HealthRestoreAmount > 0.0f
			? Reward.HealthRestoreAmount
			: FMath::Max(0.0f, Reward.AfterValue - BeforeHealth);
		const float NewHealth = FMath::Min(HealthComponent->GetMaxHealth(), BeforeHealth + RestoreAmount);
		if (NewHealth <= BeforeHealth + KINDA_SMALL_NUMBER)
		{
			return false;
		}

		HealthComponent->SetCurrentHealth(NewHealth);
		UE_LOG(LogRoguelike, Log, TEXT("Reward applied: Health %.1f -> %.1f."), BeforeHealth, NewHealth);
		return true;
	}
	default:
		return false;
	}
}

void ARoguelikeRewardManager::CloseRewardUI()
{
	if (ActiveRewardWidget)
	{
		ActiveRewardWidget->SetSelectionFinishedCallback(FSimpleDelegate());
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
		// Gameplay keeps the cursor visible for aiming and mouse-driven attacks.
		// Do not hide it when the modal reward UI gives input back to the game.
		PlayerController->SetShowMouseCursor(true);
		PlayerController->SetIgnoreMoveInput(false);
		PlayerController->SetIgnoreLookInput(false);
	}
}

FRoguelikeRewardOption ARoguelikeRewardManager::MakeOption(
	ERoguelikeRewardType RewardType,
	EPlayerSkillID SkillID,
	ESkillUpgradeType UpgradeType,
	EPlayerSkillForm TargetSkillForm,
	const FText& Title,
	const FText& Description) const
{
	FRoguelikeRewardOption Option;
	Option.RewardType = RewardType;
	Option.SkillID = SkillID;
	Option.UpgradeType = UpgradeType;
	Option.ModifierID = ESkillModifierID::None;
	Option.StackDelta = 0;
	Option.CurrentSkillForm = CachedSkillComponent
		? CachedSkillComponent->GetSkillForm(SkillID)
		: EPlayerSkillForm::Default;
	Option.TargetSkillForm = RewardType == ERoguelikeRewardType::ChangeSkillForm
		? TargetSkillForm
		: Option.CurrentSkillForm;
	Option.Title = Title;
	Option.Description = Description;
	PopulateRewardPresentation(Option);
	return Option;
}

FRoguelikeRewardOption ARoguelikeRewardManager::MakeCurrencyOption(
	int32 Amount,
	const FText& Title,
	const FText& Description) const
{
	FRoguelikeRewardOption Option;
	Option.RewardType = ERoguelikeRewardType::Currency;
	Option.StackDelta = FMath::Max(0, Amount);
	Option.CurrencyAmount = FMath::Max(0, Amount);
	Option.Title = Title;
	Option.Description = Description;
	PopulateRewardPresentation(Option);
	return Option;
}

FRoguelikeRewardOption ARoguelikeRewardManager::MakeHealthOption(
	float RecoveryAmount,
	const FText& Title,
	const FText& Description) const
{
	FRoguelikeRewardOption Option;
	Option.RewardType = ERoguelikeRewardType::Health;
	Option.HealthRestoreAmount = FMath::Max(0.0f, RecoveryAmount);
	Option.Title = Title;
	Option.Description = Description;
	if (CachedPlayer && CachedPlayer->GetHealthComponent())
	{
		const UHealthComponent* HealthComponent = CachedPlayer->GetHealthComponent();
		Option.BeforeValue = HealthComponent->GetCurrentHealth();
		Option.AfterValue = FMath::Min(HealthComponent->GetMaxHealth(), Option.BeforeValue + Option.HealthRestoreAmount);
	}
	PopulateRewardPresentation(Option);
	return Option;
}

FRoguelikeRewardOption ARoguelikeRewardManager::MakeModifierOption(
	EPlayerSkillID SkillID,
	ESkillModifierID ModifierID,
	int32 StackDelta,
	const FText& Title,
	const FText& Description) const
{
	FRoguelikeRewardOption Option;
	Option.RewardType = ERoguelikeRewardType::Modifier;
	Option.SkillID = SkillID;
	Option.UpgradeType = ESkillUpgradeType::None;
	Option.ModifierID = ModifierID;
	Option.StackDelta = StackDelta;
	Option.CurrentSkillForm = CachedSkillComponent
		? CachedSkillComponent->GetSkillForm(SkillID)
		: EPlayerSkillForm::Default;
	Option.TargetSkillForm = Option.CurrentSkillForm;
	Option.Title = Title;
	Option.Description = Description;
	FillModifierPreview(Option);
	PopulateRewardPresentation(Option);
	return Option;
}

bool ARoguelikeRewardManager::TryBuildDebugRewardOption(
	const FString& RewardIdentifier,
	FRoguelikeRewardOption& OutOption) const
{
	const FString Identifier = RewardIdentifier.TrimStartAndEnd();
	const auto Matches = [&Identifier](std::initializer_list<const TCHAR*> Aliases)
	{
		for (const TCHAR* Alias : Aliases)
		{
			if (Identifier.Equals(Alias, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	};

	if (Matches({TEXT("ProjectileHoming"), TEXT("Homing"), TEXT("引墨")}))
	{
		OutOption = MakeModifierOption(
			EPlayerSkillID::TripleProjectile,
			ESkillModifierID::ProjectileHoming,
			1,
			FText::FromString(TEXT("引墨")),
			FText::FromString(TEXT("右键命中的目标获得标记，Q 三枚弹幕在飞行中修正方向追踪该目标。")));
		return true;
	}

	if (Matches({TEXT("AddProjectile"), TEXT("Q.AddProjectile"), TEXT("投射物增幅")}))
	{
		OutOption = MakeModifierOption(
			EPlayerSkillID::TripleProjectile,
			ESkillModifierID::AddProjectile,
			1,
			FText::FromString(TEXT("投射物增幅")),
			FText::FromString(TEXT("Q 增加一枚投射物，墨雷形态同样生效。")));
		return true;
	}

	if (Matches({TEXT("InkGrenade"), TEXT("Q.InkGrenade"), TEXT("墨雷形态")}))
	{
		OutOption = MakeModifierOption(
			EPlayerSkillID::TripleProjectile,
			ESkillModifierID::InkGrenade,
			1,
			FText::FromString(TEXT("墨雷形态")),
			FText::FromString(TEXT("Q 投射物变为延迟爆炸的墨雷。")));
		return true;
	}

	if (Matches({TEXT("ExtraExplosion"), TEXT("Q.ExtraExplosion"), TEXT("余烬连爆")}))
	{
		OutOption = MakeModifierOption(
			EPlayerSkillID::TripleProjectile,
			ESkillModifierID::ExtraExplosion,
			1,
			FText::FromString(TEXT("余烬连爆")),
			FText::FromString(TEXT("每枚 Q 墨雷在原地追加一次爆炸。")));
		return true;
	}

	if (Matches({TEXT("Q.CooldownDown"), TEXT("QCooldownDown"), TEXT("疾速回转")}))
	{
		OutOption = MakeModifierOption(
			EPlayerSkillID::TripleProjectile,
			ESkillModifierID::CooldownDown,
			1,
			FText::FromString(TEXT("疾速回转")),
			FText::FromString(TEXT("Q 冷却时间缩短 0.5 秒。")));
		return true;
	}

	if (Matches({TEXT("TwinSlash"), TEXT("E.TwinSlash"), TEXT("双重环斩")}))
	{
		OutOption = MakeModifierOption(
			EPlayerSkillID::CircularSlash,
			ESkillModifierID::TwinSlash,
			1,
			FText::FromString(TEXT("双重环斩")),
			FText::FromString(TEXT("E 在短暂延迟后追加一次斜向斩击，造成 80% 伤害。")));
		return true;
	}

	if (Matches({TEXT("NullRing"), TEXT("E.NullRing"), TEXT("净墨环")}))
	{
		OutOption = MakeModifierOption(
			EPlayerSkillID::CircularSlash,
			ESkillModifierID::NullRing,
			1,
			FText::FromString(TEXT("净墨环")),
			FText::FromString(TEXT("E 斩击区域会抹除其中的敌方投射物。")));
		return true;
	}

	if (Matches({TEXT("RadiusUp"), TEXT("E.RadiusUp"), TEXT("扩展环斩")}))
	{
		OutOption = MakeModifierOption(
			EPlayerSkillID::CircularSlash,
			ESkillModifierID::RadiusUp,
			1,
			FText::FromString(TEXT("扩展环斩")),
			FText::FromString(TEXT("E 斩击半径增加 60。")));
		return true;
	}

	if (Matches({TEXT("E.CooldownDown"), TEXT("ECooldownDown"), TEXT("回锋")}))
	{
		OutOption = MakeModifierOption(
			EPlayerSkillID::CircularSlash,
			ESkillModifierID::CooldownDown,
			1,
			FText::FromString(TEXT("回锋")),
			FText::FromString(TEXT("E 冷却时间缩短 0.4 秒。")));
		return true;
	}

	return false;
}

void ARoguelikeRewardManager::FillModifierPreview(FRoguelikeRewardOption& Option) const
{
	if (!IsValid(CachedSkillComponent))
	{
		return;
	}

	const FResolvedSkillSpec BeforeSpec = CachedSkillComponent->ResolveSkillSpec(Option.SkillID);
	switch (Option.ModifierID)
	{
	case ESkillModifierID::AddProjectile:
		Option.BeforeValue = static_cast<float>(CachedSkillComponent->GetTripleProjectileCount());
		Option.AfterValue = FMath::Min(7.0f, Option.BeforeValue + static_cast<float>(Option.StackDelta));
		break;
	case ESkillModifierID::InkGrenade:
		Option.BeforeValue = BeforeSpec.PayloadType == ESkillPayloadType::InkGrenade ? 1.0f : 0.0f;
		Option.AfterValue = 1.0f;
		break;
	case ESkillModifierID::ExtraExplosion:
		Option.BeforeValue = static_cast<float>(BeforeSpec.ExplosionCount);
		Option.AfterValue = Option.BeforeValue + static_cast<float>(Option.StackDelta);
		break;
	case ESkillModifierID::TwinSlash:
		Option.BeforeValue = static_cast<float>(BeforeSpec.HitCount);
		Option.AfterValue = Option.BeforeValue + static_cast<float>(Option.StackDelta);
		break;
	case ESkillModifierID::NullRing:
		Option.BeforeValue = BeforeSpec.bNullifyEnemyProjectiles ? 1.0f : 0.0f;
		Option.AfterValue = 1.0f;
		break;
	case ESkillModifierID::RadiusUp:
		Option.BeforeValue = CachedSkillComponent->GetCircularSlashRadius();
		Option.AfterValue = FMath::Min(440.0f, Option.BeforeValue + 60.0f * static_cast<float>(Option.StackDelta));
		break;
	case ESkillModifierID::CooldownDown:
		Option.BeforeValue = CachedSkillComponent->GetSkillCooldown(Option.SkillID);
		Option.AfterValue = Option.SkillID == EPlayerSkillID::TripleProjectile
			? FMath::Max(2.0f, Option.BeforeValue - 0.5f * static_cast<float>(Option.StackDelta))
			: FMath::Max(1.6f, Option.BeforeValue - 0.4f * static_cast<float>(Option.StackDelta));
		break;
	case ESkillModifierID::ProjectileHoming:
		Option.BeforeValue = BeforeSpec.bEnableHoming ? 1.0f : 0.0f;
		Option.AfterValue = 1.0f;
		break;
	default:
		break;
	}
}

void ARoguelikeRewardManager::PopulateRewardPresentation(FRoguelikeRewardOption& Option) const
{
	if (Option.ShortDescription.IsEmpty())
	{
		Option.ShortDescription = Option.Description;
	}

	if (Option.RewardType == ERoguelikeRewardType::Currency)
	{
		Option.PrimaryValue = FText::FromString(FString::Printf(TEXT("+%d"), Option.CurrencyAmount));
		Option.BuildType = FText::GetEmpty();
		Option.TargetSkill = FText::GetEmpty();
		Option.RewardIcon = LoadObject<UTexture2D>(
			nullptr,
			TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Reward_PureInk.T_UI_Reward_PureInk"));
		return;
	}

	if (Option.RewardType == ERoguelikeRewardType::Health)
	{
		const float MaxHealth = CachedPlayer && CachedPlayer->GetHealthComponent()
			? CachedPlayer->GetHealthComponent()->GetMaxHealth()
			: 0.0f;
		const float RecoveryPercent = MaxHealth > KINDA_SMALL_NUMBER
			? (Option.HealthRestoreAmount / MaxHealth) * 100.0f
			: 0.0f;
		Option.PrimaryValue = FText::FromString(FString::Printf(TEXT("+%.0f%%"), RecoveryPercent));
		Option.BuildType = FText::GetEmpty();
		Option.TargetSkill = FText::GetEmpty();
		Option.RewardIcon = LoadObject<UTexture2D>(
			nullptr,
			TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Reward_Health.T_UI_Reward_Health"));
		return;
	}

	const TCHAR* InputSlot = Option.SkillID == EPlayerSkillID::CircularSlash ? TEXT("E") : TEXT("Q");
	const TCHAR* SkillName = Option.SkillID == EPlayerSkillID::CircularSlash ? TEXT("环斩") : TEXT("三连墨矢");
	Option.TargetSkill = FText::FromString(FString::Printf(TEXT("%s  %s"), InputSlot, SkillName));
	Option.BuildType = FText::FromString(TEXT("强化构筑"));

	const TCHAR* IconPath = Option.SkillID == EPlayerSkillID::CircularSlash
		? TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_TwinSlash.T_UI_Build_TwinSlash")
		: TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_ProjectileCount.T_UI_Build_ProjectileCount");

	switch (Option.ModifierID)
	{
	case ESkillModifierID::AddProjectile:
		Option.OldValue = FText::FromString(FString::Printf(TEXT("%.0f 枚"), Option.BeforeValue));
		Option.NewValue = FText::FromString(FString::Printf(TEXT("%.0f 枚"), Option.AfterValue));
		IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_ProjectileCount.T_UI_Build_ProjectileCount");
		break;
	case ESkillModifierID::InkGrenade:
		Option.OldValue = FText::FromString(TEXT("普通墨矢"));
		Option.NewValue = FText::FromString(TEXT("墨雷"));
		IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_InkGrenade.T_UI_Build_InkGrenade");
		break;
	case ESkillModifierID::ExtraExplosion:
		Option.OldValue = FText::FromString(FString::Printf(TEXT("%.0f 次"), Option.BeforeValue));
		Option.NewValue = FText::FromString(FString::Printf(TEXT("%.0f 次"), Option.AfterValue));
		IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_ExtraExplosion.T_UI_Build_ExtraExplosion");
		break;
	case ESkillModifierID::TwinSlash:
		Option.OldValue = FText::FromString(FString::Printf(TEXT("%.0f 段"), Option.BeforeValue));
		Option.NewValue = FText::FromString(FString::Printf(TEXT("%.0f 段"), Option.AfterValue));
		IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_TwinSlash.T_UI_Build_TwinSlash");
		break;
	case ESkillModifierID::NullRing:
		Option.OldValue = FText::FromString(Option.BeforeValue > 0.5f ? TEXT("开启") : TEXT("关闭"));
		Option.NewValue = FText::FromString(TEXT("开启"));
		IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_ProjectileErase.T_UI_Build_ProjectileErase");
		break;
	case ESkillModifierID::RadiusUp:
		Option.OldValue = FText::FromString(FString::Printf(TEXT("%.0f"), Option.BeforeValue));
		Option.NewValue = FText::FromString(FString::Printf(TEXT("%.0f"), Option.AfterValue));
		IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_Radius.T_UI_Build_Radius");
		break;
	case ESkillModifierID::CooldownDown:
		Option.OldValue = FText::FromString(FString::Printf(TEXT("%.1f 秒"), Option.BeforeValue));
		Option.NewValue = FText::FromString(FString::Printf(TEXT("%.1f 秒"), Option.AfterValue));
		IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_Cooldown.T_UI_Build_Cooldown");
		break;
	case ESkillModifierID::ProjectileHoming:
		Option.OldValue = FText::FromString(Option.BeforeValue > 0.5f ? TEXT("开启") : TEXT("关闭"));
		Option.NewValue = FText::FromString(TEXT("开启"));
		// No dedicated homing artwork exists yet; keep the icon contract stable
		// with the existing projectile-build placeholder until final art arrives.
		IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_ProjectileCount.T_UI_Build_ProjectileCount");
		break;
	default:
		break;
	}

	if (!Option.OldValue.IsEmpty() || !Option.NewValue.IsEmpty())
	{
		Option.PrimaryValue = FText::FromString(FString::Printf(TEXT("%s → %s"),
			*Option.OldValue.ToString(), *Option.NewValue.ToString()));
	}
	Option.RewardIcon = LoadObject<UTexture2D>(nullptr, IconPath);
}
