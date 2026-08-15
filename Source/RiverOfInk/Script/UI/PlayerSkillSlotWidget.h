// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "PlayerSkillSlotWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UOverlay;
class USizeBox;
class UTextBlock;
class UTexture2D;

UENUM(BlueprintType)
enum class EPlayerSkillHudSlotState : uint8
{
	Hidden,
	Cooldown,
	ReadyFeedback,
	FadeOut
};

/**
 * Small reusable Q/E presentation slot.
 *
 * The slot owns one cached cooldown MID. It only consumes cooldown values
 * supplied by the parent HUD; it never starts or changes gameplay timers.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API UPlayerSkillSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Initialize the slot label and skill identity without changing gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Slot")
	void InitializeSkillSlot(EPlayerSkillID InSkillID, const FText& InKeyText);

	/** Set the standalone project icon used by this slot. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Slot")
	void SetSkillIcon(UTexture2D* InSkillIcon);

	/** Set the key hint shown in the lower-right corner of the slot. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Slot")
	void SetKeyText(const FText& InKeyText);

	/** Show the slot and reset its brush reveal to the start of a real cooldown. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Slot")
	void StartCooldown(float InCooldownDuration);

	/** Feed the current remaining/duration values from the existing skill component. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Slot")
	void UpdateCooldown(float InCooldownRemaining, float InCooldownDuration);

	/** Finish the angular reveal and play the short ready/fade feedback. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Slot")
	void FinishCooldown();

	/** Set the material progress directly, clamped to the 0..1 reveal range. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Slot")
	void SetCooldownProgress(float InProgress);

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Slot")
	float GetCooldownProgress() const { return CooldownProgress; }

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Slot")
	EPlayerSkillHudSlotState GetSlotState() const { return SlotState; }

	/** Appearance is intentionally exposed so a Blueprint skin can tune colors and scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor SkillIconColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor CooldownInkColor = FLinearColor(0.015f, 0.012f, 0.01f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor KeyTextColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = "64.0", ClampMax = "160.0"))
	float SlotSize = 94.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = "32.0", ClampMax = "120.0"))
	float IconSize = 56.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = "48.0", ClampMax = "140.0"))
	float CooldownInkSize = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = "8", ClampMax = "48"))
	int32 KeyFontSize = 20;

	/** Optional Blueprint overrides; null falls back to the stable project asset paths. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooldown")
	TObjectPtr<UMaterialInterface> CooldownMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooldown")
	TObjectPtr<UTexture2D> CooldownTexture;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildDefaultWidgetTree();
	void EnsureCooldownMaterial();
	void SetVisualScale(float Scale);
	void UpdateReadyFeedback();
	void ClearFeedbackTimer();

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> SlotSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> OverlayRoot;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ImageSkillIcon;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ImageCooldownInk;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TextKey;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CooldownMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedCooldownTexture;

	EPlayerSkillID SkillID = EPlayerSkillID::None;
	EPlayerSkillHudSlotState SlotState = EPlayerSkillHudSlotState::Hidden;
	float CooldownProgress = 0.0f;
	float CooldownDuration = 0.0f;
	bool bCooldownActive = false;
	float FeedbackStartTime = 0.0f;
	float FadeStartTime = 0.0f;
	FTimerHandle FeedbackTimer;
};
