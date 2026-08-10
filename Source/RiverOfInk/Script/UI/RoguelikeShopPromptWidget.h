// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RoguelikeShopPromptWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UTextBlock;

/**
 * Display-only proximity prompt for the Shop Area.
 *
 * The manager controls visibility from overlap events; this widget never
 * polls player position and never intercepts gameplay input.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API URoguelikeShopPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Shop|HUD")
	void SetPromptText(const FText& InPromptText);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void BuildDefaultWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PromptBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PromptText;
};
