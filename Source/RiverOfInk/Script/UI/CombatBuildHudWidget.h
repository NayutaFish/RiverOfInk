// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "UI/CombatBuildIconPlaceholderWidget.h"
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
 * Display-only combat build HUD.
 *
 * The widget owns only the two-entry visible window of the chronological
 * build history: the newest entry is the primary slot and the previous entry
 * is the secondary slot. Detailed build information is intentionally not part
 * of this widget.
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

	/** Compatibility forwarder to the player-owned detail modal. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Build")
	void ToggleBuildDetails();

	/** Apply the player-configured key label to the compact prompt. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Build")
	void SetDetailsKeyLabel(const FText& InKeyLabel);

	/** Whether the player-owned detail modal is currently open. */
	UFUNCTION(BlueprintPure, Category = "HUD|Build")
	bool IsBuildDetailsOpen() const;

	/** Optional panel texture. A native border fallback is used when absent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<UTexture2D> PanelTexture;

	/** Optional recent-build background layers; path fallbacks load the final art assets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Recent Build")
	TObjectPtr<UTexture2D> RecentFeibaiTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Recent Build")
	TObjectPtr<UTexture2D> RecentWashTexture;

	/** Optional B keycap art; a native border fallback is used when absent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<UTexture2D> KeyCapTexture;

	/**
	 * Optional imported/configured icon overrides keyed by the stable build key
	 * (for example TwoStageArc, TwinSlash, or Cooldown). This is the first
	 * resolution tier; path-based redrawn and legacy fallbacks follow it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Build Icons")
	TMap<FName, TObjectPtr<UTexture2D>> ConfiguredBuildIcons;

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

	/** Positive right/bottom safe margins for the compact HUD viewport slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Layout")
	FMargin ViewportMargin = FMargin(0.0f, 0.0f, 150.0f, 300.0f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildDefaultWidgetTree();
	void RefreshRecentBackgroundLayers();
	void ApplyViewportLayout();
	void BindSkillEvents();
	void UnbindSkillEvents();
	void HandleBuildHistoryChanged();
	void SetBuildEntry(const FBuildHistoryEntry* Entry, bool bRecent);
	void StartLatestBuildFeedback();
	void UpdateLatestBuildFeedback();
	void StopLatestBuildFeedback(bool bRestoreFinalState);
	void SetWidgetScale(UWidget* Widget, float Scale) const;
	void SetTextStyle(UTextBlock* TextBlock, int32 FontSize, const FLinearColor& Color) const;
	FName ResolveBuildIconKey(const FBuildHistoryEntry& Entry) const;
	ECombatBuildIconPlaceholderKind ResolvePlaceholderKind(FName BuildIconKey) const;
	UTexture2D* LoadBuildIcon(const FBuildHistoryEntry& Entry);
	UTexture2D* LoadOptionalTexture(FName CacheKey, const TCHAR* AssetPath);
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
	TObjectPtr<UImage> RecentFeibaiImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> RecentWashImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> RecentIconImage;

	UPROPERTY(Transient)
	TObjectPtr<UCombatBuildIconPlaceholderWidget> RecentIconPlaceholder;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> PreviousSlotBox;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> PreviousSlotRoot;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PreviousIconImage;

	UPROPERTY(Transient)
	TObjectPtr<UCombatBuildIconPlaceholderWidget> PreviousIconPlaceholder;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> DetailsPromptRow;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> DetailsKeyCapBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DetailsKeyCapBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailsKeyText;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> BuildIconCache;

	FText DetailsKeyLabel = FText::FromString(TEXT("B"));
	FBuildHistoryEntry LastDisplayedLatest;
	int32 LastDisplayedHistoryCount = 0;
	bool bHasDisplayedLatest = false;
	bool bSkillEventsSubscribed = false;
	float LatestFeedbackStartTime = 0.0f;
	FTimerHandle LatestFeedbackTimer;
};
