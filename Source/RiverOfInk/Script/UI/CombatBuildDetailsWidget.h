// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "UI/BuildPresentationResolver.h"
#include "CombatBuildDetailsWidget.generated.h"

class APlayerCharacter;
class UBorder;
class UCanvasPanel;
class UCombatBuildIconPlaceholderWidget;
class UCombatBuildDetailsArrowButton;
class UCombatBuildDetailsSlotButton;
class UFont;
class UHorizontalBox;
class UImage;
class UOverlay;
class USafeZone;
class UScaleBox;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class USkillComponent;

/**
 * Whitebox/details presentation for the build-book HUD.
 *
 * Slice 1-6 owns the read-only view model, responsive geometry, per-category
 * five-slot window, selection/focus navigation, modal close input and the
 * low-frequency build-state refresh subscription.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API UCombatBuildDetailsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind the whitebox to a locally controlled player's skill component. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Build Details")
	void InitializeForPlayer(APlayerCharacter* InPlayer);

	/** Rebuild the display snapshot and repaint the current whitebox tree. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Build Details")
	void RefreshDetails();

	/** Return a copy so Blueprint callers cannot mutate the runtime source state. */
	UFUNCTION(BlueprintPure, Category = "HUD|Build Details")
	FCombatBuildDetailsViewModel GetViewModel() const { return ViewModel; }

	/** Optional imported paper/fly-white layer; the pale border remains the whitebox fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<UTexture2D> PanelTexture;

	/** Optional selected-build blue wash; it is kept separate from the Icon and paper layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<UTexture2D> SelectedWashTexture;

	/** Optional imported Chinese display font; falls back to the project UI font when unavailable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Typography")
	TObjectPtr<UFont> DetailsFont;

	/** Optional imported/configured build Icon overrides keyed by stable IconKey. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Build Icons")
	TMap<FName, TObjectPtr<UTexture2D>> ConfiguredBuildIcons;

	/** Reference design size. The outer SafeZone/ScaleBox handles screen scaling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Layout", meta = (ClampMin = "720.0", ClampMax = "1800.0"))
	float PanelWidth = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Layout", meta = (ClampMin = "520.0", ClampMax = "1200.0"))
	float PanelHeight = 860.0f;

	/** Non-black paper tone used until the final panel texture is imported. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor PanelFallbackColor = FLinearColor(0.91f, 0.89f, 0.83f, 0.97f);

	/** Whether empty whitebox slots remain visible while the layout is being validated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Layout")
	bool bShowWhiteboxEmptySlots = false;

	/** Translucent veil behind the modal build book; keeps the combat scene visually subordinate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Modal")
	FLinearColor ScrimColor = FLinearColor(0.02f, 0.025f, 0.03f, 0.42f);

	/** Move one category window one slot toward older builds. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Build Details|Navigation")
	bool ShowPrevious(int32 CategoryIndex);

	/** Move one category window one slot toward newer builds. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Build Details|Navigation")
	bool ShowNext(int32 CategoryIndex);

	/** Return the first acquired-build slot currently shown for a category. */
	UFUNCTION(BlueprintPure, Category = "HUD|Build Details|Navigation")
	int32 GetCategoryStartIndex(int32 CategoryIndex) const;

	/** Return the number of acquired-build entries in a category. */
	UFUNCTION(BlueprintPure, Category = "HUD|Build Details|Navigation")
	int32 GetCategoryItemCount(int32 CategoryIndex) const;

	/** Pass the player-configured toggle key to the UI-only modal. */
	void SetDetailsKey(const FKey& InKey);

	/** Give keyboard/gamepad focus to the first visible build slot. */
	void FocusFirstAvailableSlot();

	/** Clear the player reference and transient navigation state before removal. */
	void ClearForClose();

	UFUNCTION(BlueprintPure, Category = "HUD|Build Details|Navigation")
	FName GetSelectedBuildId() const { return SelectedBuildId; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	friend class UCombatBuildDetailsSlotButton;

	void BuildDefaultWidgetTree();
	void BuildCategoryRow(ECombatBuildCategory Category, UVerticalBox* InCategoryList, int32 CategoryIndex);
	void RefreshCategorySlots();
	void RefreshSelectedPreview();
	void RefreshSelectionVisuals();
	void BuildSortedCategoryItems(TArray<TArray<const FCombatBuildDetailsItem*>>& OutItems) const;
	void SelectItem(int32 CategoryIndex, int32 ItemIndex, bool bMoveKeyboardFocus);
	void HandleSlotClicked(int32 CategoryIndex, int32 SlotIndex);
	void HandleSlotHovered(int32 CategoryIndex, int32 SlotIndex);
	void BindSkillEvents();
	void UnbindSkillEvents();
	void HandleBuildHistoryChanged();
	bool MoveSelectionHorizontal(int32 Direction);
	bool MoveSelectionVertical(int32 Direction);
	bool HandleNavigationKey(const FKey& Key);
	void FocusSelectedSlot();
	void SetTextStyle(UTextBlock* TextBlock, int32 FontSize, const FLinearColor& Color) const;
	UTexture2D* LoadBuildIcon(FName IconKey);
	void BindArrowCallbacks(UButton* PreviousButton, UButton* NextButton, int32 CategoryIndex);
	int32 GetMaxCategoryStartIndex(int32 CategoryIndex) const;
	static int32 GetCategoryIndex(ECombatBuildCategory Category);

	UFUNCTION()
	void HandlePreviousArrow0();
	UFUNCTION()
	void HandlePreviousArrow1();
	UFUNCTION()
	void HandlePreviousArrow2();
	UFUNCTION()
	void HandlePreviousArrow3();
	UFUNCTION()
	void HandlePreviousArrow4();
	UFUNCTION()
	void HandleNextArrow0();
	UFUNCTION()
	void HandleNextArrow1();
	UFUNCTION()
	void HandleNextArrow2();
	UFUNCTION()
	void HandleNextArrow3();
	UFUNCTION()
	void HandleNextArrow4();

	UPROPERTY(Transient)
	TObjectPtr<APlayerCharacter> ObservedPlayer;

	UPROPERTY(Transient)
	TObjectPtr<USkillComponent> ObservedSkillComponent;

	/** True while the details widget owns a low-frequency build-state subscription. */
	bool bSkillEventsSubscribed = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Data", meta = (AllowPrivateAccess = "true"))
	FCombatBuildDetailsViewModel ViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ScrimOverlay;

	UPROPERTY(Transient)
	TObjectPtr<USafeZone> DetailsSafeZone;

	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> DetailsScaleBox;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> DetailsDesignBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DetailsPanel;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> DetailsPanelOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PanelImage;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> PagesRow;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHorizontalBox>> CategorySlotRows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> CategoryPreviousArrowTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> CategoryNextArrowTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> CategoryPreviousArrowBoxes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> CategoryNextArrowBoxes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> CategoryPreviousArrowButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> CategoryNextArrowButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCombatBuildDetailsSlotButton>> CategorySlotButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CategorySlotSelectionWashes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CategorySlotImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCombatBuildIconPlaceholderWidget>> CategorySlotPlaceholders;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> SelectedPreviewRoot;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SelectedWashImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SelectedIconImage;

	UPROPERTY(Transient)
	TObjectPtr<UCombatBuildIconPlaceholderWidget> SelectedIconPlaceholder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SelectedBuildTitle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SelectedBuildDescription;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> BuildIconCache;

	/** Per-category first visible acquired-build index; five slots stay visible. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Navigation", meta = (AllowPrivateAccess = "true"))
	TArray<int32> CategoryStartIndices;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Navigation", meta = (AllowPrivateAccess = "true"))
	TArray<int32> CategoryItemCounts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (AllowPrivateAccess = "true"))
	FKey DetailsKey = EKeys::B;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Navigation", meta = (AllowPrivateAccess = "true"))
	FName SelectedBuildId;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Navigation", meta = (AllowPrivateAccess = "true"))
	int32 SelectedCategoryIndex = INDEX_NONE;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Navigation", meta = (AllowPrivateAccess = "true"))
	int32 SelectedItemIndex = INDEX_NONE;
};

/** Transparent arrow hit target; the visible glyph remains a separate child. */
UCLASS()
class RIVEROFINK_API UCombatBuildDetailsArrowButton : public UButton
{
	GENERATED_BODY()

public:
	explicit UCombatBuildDetailsArrowButton(const FObjectInitializer& ObjectInitializer);
};

/** Slot hit target that carries its category/slot coordinates to the details widget. */
UCLASS()
class RIVEROFINK_API UCombatBuildDetailsSlotButton : public UButton
{
	GENERATED_BODY()

public:
	explicit UCombatBuildDetailsSlotButton(const FObjectInitializer& ObjectInitializer);
	void InitializeForDetails(UCombatBuildDetailsWidget* InOwner, int32 InCategoryIndex, int32 InSlotIndex);

private:
	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UPROPERTY(Transient)
	TObjectPtr<UCombatBuildDetailsWidget> OwnerWidget;

	UPROPERTY(Transient)
	int32 CategoryIndex = INDEX_NONE;

	UPROPERTY(Transient)
	int32 SlotIndex = INDEX_NONE;
};
