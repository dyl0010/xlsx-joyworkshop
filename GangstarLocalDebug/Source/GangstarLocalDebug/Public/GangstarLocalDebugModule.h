#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
class FGangstarLocalDebugInputProcessor;
#endif // WITH_GANGSTAR_LOCAL_IMGUI

class FGangstarLocalDebugModule : public IModuleInterface
{
public:
	FGangstarLocalDebugModule();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
#if WITH_GANGSTAR_LOCAL_IMGUI
	friend class FGangstarLocalDebugInputProcessor;
#endif // WITH_GANGSTAR_LOCAL_IMGUI

	void RegisterImGuiDebugDelegate();
	void UnregisterImGuiDebugDelegate();
	void RegisterInputProcessor();
	void UnregisterInputProcessor();
	void ConfigureDefaultImGuiInput();
	void ToggleImGuiInput();
	void ToggleDebugWindow();
	void DrawLocalDebugWindow();

private:
	FDelegateHandle ImGuiDebugDelegateHandle;
#if WITH_GANGSTAR_LOCAL_IMGUI
	TSharedPtr<FGangstarLocalDebugInputProcessor> InputProcessor;
#endif // WITH_GANGSTAR_LOCAL_IMGUI
	bool bShowDebugWindow;
};
