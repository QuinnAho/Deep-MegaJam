// Tests only: a console variable set for a scope's lifetime and restored when it ends, even when the test fails.
// Deep.World.Still is latched when the arena is built, so a scope that pins it must open before the test's world.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

namespace DeepTest
{
	struct FCVarScope
	{
		IConsoleVariable* Var = nullptr;
		int32 Before = 0;

		FCVarScope(const TCHAR* Name, int32 Value)
		{
			Var = IConsoleManager::Get().FindConsoleVariable(Name);
			if (Var)
			{
				Before = Var->GetInt();
				Var->Set(Value, ECVF_SetByCode);
			}
		}

		~FCVarScope()
		{
			if (Var)
			{
				Var->Set(Before, ECVF_SetByCode);
			}
		}

		FCVarScope(const FCVarScope&) = delete;
		FCVarScope& operator=(const FCVarScope&) = delete;
	};
}
