#pragma once

struct FGangstarLocalDebugContext;

class FGangstarLocalDebugGoalPanel
{
public:
	static void Capture(const FGangstarLocalDebugContext& Context);
	static void Draw(const FGangstarLocalDebugContext& Context);
};
