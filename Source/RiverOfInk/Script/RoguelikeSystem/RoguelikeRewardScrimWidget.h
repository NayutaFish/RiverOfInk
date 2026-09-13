// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RoguelikeRewardScrimWidget.generated.h"

class UBackgroundBlur;
class UCanvasPanel;
class UImage;

/**
 * Viewport layer below the reward cards. Keeping the BackgroundBlur in its own
 * UUserWidget guarantees that it can only sample the game scene, never cards
 * from the higher-priority reward widget.
 */
UCLASS()
class RIVEROFINK_API URoguelikeRewardScrimWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ConfigureScrim(float InBlurStrength);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void BuildWidgetTree();
	void ApplyStyle();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBackgroundBlur> BackgroundBlur;

	UPROPERTY(Transient)
	TObjectPtr<UImage> BackgroundOverlay;

	float BlurStrength = 14.0f;
	bool bNativeTreeBuilt = false;
};
