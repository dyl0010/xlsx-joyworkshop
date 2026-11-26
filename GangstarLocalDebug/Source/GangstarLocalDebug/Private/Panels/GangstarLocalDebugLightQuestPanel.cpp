#include "Panels/GangstarLocalDebugLightQuestPanel.h"

#include "GangstarLocalDebugContext.h"
#include "GangstarLocalDebugHelpers.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
#include "LightQuest/LightQuestComponent.h"
#include "LightQuest/LightQuestTypes.h"
#include <imgui.h>
#endif // WITH_GANGSTAR_LOCAL_IMGUI

namespace GangstarLocalDebugLightQuestPanel
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	static void DrawQuestHeader()
	{
		ImGui::Columns(9, "LightQuestColumns", true);
		ImGui::TextUnformatted("Main"); ImGui::NextColumn();
		ImGui::TextUnformatted("InstanceId"); ImGui::NextColumn();
		ImGui::TextUnformatted("QuestId"); ImGui::NextColumn();
		ImGui::TextUnformatted("Type"); ImGui::NextColumn();
		ImGui::TextUnformatted("State"); ImGui::NextColumn();
		ImGui::TextUnformatted("FailReason"); ImGui::NextColumn();
		ImGui::TextUnformatted("Category"); ImGui::NextColumn();
		ImGui::TextUnformatted("Priority"); ImGui::NextColumn();
		ImGui::TextUnformatted("Time"); ImGui::NextColumn();
		ImGui::Separator();
	}
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugLightQuestPanel::Draw(const FGangstarLocalDebugContext& Context)
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	if (!Context.LightQuestComponent)
	{
		ImGui::TextUnformatted("LightQuestComponent not found.");
		return;
	}

	const TArray<FLightQuestClientData>& Quests = Context.LightQuestComponent->GetClientQuestList();
	ImGui::Text("Quest Count: %d", Quests.Num());
	ImGui::Text("MainQuestInstanceId: %s", TCHAR_TO_UTF8(*GangstarLocalDebugHelpers::FormatGuid(Context.LightQuestComponent->GetMainQuestInstanceId())));
	ImGui::Text("ManualMainQuest: %s", Context.LightQuestComponent->IsManualMainQuest() ? "true" : "false");
	ImGui::Separator();

	GangstarLocalDebugLightQuestPanel::DrawQuestHeader();
	for (const FLightQuestClientData& Quest : Quests)
	{
		GangstarLocalDebugHelpers::DrawCopyableText(Quest.bIsMainQuest ? TEXT("*") : TEXT("")); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatGuid(Quest.InstanceId)); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Quest.QuestId)); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(Quest.QuestType)); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(Quest.State)); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(Quest.FailReason)); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Quest.ShowCategory)); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Quest.SortPriority)); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("Start=%s End=%s"),
			*GangstarLocalDebugHelpers::FormatUnixTime(Quest.StartRealTime),
			*GangstarLocalDebugHelpers::FormatUnixTime(Quest.EndRealTime))); ImGui::NextColumn();
	}
	ImGui::Columns(1);
#else
	(void)Context;
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}
