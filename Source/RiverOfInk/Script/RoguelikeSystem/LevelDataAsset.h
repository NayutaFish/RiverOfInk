// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RoguelikeSystem/RoguelikeRunTypes.h"
#include "LevelDataAsset.generated.h"

class UWorld;

/**
 * 顺序关卡列表里的一项。
 *
 * 数组顺序 = 玩家推进顺序；`RoomType` 决定这一关按"战斗房间"还是"商店房间"处理
 * （战斗房间会刷怪 + 清场奖励，商店房间不刷怪、出口进房即开、商店 HUD 可交互）。
 */
USTRUCT(BlueprintType)
struct FRunLevelEntry
{
	GENERATED_BODY()

	/** 关卡标识，只用于日志与结算展示；留空时按序号自动生成（Level_01、Level_02...）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	FName LevelId = NAME_None;

	/** 这一关用的 Level 地图资产（在编辑器里把 .umap 拖进来）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	TSoftObjectPtr<UWorld> LevelMap;

	/** 关卡类型：Combat = 刷怪 + 清场奖励；Shop = 商店房间。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	ERoguelikeRoomType RoomType = ERoguelikeRoomType::Combat;

	/** 战斗房间的难度档位；商店房间保持默认即可。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	ERoguelikeEncounterTier EncounterTier = ERoguelikeEncounterTier::Normal;
};

/**
 * 关卡数据资产：一条**顺序固定**的关卡列表，玩家从第 0 项一路打到终点。
 *
 * 约定：资产放在 `Content/DataAsset/LevelData/DA_LevelData`（RunFlow 按这个固定资产路径加载）。
 * 只要这个资产存在且列表非空，RunFlow 就走"顺序关卡列表"模式；否则退回原来的
 * 大关 / 房间池白盒配置。
 *
 * 通关规则：最后一个关卡（列表终点）**清场瞬间**即通关 —— 广播 FGameClearedEvent 并进入
 * Result 状态，不再弹该关的清场奖励。如果终点是商店房间（没有清场），则走完它的出口时通关。
 */
UCLASS(BlueprintType)
class RIVEROFINK_API ULevelDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 顺序关卡列表：第 0 项是进入游戏后的第一关，最后一项是终点。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level|Flow", meta = (TitleProperty = "LevelId"))
	TArray<FRunLevelEntry> Levels;
};
