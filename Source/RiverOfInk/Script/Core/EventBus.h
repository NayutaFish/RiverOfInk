// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

/**
 * 全局事件总线
 * 用于解耦模块间的直接依赖。参考 Unity 项目 EventBus.cs 适配。
 *
 * 使用模式：
 *   订阅方：FEventBus::Subscribe<FPlayerDiedEvent>(Handler) → 保存返回的 FDelegateHandle
 *   发布方：FEventBus::Publish<FPlayerDiedEvent>(Event)
 *   取消订阅：FEventBus::Unsubscribe<FPlayerDiedEvent>(Handle)
 *
 * 说明：
 *   - 每个事件类型拥有独立的委托链（模板静态变量，零运行时类型查询）
 *   - 发布时若无订阅者则安全无操作，不会报错
 *   - 场景切换时可调用 Clear() 强制清空某类事件的所有订阅
 */
class FEventBus
{
public:
	/** 订阅事件，返回句柄（用于取消订阅） */
	template <typename TEvent>
	static FDelegateHandle Subscribe(const TFunction<void(const TEvent&)>& InHandler)
	{
		return GetEventDelegate<TEvent>().AddLambda(InHandler);
	}

	/** 取消订阅 */
	template <typename TEvent>
	static void Unsubscribe(const FDelegateHandle& InHandle)
	{
		GetEventDelegate<TEvent>().Remove(InHandle);
	}

	/** 发布事件（无订阅者时安全无操作） */
	template <typename TEvent>
	static void Publish(const TEvent& InEvent)
	{
		GetEventDelegate<TEvent>().Broadcast(InEvent);
	}

	/** 清空某类事件的所有订阅（用于场景切换时强制重置） */
	template <typename TEvent>
	static void Clear()
	{
		GetEventDelegate<TEvent>().Clear();
	}

private:
	/** 按事件类型实例化独立的委托链（每类事件一份静态实例） */
	template <typename TEvent>
	static TMulticastDelegate<void(const TEvent&)>& GetEventDelegate()
	{
		static TMulticastDelegate<void(const TEvent&)> EventDelegate;
		return EventDelegate;
	}
};
