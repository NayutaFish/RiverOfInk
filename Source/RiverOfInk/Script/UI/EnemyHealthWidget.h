// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHealthWidget.generated.h"

class AEnemyBase;
class UCanvasPanel;
class UProgressBar;

/**
 * First-pass world-space enemy health bar.
 *
 * The widget starts collapsed and becomes visible after the observed enemy
 * broadcasts its first effective health change. Damage numbers are intentionally
 * outside this slice and will be added as a separate transient layer later.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API UEnemyHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind the widget to an enemy and immediately synchronize its initial state. */
	UFUNCTION(BlueprintCallable, Category = "HUD|EnemyHealth")
	void InitializeForEnemy(AEnemyBase* InEnemy);

	/** Refresh the bar from an enemy health event. */
	UFUNCTION(BlueprintCallable, Category = "HUD|EnemyHealth")
	void RefreshHealth(float InCurrentHealth, float InMaxHealth);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildDefaultWidgetTree();
	void BindToEnemy();
	void UnbindFromEnemy();

	UFUNCTION()
	void HandleEnemyHealthChanged(float InCurrentHealth, float InMaxHealth);

	UPROPERTY(Transient)
	TObjectPtr<AEnemyBase> ObservedEnemy;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	bool bEnemyHealthEventBound = false;
};