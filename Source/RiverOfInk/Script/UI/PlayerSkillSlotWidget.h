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

	/** Set the Alpha Reveal start angle and clockwise sweep range in degrees. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Slot|Cooldown")
	void SetCooldownRevealAngleRange(float InStartAngle, float InSweepAngle);

	/** Completed cooldown fraction: 0.0 immediately after cast, 1.0 at cooldown completion. */
	UFUNCTION(BlueprintPure, Category = "HUD|Skill Slot")
	float GetCooldownProgress() const { return CooldownProgress; }

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Slot")
	EPlayerSkillHudSlotState GetSlotState() const { return SlotState; }

	/** Appearance is intentionally exposed so a Blueprint skin can tune colors and scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	/** Preserve the original project icon artwork; the cooldown ink is a separate layer. */
	FLinearColor SkillIconColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor CooldownInkColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor KeyTextColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = "64.0", ClampMax = "160.0"))
	/** The slot is deliberately larger than the icon so the ink reads as an outer ring. */
	float SlotSize = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = "32.0", ClampMax = "120.0"))
	float IconSize = 48.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = "48.0", ClampMax = "140.0"))
	/** Keep the ink layer larger than the icon; its transparent center frames the icon. */
	float CooldownInkSize = 132.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = "8", ClampMax = "48"))
	int32 KeyFontSize = 20;

	/** Optional Blueprint overrides; null falls back to the stable project asset paths. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooldown")
	TObjectPtr<UMaterialInterface> CooldownMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooldown")
	TObjectPtr<UTexture2D> CooldownTexture;

	/** Alpha Reveal start angle in degrees. The current default begins around 8 o'clock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooldown|Reveal", meta = (ClampMin = "-360.0", ClampMax = "360.0", UIMin = "-360.0", UIMax = "360.0"))
	float CooldownRevealStartAngle = 150.0f;

	/** Total clockwise Alpha Reveal range in degrees. 360 degrees reveals a complete ring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooldown|Reveal", meta = (ClampMin = "0.0", ClampMax = "360.0", UIMin = "0.0", UIMax = "360.0"))
	float CooldownRevealSweepAngle = 360.0f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildDefaultWidgetTree();
	void EnsureCooldownMaterial();
	void ApplyCooldownRevealParameters();
	void SetVisualScale(float Scale);
	void UpdateReadyFeedback();
	void ClearFeedbackTimer();

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> SlotSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> CooldownRingBox;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> SkillIconBox;

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
