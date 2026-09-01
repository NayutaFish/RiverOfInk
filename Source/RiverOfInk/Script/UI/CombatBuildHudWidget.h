// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "CombatBuildHudWidget.generated.h"

class APlayerCharacter;
class UBorder;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UOverlay;
class USizeBox;
class USkillComponent;
class UTextBlock;
class UTexture2D;
class UWidget;

/**
 * Combat-only recent-build HUD.
 *
 * The widget reads the chronological build history owned by USkillComponent.
 * It does not infer history from the current modifier totals, so repeated
 * acquisitions remain visible in their real order after a level transition
 * or a UI reconstruction.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API UCombatBuildHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind the display to the locally controlled player's skill component. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Build")
	void InitializeForPlayer(APlayerCharacter* InPlayer);

	/** Refresh the two visible slots from the persistent run snapshot. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Build")
	void RefreshBuildHistory();

	/** Toggle the optional detail view. No-op while the history is empty. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Build")
	void ToggleBuildDetails();

	/** Apply the player-configured key label to the optional prompt. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Build")
	void SetDetailsKeyLabel(const FText& InKeyLabel);

	UFUNCTION(BlueprintPure, Category = "HUD|Build")
	bool IsBuildDetailsOpen() const { return bDetailsOpen; }

	/** Panel and layer assets are optional; native color fallbacks keep the HUD readable before art import. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<UTexture2D> PanelTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<UTexture2D> FlyWhiteTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<UTexture2D> RecentInkTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<UTexture2D> PreviousInkTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = "360.0", ClampMax = "720.0"))
	float PanelWidth = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = "180.0", ClampMax = "420.0"))
	float PanelHeight = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor PanelFallbackColor = FLinearColor(0.045f, 0.042f, 0.038f, 0.88f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor RecentFallbackColor = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor PreviousFallbackColor = FLinearColor::Transparent;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildDefaultWidgetTree();
	void BindSkillEvents();
	void UnbindSkillEvents();
	void HandleSkillStateChanged();
	void HandleBuildHistoryChanged();
	void SetBuildEntry(const FBuildHistoryEntry* Entry, bool bRecent);
	void UpdateDetailsVisibility();
	void StartLatestBuildFeedback();
	void UpdateLatestBuildFeedback();
	void StopLatestBuildFeedback(bool bRestoreFinalState);
	void SetWidgetScale(UWidget* Widget, float Scale) const;
	void SetTextStyle(UTextBlock* TextBlock, int32 FontSize, const FLinearColor& Color) const;
	UTexture2D* LoadBuildIcon(const FBuildHistoryEntry& Entry);
	UTexture2D* LoadOptionalTexture(FName CacheKey, const TCHAR* AssetPath);
	FText GetBuildTitle(const FBuildHistoryEntry& Entry) const;
	FText GetBuildMeta(const FBuildHistoryEntry& Entry) const;
	FText GetSkillLabel(EPlayerSkillID SkillID) const;
	FText GetBuildDetailLine(const FBuildHistoryEntry& Entry) const;
	bool IsSameEntry(const FBuildHistoryEntry& A, const FBuildHistoryEntry& B) const;

	UPROPERTY(Transient)
	TObjectPtr<APlayerCharacter> ObservedPlayer;

	UPROPERTY(Transient)
	TObjectPtr<USkillComponent> ObservedSkillComponent;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> PanelOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PanelImage;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ContentRow;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RecentSlotBox;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> RecentSlotRoot;

	UPROPERTY(Transient)
	TObjectPtr<UImage> RecentFlyWhiteImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> RecentInkImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> RecentIconImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RecentCaptionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RecentTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RecentMetaText;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> PreviousSlotBox;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> PreviousSlotRoot;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PreviousFlyWhiteImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PreviousInkImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PreviousIconImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PreviousCaptionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PreviousTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PreviousMetaText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailsPromptText;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> DetailsPromptRow;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> DetailsKeyCapBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DetailsKeyCapBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailsKeyText;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> DetailsOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailsTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailsLatestText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailsPreviousText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailsHintText;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> BuildIconCache;

	FText DetailsKeyLabel = FText::FromString(TEXT("B"));
	FBuildHistoryEntry LastDisplayedLatest;
	int32 LastDisplayedHistoryCount = 0;
	bool bHasDisplayedLatest = false;
	bool bSkillEventsSubscribed = false;
	bool bDetailsOpen = false;
	float LatestFeedbackStartTime = 0.0f;
	FTimerHandle LatestFeedbackTimer;
};
