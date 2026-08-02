// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RiverOfInkGameMode.generated.h"

UCLASS()
class RIVEROFINK_API ARiverOfInkGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARiverOfInkGameMode();

protected:
	virtual void BeginPlay() override;
};
