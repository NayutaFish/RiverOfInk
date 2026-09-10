// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/PlayerState/PlayerState_Dash.h"
#include "RiverOfInk.h"
#include "Common/CombatEffectComponent.h"
#include "Common/CombatEffectTags.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Player/PlayerState/PlayerState_Idle.h"
#include "Player/PlayerState/PlayerState_Move.h"
#include "Input/PlayerInputComponent.h"
#include "Player/PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

void UPlayerState_Dash::OnEnter_Implementation()
{
	StateEnterSoundName = TEXT("PlayerDash");
	Super::OnEnter_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 通报进入冲刺事件（供音效/特效等订阅）
	FEventBus::Publish<FPlayerEnterDashEvent>(FPlayerEnterDashEvent());

	// 标记闪避状态
	Player->bIsDashing = true;

	// 冲刺无敌帧：无敌是普通的 Timed 战斗效果，所有伤害来源共用 IsInvulnerable 这一套判定
	//（与受击后的 0.5s 无敌窗口同源）。ApplyEffect 对“同来源同标签”的效果会复用句柄并重置时长，
	// 所以已经处于无敌时直接跳过，避免把更长的无敌窗口刷新成更短的。
	if (DashInvulnerabilityDuration > KINDA_SMALL_NUMBER)
	{
		if (UCombatEffectComponent* Effects = Player->GetCombatEffectComponent())
		{
			if (!Effects->IsInvulnerable())
			{
				FCombatEffectSpec InvulnerabilitySpec;
				InvulnerabilitySpec.EffectTag = RiverOfInkCombatEffectTags::Effect_Buff_Invulnerable;
				InvulnerabilitySpec.Category = ECombatEffectCategory::Buff;
				InvulnerabilitySpec.DurationPolicy = ECombatEffectDurationPolicy::Timed;
				InvulnerabilitySpec.StackPolicy = ECombatEffectStackPolicy::RefreshDuration;
				InvulnerabilitySpec.Duration = DashInvulnerabilityDuration;
				InvulnerabilitySpec.SourceActor = Player;
				Effects->ApplyEffect(InvulnerabilitySpec);

				UE_LOG(LogRiverOfInk, Log,
					TEXT("Player Dash invulnerability applied: Duration=%.2fs."),
					DashInvulnerabilityDuration);
			}
			else
			{
				UE_LOG(LogRiverOfInk, Verbose,
					TEXT("Player Dash invulnerability skipped: already invulnerable (existing window kept)."));
			}
		}
	}

	// 订阅 WASD 输入，跟踪退出时是否有移动；同时订阅左键做 dash-attack 取消
	UPlayerInputComponent* Input = Player->FindComponentByClass<UPlayerInputComponent>();
	if (Input)
	{
		CachedInput = Input;
		Input->OnMoveXDelegate.AddUObject(this, &UPlayerState_Dash::OnMoveX);
		Input->OnMoveYDelegate.AddUObject(this, &UPlayerState_Dash::OnMoveY);
		Input->OnLmbDelegate.AddUObject(this, &UPlayerState_Dash::OnLmb);
	}

	// 冲刺期间锁定朝向（不让 CMC 按移动输入自行转向）
	Player->GetCharacterMovement()->bOrientRotationToMovement = false;

	// 冲刺方向：优先取“当前移动输入方向”（与 Move 状态同一套等距映射），没有输入时才退回当前朝向。
	// 从普攻里按冲刺取消时角色朝向是攻击方向（鼠标方向），必须用输入方向才不会冲错。
	const FVector InputDirection = Input ? Input->GetMoveWorldDirection() : FVector::ZeroVector;
	const bool bHasInputDirection = !InputDirection.IsNearlyZero();
	if (bHasInputDirection)
	{
		Player->SetActorRotation(FRotator(0.0f, InputDirection.Rotation().Yaw, 0.0f));
	}

	// 速度 = 当前朝向 × 起手速度（朝向已在上面按输入方向校正）
	Player->GetCharacterMovement()->Velocity = Player->GetActorForwardVector() * DashInitialSpeed;

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Player Dash started: Direction=%s Source=%s InitialSpeed=%.0f SustainSpeed=%.0f Duration=%.2f."),
		*Player->GetActorForwardVector().ToCompactString(),
		bHasInputDirection ? TEXT("MoveInput") : TEXT("ActorFacing"),
		DashInitialSpeed,
		DashSpeed,
		DashDuration);

	// ms 后检测退出
	bHadMoveInput = false;
	GetWorld()->GetTimerManager().SetTimer(DashTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
		if (!Player) return;

		if (bHadMoveInput)
		{
			Player->SwitchState(UPlayerState_Move::StaticClass());
		}
		else
		{
			Player->SwitchState(UPlayerState_Idle::StaticClass());
		}
	}), FMath::Max(DashDuration, KINDA_SMALL_NUMBER), false);
}

void UPlayerState_Dash::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 通报退出冲刺事件（供音效/特效等订阅）
	FEventBus::Publish<FPlayerExitDashEvent>(FPlayerExitDashEvent());

	// 取消闪避标记
	Player->bIsDashing = false;

	// 冲刺冷却
	Player->StartDashCooldown();

	// 恢复朝向跟随移动
	Player->GetCharacterMovement()->bOrientRotationToMovement = true;

	// 取消订阅
	UPlayerInputComponent* Input = CachedInput.Get();
	if (Input)
	{
		Input->OnMoveXDelegate.RemoveAll(this);
		Input->OnMoveYDelegate.RemoveAll(this);
		Input->OnLmbDelegate.RemoveAll(this);
	}

	CachedInput = nullptr;

	// 取消计时器
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DashTimerHandle);
	}
}

void UPlayerState_Dash::Update_Implementation(float DeltaTime)
{
	Super::Update_Implementation(DeltaTime);

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 持续保持冲刺速度，抵消摩擦减速
	Player->GetCharacterMovement()->Velocity = Player->GetActorForwardVector() * DashSpeed;
}

void UPlayerState_Dash::OnLmb()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player || !bAllowDashAttackCancel)
	{
		return;
	}

	// 普攻也在冷却：不做取消，把这次左键留给输入缓冲（之后回到 Move/Idle 时可按需消费）
	if (!Player->bCanAttack1)
	{
		return;
	}

	// 哈迪斯式 dash-attack：冲刺途中按左键立刻转入普攻。
	// 冲刺的 OnExit 会负责收尾（bIsDashing、冲刺冷却、朝向恢复、计时器清理），
	// 所以这里直接请求普攻即可，不需要手动停冲刺。
	if (CachedInput)
	{
		CachedInput->ClearBufferedInput(EPlayerBufferedInput::Attack1);
	}

	UE_LOG(LogRiverOfInk, Log, TEXT("Player Dash canceled into normal attack (dash-attack)."));
	Player->RequestNormalAttack();
}

void UPlayerState_Dash::OnMoveX(float Value)
{
	if (!FMath::IsNearlyZero(Value)) bHadMoveInput = true;
}

void UPlayerState_Dash::OnMoveY(float Value)
{
	if (!FMath::IsNearlyZero(Value)) bHadMoveInput = true;
}