#include "Panels/GangstarLocalDebugGoalScoreContainerPanel.h"

#include "GangstarLocalDebugContext.h"
#include "GangstarLocalDebugHelpers.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
#include "Characters/Player/Components/PlayerGeneralScoreComponent.h"
#include "Goal/GangstarGoalConst.h"
#include "Goal/GangstarGoalCounter.h"
#include "GSTMGameFramework/Table/TableManager.h"
#include <imgui.h>
#endif // WITH_GANGSTAR_LOCAL_IMGUI

namespace GangstarLocalDebugGoalScoreContainerPanel
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	static FString FormatStringParams(const TArray<FString>& Params)
	{
		return Params.Num() > 0 ? FString::Join(Params, TEXT(", ")) : TEXT("None");
	}

	static FString FormatGoalConditionType(TEnumAsByte<EGoalConditionType> Type)
	{
		const UEnum* Enum = StaticEnum<EGoalConditionType>();
		return Enum ? Enum->GetNameStringByValue(Type.GetValue()) : FString::Printf(TEXT("%d"), Type.GetValue());
	}

	static FString FormatGoalCompares(const TArray<FGoalCompare>& ExtraCompares)
	{
		if (ExtraCompares.Num() <= 0)
		{
			return TEXT("None");
		}

		TArray<FString> CompareTexts;
		CompareTexts.Reserve(ExtraCompares.Num());
		for (const FGoalCompare& ExtraCompare : ExtraCompares)
		{
			CompareTexts.Add(FString::Printf(TEXT("%s(%s)"), *GangstarLocalDebugHelpers::FormatEnum(ExtraCompare.Type), *FormatStringParams(ExtraCompare.Params)));
		}
		return FString::Join(CompareTexts, TEXT(" | "));
	}

	static bool PassesScoreContainerFilter(const FName& ScoreDataId, const FGeneralScoreTable* ScoreTable, const char* Filter)
	{
		if (!Filter || Filter[0] == '\0')
		{
			return true;
		}

		const FString FilterText = UTF8_TO_TCHAR(Filter);
		return ScoreDataId.ToString().Contains(FilterText, ESearchCase::IgnoreCase)
			|| (ScoreTable && ScoreTable->ContainerName.Contains(FilterText, ESearchCase::IgnoreCase));
	}

	static void DrawGeneralScoreRuleDetails(const FName& ScoreDataId, const FGeneralScoreTable& ScoreTable)
	{
		if (!ImGui::TreeNode(TCHAR_TO_UTF8(*FString::Printf(TEXT("Rules##%s"), *ScoreDataId.ToString()))))
		{
			return;
		}

		ImGui::Columns(9, "GeneralScoreRuleColumns", true);
		ImGui::TextUnformatted("Index"); ImGui::NextColumn();
		ImGui::TextUnformatted("Condition"); ImGui::NextColumn();
		ImGui::TextUnformatted("Params"); ImGui::NextColumn();
		ImGui::TextUnformatted("ExtraCompares"); ImGui::NextColumn();
		ImGui::TextUnformatted("Priority"); ImGui::NextColumn();
		ImGui::TextUnformatted("ChangeType"); ImGui::NextColumn();
		ImGui::TextUnformatted("ChangeValue"); ImGui::NextColumn();
		ImGui::TextUnformatted("ValueLimit"); ImGui::NextColumn();
		ImGui::TextUnformatted("LimitValue"); ImGui::NextColumn();
		ImGui::Separator();

		for (int32 RuleIndex = 0; RuleIndex < ScoreTable.ScoreRules.Num(); ++RuleIndex)
		{
			const FGeneralScoreRule& Rule = ScoreTable.ScoreRules[RuleIndex];
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), RuleIndex)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FormatGoalConditionType(Rule.FinishCondition.Type)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FormatStringParams(Rule.FinishCondition.Params)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FormatGoalCompares(Rule.ExtraCompares)); ImGui::NextColumn();
			ImGui::Text("%d", Rule.ScoreRulePriority); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(Rule.ScoreChangeType)); ImGui::NextColumn();
			ImGui::Text("%d", Rule.ScoreChangeValue); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatBool(Rule.IsValueLimit)); ImGui::NextColumn();
			ImGui::Text("%lld", Rule.ValueLimit); ImGui::NextColumn();
		}
		ImGui::Columns(1);
		ImGui::TreePop();
	}
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugGoalScoreContainerPanel::Draw(const FGangstarLocalDebugContext& Context)
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	ImGui::TextUnformatted("Source: UPlayerGeneralScoreComponent replicated ScoreContainer / Score / TotalScore");
	if (!Context.PlayerController)
	{
		ImGui::TextUnformatted("PlayerController not found.");
		return;
	}

	const UPlayerGeneralScoreComponent* ScoreComponent = Cast<UPlayerGeneralScoreComponent>(
		Context.PlayerController->GetComponentByClass(UPlayerGeneralScoreComponent::StaticClass()));
	if (!ScoreComponent)
	{
		ImGui::TextUnformatted("UPlayerGeneralScoreComponent not found.");
		return;
	}

	static char ScoreContainerFilter[128] = "";
	ImGui::PushItemWidth(260.0f);
	ImGui::InputText("Filter##GeneralScoreContainerFilter", ScoreContainerFilter, UE_ARRAY_COUNT(ScoreContainerFilter));
	ImGui::PopItemWidth();
	ImGui::SameLine();
	ImGui::TextDisabled("ScoreDataId / ContainerName");

	const TArray<FName>& ScoreContainers = ScoreComponent->GetScoreContainer();
	ImGui::Text("Active Container Count: %d", ScoreContainers.Num());
	if (ScoreContainers.Num() <= 0)
	{
		ImGui::TextUnformatted("No active score containers replicated to this client.");
		return;
	}

	ImGui::Columns(5, "GeneralScoreContainerColumns", true);
	ImGui::TextUnformatted("ScoreDataId"); ImGui::NextColumn();
	ImGui::TextUnformatted("ContainerName"); ImGui::NextColumn();
	ImGui::TextUnformatted("PlayerScore"); ImGui::NextColumn();
	ImGui::TextUnformatted("TotalScore"); ImGui::NextColumn();
	ImGui::TextUnformatted("RuleCount"); ImGui::NextColumn();
	ImGui::Separator();

	TArray<FName> SortedScoreContainers = ScoreContainers;
	SortedScoreContainers.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	TArray<TPair<FName, const FGeneralScoreTable*>> VisibleTables;
	for (const FName& ScoreDataId : SortedScoreContainers)
	{
		const FGeneralScoreTable* ScoreTable = UTableManager::Get()
			? UTableManager::Get()->FindTableRow<FGeneralScoreTable>(ScoreDataId, TEXT("GangstarLocalDebugGoalScoreContainerPanel"), false)
			: nullptr;
		if (!GangstarLocalDebugGoalScoreContainerPanel::PassesScoreContainerFilter(ScoreDataId, ScoreTable, ScoreContainerFilter))
		{
			continue;
		}

		VisibleTables.Emplace(ScoreDataId, ScoreTable);
		GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(ScoreDataId)); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(ScoreTable ? ScoreTable->ContainerName : TEXT("<missing table row>")); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%lld"), ScoreComponent->GetScore(ScoreDataId))); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%lld"), ScoreComponent->GetTotalScore(ScoreDataId))); ImGui::NextColumn();
		ImGui::Text("%d", ScoreTable ? ScoreTable->ScoreRules.Num() : 0); ImGui::NextColumn();
	}
	ImGui::Columns(1);

	if (VisibleTables.Num() <= 0)
	{
		ImGui::TextUnformatted("No matching score containers.");
		return;
	}

	ImGui::Separator();
	for (const TPair<FName, const FGeneralScoreTable*>& VisibleTable : VisibleTables)
	{
		if (VisibleTable.Value)
		{
			GangstarLocalDebugGoalScoreContainerPanel::DrawGeneralScoreRuleDetails(VisibleTable.Key, *VisibleTable.Value);
		}
	}
#else
	(void)Context;
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}
