// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "PlayerSkillWidget.generated.h"

class APlayerCharacter;
class USkillComponent;
class UBorder;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UTexture2D;

/**
 * First-pass fixed Q/E skill HUD.
 *
 * The native widget tree keeps the slice usable without a Blueprint asset.
 * A Blueprint subclass can replace the tree later while the skill component
 * remains the single source of truth for levels and cooldowns.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API UPlayerSkillWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind the HUD to the current locally controlled player. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill")
	void InitializeForPlayer(APlayerCharacter* InPlayer);

	/** Refresh slot labels, icons, levels, and cooldown state. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill")
	void RefreshSkills();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildDefaultWidgetTree();
	void BindSkillEvents();
	void UnbindSkillEvents();
	void HandleSkillStateChanged();
	void RefreshCooldowns();
	void RefreshSlot(
		const FPlayerSkillSlot& Slot,
		EPlayerSkillID FallbackSkillID,
		UImage* Icon,
		UTextBlock* Title,
		UTextBlock* Level,
		UProgressBar* CooldownBar,
		UTextBlock* CooldownText,
		const TCHAR* KeyLabel);

	UPROPERTY(Transient)
	TObjectPtr<APlayerCharacter> ObservedPlayer;

	UPROPERTY(Transient)
	TObjectPtr<USkillComponent> ObservedSkillComponent;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> SkillBar;

	/** Explicit card backgrounds keep the HUD readable over the white-box floor. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> QCardBorder;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ECardBorder;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> QIcon;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QTitle;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QLevel;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> QCooldownBar;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QCooldownText;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> EIcon;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ETitle;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ELevel;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ECooldownBar;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ECooldownText;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TripleProjectileIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CircularSlashIcon;

	FTimerHandle CooldownRefreshTimer;
	bool bSkillEventSubscribed = false;
};
