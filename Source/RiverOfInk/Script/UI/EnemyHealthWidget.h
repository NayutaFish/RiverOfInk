// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enemy/EnemyBase/EnemyHealthTypes.h"
#include "EnemyHealthWidget.generated.h"

class AEnemyBase;
class UCanvasPanel;
class UImage;
class UOverlay;
class UProgressBar;
class USizeBox;
class UTexture2D;

enum class EEnemyHealthTrailState : uint8
{
	Idle,
	Hold,
	Collapse
};

/**
 * Shared screen-space enemy health widget.
 *
 * The gameplay actor owns the health value. This widget owns only presentation
 * state: current health, an independent recent-damage ghost, and the small
 * normal/elite layout choice. Damage numbers are intentionally outside Phase 1.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API UEnemyHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UEnemyHealthWidget(const FObjectInitializer& ObjectInitializer);

	/** Bind the widget to an enemy and immediately synchronize its initial state. */
	UFUNCTION(BlueprintCallable, Category = "HUD|EnemyHealth")
	void InitializeForEnemy(AEnemyBase* InEnemy);

	/** Force a non-animated health snapshot. Gameplay changes use the event path. */
	UFUNCTION(BlueprintCallable, Category = "HUD|EnemyHealth")
	void RefreshHealth(float InCurrentHealth, float InMaxHealth);

	UFUNCTION(BlueprintPure, Category = "HUD|EnemyHealth")
	float GetCurrentHealthRatio() const { return CurrentHealthRatio; }

	UFUNCTION(BlueprintPure, Category = "HUD|EnemyHealth")
	float GetDamageGhostRatio() const { return DamageGhostRatio; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildDefaultWidgetTree();
	void ConfigureWidgetTree();
	void ConfigureProgressBar(
		UProgressBar* InProgressBar,
		const FLinearColor& InFillColor,
		float InPercent,
		UTexture2D* InFillTexture = nullptr);
	void BindToEnemy();
	void UnbindFromEnemy();

	UFUNCTION()
	void HandleEnemyHealthChanged(
		float InPreviousHealth,
		float InCurrentHealth,
		float InMaxHealth,
		EEnemyHealthChangeReason InChangeReason);

	void ResetHealthState(float InCurrentHealth, float InMaxHealth);
	void ApplyHealthVisuals();
	void StartDamageTrail(float OldCurrentHealthRatio);
	void AdvanceDamageTrail();
	void StopDamageTrail();
	void StartDamageTrailTimer();
	void StopDamageTrailTimer();
	void ApplyRankLayout();
	static float GetSafeHealthRatio(float InCurrentHealth, float InMaxHealth);

	UPROPERTY(Transient)
	TObjectPtr<AEnemyBase> ObservedEnemy;

	/** RectFull canvas root matching the player health widget layout. */
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> HealthSizeBox;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> HealthOverlay;

	/** Full-size rectangle containing only the health and damage bars. */
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> HealthBarLayer;

	/** Independent full-size rectangle containing only the frame image. */
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> HealthFrameLayer;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_EmptyHealth;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_RecentDamage;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_CurrentHealth;

	/** Transparent ink frame rendered above the three health layers. */
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_HealthFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|EnemyHealth|Layout", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	FVector2D NormalWidgetSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|EnemyHealth|Layout", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	FVector2D EliteWidgetSize;

	/** RectFull inset for the Normal bar; the frame keeps the full widget rect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|EnemyHealth|Layout", meta = (AllowPrivateAccess = "true"))
	FMargin NormalBarInset;

	/** RectFull inset for the Elite bar; the frame keeps the full widget rect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|EnemyHealth|Layout", meta = (AllowPrivateAccess = "true"))
	FMargin EliteBarInset;

	/** Positive expansion applied symmetrically to the independent frame rect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|EnemyHealth|Layout", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	FVector2D HealthFrameExpansion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|EnemyHealth|Style", meta = (AllowPrivateAccess = "true"))
	FLinearColor CurrentHealthColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|EnemyHealth|Style", meta = (AllowPrivateAccess = "true"))
	FLinearColor RecentDamageColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|EnemyHealth|Style", meta = (AllowPrivateAccess = "true"))
	FLinearColor EmptyHealthColor;

	/** Minimal transparent frame used by Normal enemies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|EnemyHealth|Skin", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> NormalFrameTexture;

	/** Strengthened transparent frame used by Elite enemies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|EnemyHealth|Skin", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> EliteFrameTexture;

	/** Low-contrast xuan paper used only by the current-health fill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|EnemyHealth|Skin", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> HealthPaperTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|EnemyHealth|DamageTrail", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float RecentDamageHoldTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|EnemyHealth|DamageTrail", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float RecentDamageCollapseDuration;

	float LastKnownCurrentHealth = 0.0f;
	float LastKnownMaxHealth = 1.0f;
	float DamageGhostRatio = 1.0f;
	float DamageCollapseStartRatio = 1.0f;
	float DamageTrailElapsed = 0.0f;
	float CurrentHealthRatio = 1.0f;
	bool bHasHealthBaseline = false;

	EEnemyRank EnemyRank = EEnemyRank::Normal;
	EEnemyHealthTrailState DamageTrailState = EEnemyHealthTrailState::Idle;
	FTimerHandle DamageTrailTimerHandle;
	bool bEnemyHealthEventBound = false;

	static constexpr float DamageTrailUpdateInterval = 1.0f / 60.0f;
};
