// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PlayerCheatCommands.generated.h"

class APlayerCharacter;

/**
 * 玩家调试指令（静态类）。
 *
 * 目前只有一条：回复玩家生命。玩家按下 H 键时由 APlayerCharacter 调用（默认回 500），
 * 也可以在蓝图里直接调 HealPlayer。
 */
UCLASS()
class RIVEROFINK_API UPlayerCheatCommands : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 默认治疗量（按 H 回血用的值）。 */
	static constexpr float DefaultHealAmount = 500.0f;

	/**
	 * 回复玩家生命，上限为最大生命。
	 *
	 * @param WorldContextObject	世界上下文（蓝图里自动填；C++ 传玩家自己即可）
	 * @param Amount				治疗量；<= 0 时使用 DefaultHealAmount（500）
	 * @return 是否成功治疗
	 */
	UFUNCTION(BlueprintCallable, Category = "Cheat|Player", meta = (WorldContext = "WorldContextObject"))
	static bool HealPlayer(const UObject* WorldContextObject, float Amount = 500.0f);
};
