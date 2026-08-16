// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHealthWidget.generated.h"

class APlayerCharacter;
class UCanvasPanel;
class UHealthComponent;
class UImage;
class UOverlay;
class UProgressBar;
class USafeZone;
class USizeBox;
class UTextBlock;
class UTexture2D;

enum class EHealthDamageTrailState : uint8
{
    Idle,
    Hold,
    Collapse
};

/**
 * Player-only health HUD.
 *
 * Gameplay health remains owned by UHealthComponent. This widget owns only
 * display state: the current ratio, a temporary damage ghost, and optional
 * development text. The native tree is also named so a WBP subclass can bind
 * the same layers without changing the data flow.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API UPlayerHealthWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPlayerHealthWidget(const FObjectInitializer& ObjectInitializer);

    /** Bind the widget to the current player and immediately establish a clean baseline. */
    UFUNCTION(BlueprintCallable, Category = "HUD|Health")
    void InitializeForPlayer(APlayerCharacter* InPlayer);

    /** Force a non-animated health snapshot. Parameter order is kept for existing callers. */
    UFUNCTION(BlueprintCallable, Category = "HUD|Health")
    void RefreshHealth(float InMaxHealth, float InCurrentHealth);

    /** Toggle development-only Current / Max text without changing bar layout. */
    UFUNCTION(BlueprintCallable, Category = "HUD|Health|Debug")
    void SetShowHealthDebugText(bool bInShow);

    UFUNCTION(BlueprintPure, Category = "HUD|Health")
    float GetCurrentHealthRatio() const { return CurrentHealthRatio; }

    UFUNCTION(BlueprintPure, Category = "HUD|Health")
    float GetDamageGhostRatio() const { return DamageGhostRatio; }

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    void BuildDefaultWidgetTree();
    void ConfigureWidgetTree();
    void ConfigureProgressBar(UProgressBar* InProgressBar, const FLinearColor& InFillColor, float InPercent, UTexture2D* InFillTexture = nullptr);

    void BindToHealthComponent(UHealthComponent* InHealthComponent);
    void UnbindFromHealthComponent();

    UFUNCTION()
    void HandleHealthChanged(float InCurrentHealth, float InMaxHealth);

    void ResetHealthState(float InCurrentHealth, float InMaxHealth);
    void ApplyHealthVisuals();
    void RefreshDebugText();

    void StartDamageTrail(float OldCurrentHealth);
    void AdvanceDamageTrail();
    void StopDamageTrail();
    void StartDamageTrailTimer();
    void StopDamageTrailTimer();

    bool IsHealthDebugTextEnabled() const;
    static float GetSafeHealthRatio(float InCurrentHealth, float InMaxHealth);

    UPROPERTY(Transient)
    TObjectPtr<APlayerCharacter> ObservedPlayer;

    UPROPERTY(Transient)
    TObjectPtr<UHealthComponent> ObservedHealthComponent;

    UPROPERTY(Transient)
    TObjectPtr<USafeZone> RootSafeZone;

    UPROPERTY(Transient)
    TObjectPtr<UCanvasPanel> RootCanvas;

    UPROPERTY(Transient)
    TObjectPtr<USizeBox> HealthSizeBox;

    UPROPERTY(Transient)
    TObjectPtr<UOverlay> HealthOverlay;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> ProgressBar_EmptyHealth;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> ProgressBar_RecentDamage;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> ProgressBar_CurrentHealth;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UImage> Image_HealthFrame;

    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_HealthDebug;

    /** RectFull-compatible design size; X can be freely adjusted because the frame uses a Box/nine-slice brush. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Health|Layout", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
    FVector2D HealthWidgetSize;

    /** Top-left design margin inside the platform safe zone. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Health|Layout", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    FVector2D HealthWidgetMargin;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Health|Style", meta = (AllowPrivateAccess = "true"))
    FLinearColor CurrentHealthColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Health|Style", meta = (AllowPrivateAccess = "true"))
    FLinearColor RecentDamageColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Health|Style", meta = (AllowPrivateAccess = "true"))
    FLinearColor EmptyHealthColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Health|Style", meta = (AllowPrivateAccess = "true"))
    FLinearColor HealthFrameColor;

    /** Transparent ink frame. Horizontal and vertical edges are preserved by the Box brush. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Health|Skin", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UTexture2D> HealthFrameTexture;

    /** Low-contrast xuan paper used only by the current-health fill. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Health|Skin", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UTexture2D> HealthPaperTexture;

    /** Normalized Box-brush margins: X preserves left/right ink caps, Y preserves top/bottom rails. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Health|Skin", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "0.5"))
    FVector2D HealthFrameSliceFraction;

    /** Overall color for the runtime health text, including numbers and separators. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Health|Style", meta = (AllowPrivateAccess = "true"))
    FLinearColor HealthTextColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Health|DamageTrail", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
    float RecentDamageHoldTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Health|DamageTrail", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
    float RecentDamageCollapseDuration;

    /** Development default is true; shipping is always forced off at runtime. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Health|Debug", meta = (AllowPrivateAccess = "true"))
    bool bShowHealthDebugText;

    float LastKnownCurrentHealth = 0.0f;
    float LastKnownMaxHealth = 1.0f;
    float DamageGhostHealth = 0.0f;
    float DamageCollapseStartHealth = 0.0f;
    float DamageTrailElapsed = 0.0f;
    float CurrentHealthRatio = 1.0f;
    float DamageGhostRatio = 1.0f;
    bool bHasHealthBaseline = false;

    EHealthDamageTrailState DamageTrailState = EHealthDamageTrailState::Idle;
    FTimerHandle DamageTrailTimerHandle;
    static constexpr float DamageTrailUpdateInterval = 1.0f / 60.0f;
};
