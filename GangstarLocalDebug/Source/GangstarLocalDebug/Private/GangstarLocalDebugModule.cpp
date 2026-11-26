#include "GangstarLocalDebugModule.h"

#include "Modules/ModuleManager.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GangstarLocalDebugContext.h"
#include "ImGuiDelegates.h"
#include "ImGuiModule.h"
#include "ImGuiModuleProperties.h"
#include "InputCoreTypes.h"
#include "Panels/GangstarLocalDebugActivityPanel.h"
#include "Panels/GangstarLocalDebugGasPanel.h"
#include "Panels/GangstarLocalDebugGoalPanel.h"
#include "Panels/GangstarLocalDebugGoalScoreContainerPanel.h"
#include "Panels/GangstarLocalDebugGridBagPanel.h"
#include "Panels/GangstarLocalDebugItemContainerPanel.h"
#include "Panels/GangstarLocalDebugLightQuestPanel.h"
#include "Panels/GangstarLocalDebugMiscPanel.h"
#include "Panels/GangstarLocalDebugProcurementPanel.h"
#include "Panels/GangstarLocalDebugProjectMapPanel.h"
#include "Panels/GangstarLocalDebugTearDownDynamicsPanel.h"
#include "Panels/GangstarLocalDebugTradeCalcPanel.h"
#include <imgui.h>
#endif // WITH_GANGSTAR_LOCAL_IMGUI

DEFINE_LOG_CATEGORY_STATIC(LogGangstarLocalDebugModule, Log, All);

IMPLEMENT_MODULE(FGangstarLocalDebugModule, GangstarLocalDebug);

namespace GangstarLocalDebugModule
{
	static constexpr const TCHAR* ModuleName = TEXT("GangstarLocalDebug");
	static constexpr const TCHAR* ToggleInputKeyName = TEXT("F8");
	static constexpr const TCHAR* ToggleWindowKeyName = TEXT("F9");
}

#if WITH_GANGSTAR_LOCAL_IMGUI
namespace GangstarLocalDebugNavigation
{
    enum class EPanel : uint8
    {
        ProjectMap,
        General,
        GAS,
        Options,
        Activity,
        LightQuest,
        Goals,
        GoalScoreContainer,
        GridBag,
        ItemContainer,
        Procurement,
        TearDownDynamics,
        TradeCalc,
    };

    struct FPanelEntry
    {
        const char* GroupName;
        const char* PanelName;
        EPanel Panel;
    };

    static constexpr FPanelEntry PanelEntries[] =
    {
        { "Overview", "ProjectMap", EPanel::ProjectMap },
        { "Overview", "General", EPanel::General },
        { "Player", "GAS", EPanel::GAS },
        { "Player", "Activity", EPanel::Activity },
        { "Player", "LightQuest", EPanel::LightQuest },
        { "Player", "Goals", EPanel::Goals },
        { "Player", "Goal - ScoreContainer", EPanel::GoalScoreContainer },
        { "Inventory / Economy", "GridBag", EPanel::GridBag },
        { "Inventory / Economy", "ItemContainer", EPanel::ItemContainer },
        { "Inventory / Economy", "Procurement", EPanel::Procurement },
        { "Inventory / Economy", "TearDownDynamics", EPanel::TearDownDynamics },
        { "Inventory / Economy", "TradeCalc", EPanel::TradeCalc },
        { "Tools", "Options", EPanel::Options },
    };

    static bool PassesFilter(const char* Text, const char* Filter)
    {
        if (!Filter || Filter[0] == '\0')
        {
                return true;
        }

        return FCStringAnsi::Stristr(Text, Filter) != nullptr;
    }

    static const char* GetPanelName(EPanel Panel)
    {
        for (const FPanelEntry& Entry : PanelEntries)
        {
            if (Entry.Panel == Panel)
            {
                return Entry.PanelName;
            }
        }

        return "Unknown";
    }

    static void DrawNavigation(EPanel& SelectedPanel)
    {
        static char Filter[64] = {};

        ImGui::TextUnformatted("Panels");
        ImGui::PushItemWidth(-1.0f);
        ImGui::InputText("##GangstarLocalDebugPanelFilter", Filter, UE_ARRAY_COUNT(Filter));
        ImGui::PopItemWidth();
        ImGui::Separator();

        const char* CurrentGroupName = nullptr;
        for (const FPanelEntry& Entry : PanelEntries)
        {
            if (!PassesFilter(Entry.GroupName, Filter) && !PassesFilter(Entry.PanelName, Filter))
            {
                continue;
            }

            if (CurrentGroupName != Entry.GroupName)
            {
                if (CurrentGroupName)
                {
                    ImGui::Spacing();
                }

                CurrentGroupName = Entry.GroupName;
                ImGui::TextColored(ImVec4(0.70f, 0.85f, 1.0f, 1.0f), "%s", CurrentGroupName);
            }

            if (ImGui::Selectable(Entry.PanelName, SelectedPanel == Entry.Panel))
            {
                SelectedPanel = Entry.Panel;
            }
        }
    }

    static void DrawSelectedPanel(EPanel SelectedPanel, const FGangstarLocalDebugContext& Context)
    {
        ImGui::TextUnformatted(GetPanelName(SelectedPanel));
        ImGui::Separator();

        switch (SelectedPanel)
        {
        case EPanel::ProjectMap:
            FGangstarLocalDebugProjectMapPanel::Draw(Context);
            break;
        case EPanel::General:
            FGangstarLocalDebugMiscPanel::DrawGeneral(Context);
            break;
        case EPanel::GAS:
            FGangstarLocalDebugGasPanel::Draw(Context);
            break;
        case EPanel::Options:
            FGangstarLocalDebugMiscPanel::DrawOptions();
            break;
        case EPanel::Activity:
            FGangstarLocalDebugActivityPanel::Draw(Context);
            break;
        case EPanel::LightQuest:
            FGangstarLocalDebugLightQuestPanel::Draw(Context);
            break;
        case EPanel::Goals:
            FGangstarLocalDebugGoalPanel::Draw(Context);
            break;
        case EPanel::GoalScoreContainer:
            FGangstarLocalDebugGoalScoreContainerPanel::Draw(Context);
            break;
        case EPanel::GridBag:
            FGangstarLocalDebugGridBagPanel::Draw(Context);
            break;
        case EPanel::ItemContainer:
            FGangstarLocalDebugItemContainerPanel::Draw(Context);
            break;
        case EPanel::Procurement:
            FGangstarLocalDebugProcurementPanel::Draw(Context);
            break;
        case EPanel::TearDownDynamics:
            FGangstarLocalDebugTearDownDynamicsPanel::Draw(Context);
            break;
        case EPanel::TradeCalc:
            FGangstarLocalDebugTradeCalcPanel::Draw(Context);
            break;
        default:
            ImGui::TextUnformatted("Panel is not implemented.");
            break;
        }
    }
}

class FGangstarLocalDebugInputProcessor : public IInputProcessor
{
public:
	explicit FGangstarLocalDebugInputProcessor(FGangstarLocalDebugModule& InOwner)
		: Owner(InOwner)
	{
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.IsRepeat())
		{
			return false;
		}

		if (InKeyEvent.GetKey() == EKeys::F8)
		{
			Owner.ToggleImGuiInput();
			return true;
		}

		if (InKeyEvent.GetKey() == EKeys::F9)
		{
			Owner.ToggleDebugWindow();
			return true;
		}

		return false;
	}

	virtual const TCHAR* GetDebugName() const override
	{
		return TEXT("GangstarLocalDebugInputProcessor");
	}

private:
	FGangstarLocalDebugModule& Owner;
};
#endif // WITH_GANGSTAR_LOCAL_IMGUI

FGangstarLocalDebugModule::FGangstarLocalDebugModule()
	: bShowDebugWindow(true)
{
}

void FGangstarLocalDebugModule::StartupModule()
{
	UE_LOG(LogGangstarLocalDebugModule, Display, TEXT("[%s] StartupModule entered. WITH_GANGSTAR_LOCAL_IMGUI=%d"),
		GangstarLocalDebugModule::ModuleName,
		WITH_GANGSTAR_LOCAL_IMGUI ? 1 : 0);

	RegisterImGuiDebugDelegate();
	RegisterInputProcessor();
	ConfigureDefaultImGuiInput();
}

void FGangstarLocalDebugModule::ShutdownModule()
{
	UE_LOG(LogGangstarLocalDebugModule, Display, TEXT("[%s] ShutdownModule entered. ImGuiDelegateBound=%d"),
		GangstarLocalDebugModule::ModuleName,
		ImGuiDebugDelegateHandle.IsValid() ? 1 : 0);

	UnregisterInputProcessor();
	UnregisterImGuiDebugDelegate();
}

void FGangstarLocalDebugModule::RegisterImGuiDebugDelegate()
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	UnregisterImGuiDebugDelegate();

	ImGuiDebugDelegateHandle = FImGuiDelegates::OnMultiContextDebug().AddRaw(this, &FGangstarLocalDebugModule::DrawLocalDebugWindow);

	UE_LOG(LogGangstarLocalDebugModule, Display, TEXT("[%s] Bound FImGuiDelegates::OnMultiContextDebug."),
		GangstarLocalDebugModule::ModuleName);
#else
	UE_LOG(LogGangstarLocalDebugModule, Display, TEXT("[%s] Local ImGui is unavailable. Debug window will not be registered."),
		GangstarLocalDebugModule::ModuleName);
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugModule::UnregisterImGuiDebugDelegate()
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	if (!ImGuiDebugDelegateHandle.IsValid())
	{
		return;
	}

	FImGuiDelegates::OnMultiContextDebug().Remove(ImGuiDebugDelegateHandle);
	ImGuiDebugDelegateHandle.Reset();

	UE_LOG(LogGangstarLocalDebugModule, Display, TEXT("[%s] Unbound FImGuiDelegates::OnMultiContextDebug."),
		GangstarLocalDebugModule::ModuleName);
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugModule::RegisterInputProcessor()
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	UnregisterInputProcessor();

	if (!FSlateApplication::IsInitialized())
	{
		UE_LOG(LogGangstarLocalDebugModule, Warning, TEXT("[%s] SlateApplication is not initialized. Hotkeys are unavailable."),
			GangstarLocalDebugModule::ModuleName);
		return;
	}

	InputProcessor = MakeShared<FGangstarLocalDebugInputProcessor>(*this);
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor, 0);

	UE_LOG(LogGangstarLocalDebugModule, Display, TEXT("[%s] Registered local debug hotkeys. %s toggles ImGui input, %s toggles window visibility."),
		GangstarLocalDebugModule::ModuleName,
		GangstarLocalDebugModule::ToggleInputKeyName,
		GangstarLocalDebugModule::ToggleWindowKeyName);
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugModule::UnregisterInputProcessor()
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	if (!InputProcessor.IsValid())
	{
		return;
	}

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}

	InputProcessor.Reset();

	UE_LOG(LogGangstarLocalDebugModule, Display, TEXT("[%s] Unregistered local debug hotkeys."),
		GangstarLocalDebugModule::ModuleName);
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugModule::ConfigureDefaultImGuiInput()
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	FImGuiModuleProperties& Properties = FImGuiModule::Get().GetProperties();
	Properties.SetInputEnabled(true);
	Properties.SetMouseInputShared(true);
	Properties.SetKeyboardInputShared(false);
	Properties.SetGamepadInputShared(false);

	UE_LOG(LogGangstarLocalDebugModule, Display, TEXT("[%s] Default ImGui input configured. Input=1 MouseShared=1 KeyboardShared=0 GamepadShared=0."),
		GangstarLocalDebugModule::ModuleName);
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugModule::ToggleImGuiInput()
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	FImGuiModuleProperties& Properties = FImGuiModule::Get().GetProperties();
	Properties.ToggleInput();

	UE_LOG(LogGangstarLocalDebugModule, Display, TEXT("[%s] Toggled ImGui input to %d."),
		GangstarLocalDebugModule::ModuleName,
		Properties.IsInputEnabled() ? 1 : 0);
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugModule::ToggleDebugWindow()
{
	bShowDebugWindow = !bShowDebugWindow;

	UE_LOG(LogGangstarLocalDebugModule, Display, TEXT("[%s] Toggled debug window visibility to %d."),
		GangstarLocalDebugModule::ModuleName,
		bShowDebugWindow ? 1 : 0);
}

void FGangstarLocalDebugModule::DrawLocalDebugWindow()
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	if (!bShowDebugWindow)
	{
		return;
	}

	FGangstarLocalDebugContext Context = FGangstarLocalDebugContext::Build();
	FGangstarLocalDebugGoalPanel::Capture(Context);

	ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowBgAlpha(0.55f);
	if (!ImGui::Begin("Gangstar Local Debug", &bShowDebugWindow))
	{
		ImGui::End();
		return;
	}

	ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "F8: Toggle ImGui input | F9: Show / hide this window");
	ImGui::SameLine();
	ImGui::Text("Input: %s", FImGuiModule::Get().GetProperties().IsInputEnabled() ? "On" : "Off");
	ImGui::Separator();

	static GangstarLocalDebugNavigation::EPanel SelectedPanel = GangstarLocalDebugNavigation::EPanel::ProjectMap;

	ImGui::BeginChild("GangstarLocalDebugNavigation", ImVec2(220.0f, 0.0f), true);
	GangstarLocalDebugNavigation::DrawNavigation(SelectedPanel);
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("GangstarLocalDebugContent", ImVec2(0.0f, 0.0f), true);
	GangstarLocalDebugNavigation::DrawSelectedPanel(SelectedPanel, Context);
	ImGui::EndChild();

	ImGui::End();
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}
