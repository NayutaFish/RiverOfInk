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
		TEXT("Add Projectile"),
		TEXT("Q fires one additional projectile or Ink Grenade."));
	AddModifierCandidate(
		EPlayerSkillID::TripleProjectile,
		ESkillModifierID::InkGrenade,
		TEXT("Ink Grenade"),
		TEXT("Q projectiles become thrown ink grenades that explode on fuse."));
	AddModifierCandidate(
		EPlayerSkillID::TripleProjectile,
		ESkillModifierID::ExtraExplosion,
		TEXT("Extra Explosion"),
		TEXT("Each Q ink grenade explodes one additional time at the same location."));
	AddModifierCandidate(
		EPlayerSkillID::TripleProjectile,
		ESkillModifierID::CooldownDown,
		TEXT("Quick Reload"),
		TEXT("Q cooldown is reduced by 0.5 seconds."));

	// E build candidates intentionally do not form an exclusive group. Twin
	// Slash and Null Ring can coexist, so the resolver can produce two hits
	// where both attack areas also erase enemy projectiles.
	AddModifierCandidate(
		EPlayerSkillID::CircularSlash,
		ESkillModifierID::TwinSlash,
		TEXT("Twin Slash"),
		TEXT("E repeats after a short delay with an angled second hit for 80% damage."));
	AddModifierCandidate(
		EPlayerSkillID::CircularSlash,
		ESkillModifierID::NullRing,
		TEXT("Null Ring"),
		TEXT("E erases marked enemy projectiles inside each slash area."));
	AddModifierCandidate(
		EPlayerSkillID::CircularSlash,
		ESkillModifierID::RadiusUp,
		TEXT("Expanded Slash"),
		TEXT("E radius is increased by 60."));
	AddModifierCandidate(
		EPlayerSkillID::CircularSlash,
		ESkillModifierID::CooldownDown,
		TEXT("Swift Recovery"),
		TEXT("E cooldown is reduced by 0.4 seconds."));

	TArray<FRoguelikeRewardOption> Options;
	while (!Candidates.IsEmpty() && Options.Num() < 2)
	{
		const int32 CandidateIndex = FMath::RandRange(0, Candidates.Num() - 1);
		Options.Add(Candidates[CandidateIndex]);
		Candidates.RemoveAtSwap(CandidateIndex);
	}
	for (const FRoguelikeRewardOption& Option : Options)
	{
		UE_LOG(LogRoguelike, Log,
			TEXT("Reward generated: Title=%s Skill=%d Type=%d Modifier=%d Stack=%d Upgrade=%d CurrentForm=%d TargetForm=%d Before=%.2f After=%.2f."),
			*Option.Title.ToString(),
			static_cast<int32>(Option.SkillID),
			static_cast<int32>(Option.RewardType),
			static_cast<int32>(Option.ModifierID),
			Option.StackDelta,
			static_cast<int32>(Option.UpgradeType),
			static_cast<int32>(Option.CurrentSkillForm),
			static_cast<int32>(Option.TargetSkillForm),
			Option.BeforeValue,
			Option.AfterValue);
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
	default:
		return false;
	}
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
	return Option;
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
	default:
		break;
	}
}
