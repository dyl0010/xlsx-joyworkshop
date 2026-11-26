#pragma once

struct FGangstarLocalDebugContext;

class FGangstarLocalDebugMiscPanel
{
public:
	static void DrawGeneral(const FGangstarLocalDebugContext& Context);
	static void DrawOptions();
};
