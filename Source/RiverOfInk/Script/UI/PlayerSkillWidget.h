// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "PlayerSkillWidget.generated.h"

class APlayerCharacter;
class UCanvasPanel;
class UHorizontalBox;
class UPlayerSkillSlotWidget;
class UTexture2D;
class USkillComponent;

/**
 * Compact bottom-left Q/E skill HUD.
 *
 * This widget is a presentation layer over USkillComponent. It does not
 * create cooldowns or alter skill input/cast behavior.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API UPlayerSkillWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind the HUD to the current locally controlled player. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill")
	void InitializeForPlayer(APlayerCharacter* InPlayer);

	/** Refresh icons and slot state from the current skill build. */
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
		UPlayerSkillSlotWidget* SkillSlot,
		const TCHAR* KeyLabel);
	UTexture2D* LoadSkillIcon(EPlayerSkillID SkillID);

	UPROPERTY(Transient)
	TObjectPtr<APlayerCharacter> ObservedPlayer;

	UPROPERTY(Transient)
	TObjectPtr<USkillComponent> ObservedSkillComponent;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> SkillBar;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UPlayerSkillSlotWidget> QSkillSlot;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UPlayerSkillSlotWidget> ESkillSlot;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TripleProjectileIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CircularSlashIcon;

	FTimerHandle CooldownRefreshTimer;
	bool bSkillEventSubscribed = false;
};
