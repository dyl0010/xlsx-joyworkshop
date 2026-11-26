#include "Panels/GangstarLocalDebugActivityPanel.h"

#include "GangstarLocalDebugContext.h"
#include "GangstarLocalDebugHelpers.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
#include "ActivityController/ActivityControllerDef.h"
#include "ActivityManager/ActivityManagerDef.h"
#include "Goal/GangstarGoalCounter.h"
#include "Goal/GoalDataManager.h"
#include "GSTMGameFramework/Table/TableManager.h"
#include <imgui.h>
#endif // WITH_GANGSTAR_LOCAL_IMGUI

namespace GangstarLocalDebugActivityPanel
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	struct FActivityPanelState
	{
		FName SelectedActivityId;
		char ActivityFilter[128] = {};
	};

	struct FActivityControllerRuntimeInfo
	{
		FName ActivityControllerID;
		EActivityStatusType ActivityStatusType = EActivityStatusType::Close;
		int64 NextTimeStamp = 0;
		int64 CurTimeStamp = 0;
	};

	static FActivityPanelState& GetState()
	{
		static FActivityPanelState State;
		return State;
	}

	static FString FormatNameOrNone(const FName& Value)
	{
		return Value.IsNone() ? FString(TEXT("None")) : Value.ToString();
	}

	static FString FormatUnixTimeOrDash(const int64 UnixTime)
	{
		return UnixTime > 0 ? GangstarLocalDebugHelpers::FormatUnixTime(UnixTime) : FString(TEXT("--"));
	}

	static bool ContainsFilter(const FString& Text, const FString& FilterText)
	{
		return FilterText.IsEmpty() || Text.Contains(FilterText, ESearchCase::IgnoreCase);
	}

	static bool PassesActivityFilter(const FName& ActivityId, const FActivityManagerTable& ActivityTable, const char* Filter)
	{
		if (!Filter || Filter[0] == '\0')
		{
			return true;
		}

		const FString FilterText = UTF8_TO_TCHAR(Filter);
		return ContainsFilter(ActivityId.ToString(), FilterText)
			|| ContainsFilter(ActivityTable.ControllerID.ToString(), FilterText)
			|| ContainsFilter(GangstarLocalDebugHelpers::FormatText(ActivityTable.DisplayTitle), FilterText)
			|| ContainsFilter(GangstarLocalDebugHelpers::FormatEnum(ActivityTable.ActivityType), FilterText);
	}

	static FString FormatRuntimeActivityData(const FActivityData& ActivityData)
	{
		return FString::Printf(
			TEXT("Status=%s Unlock=%s Visible=%s Open=%s Close=%s Version=%s"),
			*GangstarLocalDebugHelpers::FormatEnum(ActivityData.ActivityStatusType),
			*GangstarLocalDebugHelpers::FormatBool(ActivityData.IsUnLock),
			*GangstarLocalDebugHelpers::FormatBool(ActivityData.IsVisible),
			*FormatUnixTimeOrDash(ActivityData.OpenUTCTimestamp),
			*FormatUnixTimeOrDash(ActivityData.CloseUTCTimestamp),
			*ActivityData.ActvityVersion);
	}

	static FString FormatGoalTargetCount(const FGoalTable* GoalData)
	{
		return GoalData ? FString::Printf(TEXT("%lld"), static_cast<int64>(GoalData->TargetCount)) : FString(TEXT("MissingTable"));
	}

	static FString FormatGoalConditionType(const TEnumAsByte<EGoalConditionType> Type)
	{
		const UEnum* Enum = StaticEnum<EGoalConditionType>();
		return Enum ? Enum->GetNameStringByValue(Type.GetValue()) : FString::Printf(TEXT("%d"), Type.GetValue());
	}

	static const FActivityGoalTable* FindActivityGoalTable(const FName& ActivityId, bool& bIsSpecial)
	{
		bIsSpecial = false;
		if (const FActivityGoalSpecialTable* SpecialGoalTable = UTableManager::Get()->FindTableRow<FActivityGoalSpecialTable>(ActivityId))
		{
			bIsSpecial = true;
			return SpecialGoalTable;
		}
		return UTableManager::Get()->FindTableRow<FActivityGoalTable>(ActivityId);
	}

	static UClass* FindGameplayActivityFunctionLibraryClass()
	{
		return FindObject<UClass>(ANY_PACKAGE, TEXT("GameplayActivityFunctionLibrary"));
	}

	static bool InvokeGetActivityControllerInfo(UObject* WorldContextObject, const FName& ActivityControllerID, FActivityControllerRuntimeInfo& OutControllerInfo)
	{
		UClass* FunctionLibraryClass = FindGameplayActivityFunctionLibraryClass();
		if (!FunctionLibraryClass)
		{
			return false;
		}

		UFunction* Function = FunctionLibraryClass->FindFunctionByName(TEXT("GetActivityControllerInfo"));
		if (!Function)
		{
			return false;
		}

		struct FGetActivityControllerInfoParams
		{
			UObject* WorldContextObject = nullptr;
			FName ActivityControllerID;
			FActivityControllerRuntimeInfo OutControllerInfo;
			bool ReturnValue = false;
		};

		FGetActivityControllerInfoParams Params;
		Params.WorldContextObject = WorldContextObject;
		Params.ActivityControllerID = ActivityControllerID;
		FunctionLibraryClass->GetDefaultObject()->ProcessEvent(Function, &Params);
		OutControllerInfo = Params.OutControllerInfo;
		return Params.ReturnValue;
	}

	static bool InvokeGetActivityOpenCloseTime(UObject* WorldContextObject, const FName& ActivityID, int64& OpenTimeStamp, int64& CloseTimeStamp)
	{
		UClass* FunctionLibraryClass = FindGameplayActivityFunctionLibraryClass();
		if (!FunctionLibraryClass)
		{
			return false;
		}

		UFunction* Function = FunctionLibraryClass->FindFunctionByName(TEXT("GetActivityOpenCloseTime"));
		if (!Function)
		{
			return false;
		}

		struct FGetActivityOpenCloseTimeParams
		{
			UObject* WorldContextObject = nullptr;
			FName ActivityID;
			int64 OpenTimeStamp = 0;
			int64 CloseTimeStamp = 0;
			bool ReturnValue = false;
		};

		FGetActivityOpenCloseTimeParams Params;
		Params.WorldContextObject = WorldContextObject;
		Params.ActivityID = ActivityID;
		FunctionLibraryClass->GetDefaultObject()->ProcessEvent(Function, &Params);
		OpenTimeStamp = Params.OpenTimeStamp;
		CloseTimeStamp = Params.CloseTimeStamp;
		return Params.ReturnValue;
	}

	static void DrawActivityList(const FGangstarLocalDebugContext& Context, FActivityPanelState& State)
	{
		TMap<FName, const FActivityManagerTable*> ActivityRows;
		UTableManager::Get()->TryGetAllData<FActivityManagerTable>(ActivityRows);

		TArray<FName> ActivityIds;
		ActivityRows.GetKeys(ActivityIds);
		ActivityIds.Sort([](const FName& Left, const FName& Right)
		{
			return Left.LexicalLess(Right);
		});

		ImGui::PushItemWidth(320.0f);
		ImGui::InputText("Filter##ActivityFilter", State.ActivityFilter, UE_ARRAY_COUNT(State.ActivityFilter));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::TextDisabled("ActivityID / ControllerID / Title / Type");

		int32 VisibleActivityCount = 0;
		for (const FName& ActivityId : ActivityIds)
		{
			const FActivityManagerTable* ActivityTable = ActivityRows.FindRef(ActivityId);
			if (ActivityTable && PassesActivityFilter(ActivityId, *ActivityTable, State.ActivityFilter))
			{
				++VisibleActivityCount;
			}
		}

		ImGui::Text("Activities: %d / %d", VisibleActivityCount, ActivityIds.Num());
		ImGui::BeginChild("ActivityList", ImVec2(0.0f, 260.0f), true);
		ImGui::Columns(5, "ActivityListColumns", true);
		ImGui::TextUnformatted("ActivityID"); ImGui::NextColumn();
		ImGui::TextUnformatted("ControllerID"); ImGui::NextColumn();
		ImGui::TextUnformatted("Type"); ImGui::NextColumn();
		ImGui::TextUnformatted("Enabled"); ImGui::NextColumn();
		ImGui::TextUnformatted("Runtime"); ImGui::NextColumn();
		ImGui::Separator();

		for (const FName& ActivityId : ActivityIds)
		{
			const FActivityManagerTable* ActivityTable = ActivityRows.FindRef(ActivityId);
			if (!ActivityTable || !PassesActivityFilter(ActivityId, *ActivityTable, State.ActivityFilter))
			{
				continue;
			}

			const bool bSelected = State.SelectedActivityId == ActivityId;
			const FString ActivityIdText = FormatNameOrNone(ActivityId);
			const FTCHARToUTF8 ActivityIdUtf8(*ActivityIdText);
			ImGui::PushID(ActivityIdUtf8.Get());
			if (ImGui::Selectable(ActivityIdUtf8.Get(), bSelected))
			{
				State.SelectedActivityId = ActivityId;
				ImGui::SetClipboardText(ActivityIdUtf8.Get());
			}
			ImGui::NextColumn();

			GangstarLocalDebugHelpers::DrawCopyableText(FormatNameOrNone(ActivityTable->ControllerID)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(ActivityTable->ActivityType)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatBool(ActivityTable->Enable)); ImGui::NextColumn();

			FActivityControllerRuntimeInfo ControllerInfo;
			const bool bHasControllerInfo = InvokeGetActivityControllerInfo(Context.World, ActivityTable->ControllerID, ControllerInfo);
			GangstarLocalDebugHelpers::DrawCopyableText(bHasControllerInfo
				? GangstarLocalDebugHelpers::FormatEnum(ControllerInfo.ActivityStatusType)
				: FString(TEXT("Missing"))); ImGui::NextColumn();
			ImGui::PopID();
		}
		ImGui::Columns(1);
		ImGui::EndChild();
	}

	static void DrawSelectedActivityBasic(const FGangstarLocalDebugContext& Context, const FName& ActivityId, const FActivityManagerTable& ActivityTable)
	{
		const FActivityControllerTable* ControllerTable = UTableManager::Get()->FindTableRow<FActivityControllerTable>(ActivityTable.ControllerID);
		const bool bHasControllerTable = ControllerTable != nullptr;

		FActivityControllerRuntimeInfo ControllerInfo;
		const bool bHasControllerInfo = InvokeGetActivityControllerInfo(Context.World, ActivityTable.ControllerID, ControllerInfo);

		int64 OpenTimeStamp = 0;
		int64 CloseTimeStamp = 0;
		const bool bHasOpenCloseTime = InvokeGetActivityOpenCloseTime(Context.World, ActivityId, OpenTimeStamp, CloseTimeStamp);

		ImGui::TextUnformatted("Selected Activity");
		ImGui::Columns(2, "SelectedActivityBasicColumns", false);
		ImGui::TextUnformatted("ActivityID"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(FormatNameOrNone(ActivityId)); ImGui::NextColumn();
		ImGui::TextUnformatted("DisplayTitle"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatText(ActivityTable.DisplayTitle)); ImGui::NextColumn();
		ImGui::TextUnformatted("ControllerID"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(FormatNameOrNone(ActivityTable.ControllerID)); ImGui::NextColumn();
		ImGui::TextUnformatted("ActivityType"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(ActivityTable.ActivityType)); ImGui::NextColumn();
		ImGui::TextUnformatted("Enable"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatBool(ActivityTable.Enable)); ImGui::NextColumn();
		ImGui::TextUnformatted("HideWhenComplete"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatBool(ActivityTable.HideWhenComplete)); ImGui::NextColumn();
		ImGui::TextUnformatted("WidgetClass"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(ActivityTable.WidgetClass.ToSoftObjectPath().ToString()); ImGui::NextColumn();
		ImGui::TextUnformatted("Runtime ActivityData"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(TEXT("Not linked in local panel MVP")); ImGui::NextColumn();
		ImGui::Columns(1);

		ImGui::Separator();
		ImGui::TextUnformatted("Time / Runtime Status");
		ImGui::Columns(2, "SelectedActivityTimeColumns", false);
		ImGui::TextUnformatted("Controller Config"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(bHasControllerTable ? TEXT("Found") : TEXT("Missing")); ImGui::NextColumn();
		if (bHasControllerTable)
		{
			ImGui::TextUnformatted("Config OpenTime"); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(ControllerTable->OpenTime); ImGui::NextColumn();
			ImGui::TextUnformatted("Config CloseTime"); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(ControllerTable->CloseTime); ImGui::NextColumn();
			ImGui::TextUnformatted("Config ForcedClose"); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatBool(ControllerTable->ForcedClose)); ImGui::NextColumn();
			ImGui::TextUnformatted("TimeRangeType"); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(ControllerTable->TimeRangeType)); ImGui::NextColumn();
			ImGui::TextUnformatted("TimePeriodType"); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(ControllerTable->TimePeriodType)); ImGui::NextColumn();
			ImGui::TextUnformatted("ControlledGameplayType"); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(ControllerTable->ControlledGameplayType)); ImGui::NextColumn();
		}
		ImGui::TextUnformatted("Runtime ControllerInfo"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(bHasControllerInfo ? TEXT("Found") : TEXT("Missing")); ImGui::NextColumn();
		if (bHasControllerInfo)
		{
			ImGui::TextUnformatted("Runtime Status"); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(ControllerInfo.ActivityStatusType)); ImGui::NextColumn();
			ImGui::TextUnformatted("NextTimeStamp"); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FormatUnixTimeOrDash(ControllerInfo.NextTimeStamp)); ImGui::NextColumn();
			ImGui::TextUnformatted("CurTimeStamp"); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FormatUnixTimeOrDash(ControllerInfo.CurTimeStamp)); ImGui::NextColumn();
		}
		ImGui::TextUnformatted("Runtime OpenCloseTime"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(bHasOpenCloseTime
			? FString::Printf(TEXT("%s -> %s"), *FormatUnixTimeOrDash(OpenTimeStamp), *FormatUnixTimeOrDash(CloseTimeStamp))
			: FString(TEXT("Missing"))); ImGui::NextColumn();
		ImGui::Columns(1);
	}

	static void DrawActivityGoals(const FGangstarLocalDebugContext& Context, const FName& ActivityId)
	{
		(void)Context;
		bool bIsSpecialGoalTable = false;
		const FActivityGoalTable* ActivityGoalTable = FindActivityGoalTable(ActivityId, bIsSpecialGoalTable);
		ImGui::Text("Activity Goals: %s", ActivityGoalTable ? (bIsSpecialGoalTable ? "SpecialTable" : "Table") : "Missing");
		if (!ActivityGoalTable)
		{
			return;
		}

		ImGui::Columns(7, "ActivityGoalColumns", true);
		ImGui::TextUnformatted("Group"); ImGui::NextColumn();
		ImGui::TextUnformatted("WaitDays"); ImGui::NextColumn();
		ImGui::TextUnformatted("GoalId"); ImGui::NextColumn();
		ImGui::TextUnformatted("GoalTable"); ImGui::NextColumn();
		ImGui::TextUnformatted("Target"); ImGui::NextColumn();
		ImGui::TextUnformatted("Condition"); ImGui::NextColumn();
		ImGui::TextUnformatted("Remarks"); ImGui::NextColumn();
		ImGui::Separator();

		for (const FActivityGoalMissionGroup& MissionGroup : ActivityGoalTable->MissionGroupConfigs)
		{
			if (MissionGroup.ActivityGoals.Num() == 0)
			{
				GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), MissionGroup.GroupID)); ImGui::NextColumn();
				GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), MissionGroup.WaitingTime)); ImGui::NextColumn();
				ImGui::TextUnformatted("--"); ImGui::NextColumn();
				ImGui::TextUnformatted("--"); ImGui::NextColumn();
				ImGui::TextUnformatted("--"); ImGui::NextColumn();
				ImGui::TextUnformatted("--"); ImGui::NextColumn();
				ImGui::TextUnformatted("--"); ImGui::NextColumn();
				continue;
			}

			for (const FName& GoalId : MissionGroup.ActivityGoals)
			{
				const FGoalTable* GoalData = FindGoalData(GoalId);

				GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), MissionGroup.GroupID)); ImGui::NextColumn();
				GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), MissionGroup.WaitingTime)); ImGui::NextColumn();
				GangstarLocalDebugHelpers::DrawCopyableText(FormatNameOrNone(GoalId)); ImGui::NextColumn();
				GangstarLocalDebugHelpers::DrawCopyableText(GoalData ? TEXT("Found") : TEXT("Missing")); ImGui::NextColumn();
				GangstarLocalDebugHelpers::DrawCopyableText(FormatGoalTargetCount(GoalData)); ImGui::NextColumn();
				GangstarLocalDebugHelpers::DrawCopyableText(GoalData ? FormatGoalConditionType(GoalData->FinishCondition.Type) : TEXT("MissingTable")); ImGui::NextColumn();
				GangstarLocalDebugHelpers::DrawCopyableText(GoalData ? GoalData->Remarks : FString(TEXT("--"))); ImGui::NextColumn();
			}
		}
		ImGui::Columns(1);
	}
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugActivityPanel::Draw(const FGangstarLocalDebugContext& Context)
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	auto& State = GangstarLocalDebugActivityPanel::GetState();
	GangstarLocalDebugActivityPanel::DrawActivityList(Context, State);
	ImGui::Separator();

	if (State.SelectedActivityId.IsNone())
	{
		ImGui::TextUnformatted("Select an ActivityID above to inspect config, runtime status, and related goals.");
		return;
	}

	const FActivityManagerTable* ActivityTable = UTableManager::Get()->FindTableRow<FActivityManagerTable>(State.SelectedActivityId);
	if (!ActivityTable)
	{
		const FString SelectedActivityIdText = State.SelectedActivityId.ToString();
		ImGui::Text("Selected activity not found: %s", TCHAR_TO_UTF8(*SelectedActivityIdText));
		return;
	}

	GangstarLocalDebugActivityPanel::DrawSelectedActivityBasic(Context, State.SelectedActivityId, *ActivityTable);
	ImGui::Separator();
	GangstarLocalDebugActivityPanel::DrawActivityGoals(Context, State.SelectedActivityId);
#else
	(void)Context;
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}
