// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHealthWidget.generated.h"

class AHikariPlayerCharacter;
class UCanvasPanel;
class UProgressBar;
class UTextBlock;
struct FPlayerHealthChangedEvent;

/**
 * First-pass player health HUD.
 *
 * The widget builds a small progress bar in C++ so it works without a
 * dedicated Blueprint asset. A Blueprint subclass can still override the
 * class and replace the visual tree later.
 */
UCLASS(Blueprintable)
class TEST_GAMEPLAY_API UPlayerHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind the widget to the current player and immediately refresh its value. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Health")
	void InitializeForPlayer(AHikariPlayerCharacter* InPlayer);

	/** Refresh the visual state from a health event. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Health")
	void RefreshHealth(float InMaxHealth, float InCurrentHealth);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildDefaultWidgetTree();
	void SubscribeToHealthEvents();
	void UnsubscribeFromHealthEvents();
	void HandleHealthChanged(const FPlayerHealthChangedEvent& Event);

	UPROPERTY(Transient)
	TObjectPtr<AHikariPlayerCharacter> ObservedPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	FDelegateHandle HealthChangedHandle;
	bool bHealthEventSubscribed = false;
};
