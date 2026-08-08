// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalEnums.generated.h"

/**
 * 全局玩法枚举
 * 所有与 GamePlay 相关的跨模块枚举统一在此定义。
 */

// ====================
// 伤害类型
// ====================

UENUM(BlueprintType)
enum class EDamageType : uint8
{
	Physical UMETA(DisplayName = "Physical"),
	Magic UMETA(DisplayName = "Magic"),
	TrueDamage UMETA(DisplayName = "True Damage"),
	Must UMETA(DisplayName = "Must"),
	Unified UMETA(DisplayName = "Unified Damage")
};
