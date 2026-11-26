#include "Panels/GangstarLocalDebugGoalPanel.h"

#include "GangstarLocalDebugContext.h"
#include "GangstarLocalDebugHelpers.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
#include "Characters/BasePlayerState.h"
#include "GM/GmHelperActor.h"
#include "GM/GmRpcParams.h"
#include "Goal/GangstarGoalConst.h"
#include "Goal/GangstarGoalCounter.h"
#include "Goal/GoalDataManager.h"
#include "HAL/PlatformTime.h"
#include "PlayerProfile/PlayerProfileContainer.h"
#include <imgui.h>
#endif // WITH_GANGSTAR_LOCAL_IMGUI

namespace GangstarLocalDebugGoalPanel
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	static constexpr int32 MaxHistoryCount = 256;
	static constexpr int32 AllSystemTypes = INDEX_NONE;
	static constexpr float CurrentGoalListHeight = 380.0f;

	enum class EGoalStateFilter : uint8
	{
		All,
		Active,
		Finished,
	};

	enum class EGoalTransition : uint8
	{
		Accept,
		Progress,
		Finish,
		Remove,
	};

	struct FGoalSnapshotKey
	{
		int32 SystemType = 0;
		FName GoalDataId;
		int32 ConditionIndex = 0;

		friend bool operator==(const FGoalSnapshotKey& A, const FGoalSnapshotKey& B)
		{
			return A.SystemType == B.SystemType
				&& A.GoalDataId == B.GoalDataId
				&& A.ConditionIndex == B.ConditionIndex;
		}

		friend uint32 GetTypeHash(const FGoalSnapshotKey& Key)
		{
			uint32 Hash = ::GetTypeHash(Key.SystemType);
			Hash = HashCombine(Hash, ::GetTypeHash(Key.GoalDataId));
			return HashCombine(Hash, ::GetTypeHash(Key.ConditionIndex));
		}
	};

	struct FGoalSnapshotValue
	{
		bool bFinished = false;
		int64 Progress = 0;
		int64 UpdateTime = 0;
	};

	struct FGoalChangeInfo
	{
		EGoalTransition Transition = EGoalTransition::Progress;
		int64 DeltaProgress = 0;
		double CaptureTime = 0.0;
	};

	struct FGoalHistoryEvent
	{
		FGoalSnapshotKey Key;
		EGoalTransition Transition = EGoalTransition::Progress;
		int64 OldProgress = 0;
		int64 NewProgress = 0;
		bool bOldFinished = false;
		bool bNewFinished = false;
		int64 UpdateTime = 0;
		double CaptureTime = 0.0;
	};

	struct FGoalPanelState
	{
		TWeakObjectPtr<UWorld> CapturedWorld;
		TWeakObjectPtr<ABasePlayerState> CapturedPlayerState;
		int64 CapturedRoleId = 0;
		TMap<FGoalSnapshotKey, FGoalSnapshotValue> CurrentSnapshot;
		TMap<FGoalSnapshotKey, FGoalChangeInfo> ChangeInfo;
		TArray<FGoalHistoryEvent> History;
		int32 HistoryStart = 0;
		uint64 LastCaptureFrame = MAX_uint64;
		int32 SelectedSystemType = AllSystemTypes;
		EGoalStateFilter StateFilter = EGoalStateFilter::All;
		FGoalSnapshotKey SelectedGoalKey;
		bool bContextInitialized = false;
		bool bHasBaseline = false;
		bool bCapturePaused = false;
		bool bChangedOnly = false;
		bool bHasSelectedGoal = false;
		bool bEnableGmActions = true;
		bool bShowAllTableGoals = false;
		char GoalDataIdFilter[128] = {};
	};

	struct FGoalDisplayRow
	{
		FGoalSnapshotKey Key;
		FGoalSnapshotValue RuntimeValue;
		const FGoalTable* GoalData = nullptr;
		bool bHasRuntimeRecord = false;
	};

	static FGoalPanelState& GetState()
	{
		static FGoalPanelState State;
		static bool bInitialized = false;
		if (!bInitialized)
		{
			State.History.Reserve(MaxHistoryCount);
			bInitialized = true;
		}
		return State;
	}

	static const TCHAR* GetTransitionText(const EGoalTransition Transition)
	{
		switch (Transition)
		{
		case EGoalTransition::Accept:
			return TEXT("Accept");
		case EGoalTransition::Progress:
			return TEXT("Progress");
		case EGoalTransition::Finish:
			return TEXT("Finish");
		case EGoalTransition::Remove:
			return TEXT("Remove");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* GetGoalStateText(const bool bFinished)
	{
		return bFinished ? TEXT("Finished") : TEXT("Active");
	}

	static FString FormatSystemTypeName(const int32 SystemType)
	{
		if (SystemType >= EGST_None && SystemType < EGST_Count)
		{
			return UEnum::GetDisplayValueAsText(static_cast<EGoalSystemType>(SystemType)).ToString();
		}

		switch (static_cast<EExtendGoalSystemType>(SystemType))
		{
		case EExtendGoalSystemType::None:
			return TEXT("ExtendNone");
		case EExtendGoalSystemType::NpcFavorability:
			return TEXT("NpcFavorability");
		case EExtendGoalSystemType::GeneralScore:
			return TEXT("GeneralScore");
		case EExtendGoalSystemType::ArenaRacingStar:
			return TEXT("ArenaRacingStar");
		case EExtendGoalSystemType::Count:
			return TEXT("ExtendCount");
		default:
			return FString::Printf(TEXT("Unknown(%d)"), SystemType);
		}
	}

	static void ResetForContext(FGoalPanelState& State, const FGangstarLocalDebugContext& Context)
	{
		State.CapturedWorld = Context.World;
		State.CapturedPlayerState = Context.PlayerState;
		State.CapturedRoleId = Context.PlayerState ? Context.PlayerState->GetRoleId() : 0;
		State.CurrentSnapshot.Reset();
		State.ChangeInfo.Reset();
		State.History.Reset();
		State.HistoryStart = 0;
		State.LastCaptureFrame = MAX_uint64;
		State.bHasBaseline = false;
		State.bHasSelectedGoal = false;
		State.SelectedGoalKey = FGoalSnapshotKey();
		State.bContextInitialized = true;
	}

	static void AddHistory(FGoalPanelState& State, const FGoalHistoryEvent& Event)
	{
		if (State.History.Num() < MaxHistoryCount)
		{
			State.History.Add(Event);
			return;
		}

		State.History[State.HistoryStart] = Event;
		State.HistoryStart = (State.HistoryStart + 1) % MaxHistoryCount;
	}

	static const FGoalHistoryEvent& GetHistoryNewestFirst(const FGoalPanelState& State, const int32 NewestIndex)
	{
		const int32 HistoryIndex = (State.HistoryStart + State.History.Num() - 1 - NewestIndex) % State.History.Num();
		return State.History[HistoryIndex];
	}

	static FGoalHistoryEvent MakeHistoryEvent(
		const FGoalSnapshotKey& Key,
		const EGoalTransition Transition,
		const FGoalSnapshotValue& OldValue,
		const FGoalSnapshotValue& NewValue,
		const double CaptureTime)
	{
		FGoalHistoryEvent Event;
		Event.Key = Key;
		Event.Transition = Transition;
		Event.OldProgress = OldValue.Progress;
		Event.NewProgress = NewValue.Progress;
		Event.bOldFinished = OldValue.bFinished;
		Event.bNewFinished = NewValue.bFinished;
		Event.UpdateTime = NewValue.UpdateTime;
		Event.CaptureTime = CaptureTime;
		return Event;
	}

	static void SetChangeInfo(
		FGoalPanelState& State,
		const FGoalSnapshotKey& Key,
		const EGoalTransition Transition,
		const int64 DeltaProgress,
		const double CaptureTime)
	{
		FGoalChangeInfo& Info = State.ChangeInfo.FindOrAdd(Key);
		Info.Transition = Transition;
		Info.DeltaProgress = DeltaProgress;
		Info.CaptureTime = CaptureTime;
	}

	static void CaptureSnapshot(const FGangstarLocalDebugContext& Context, FGoalPanelState& State)
	{
		const int64 RoleId = Context.PlayerState ? Context.PlayerState->GetRoleId() : 0;
		if (!State.bContextInitialized
			|| State.CapturedWorld.Get() != Context.World
			|| State.CapturedPlayerState.Get() != Context.PlayerState
			|| State.CapturedRoleId != RoleId)
		{
			ResetForContext(State, Context);
		}

		if (State.LastCaptureFrame == GFrameCounter)
		{
			return;
		}
		State.LastCaptureFrame = GFrameCounter;

		if (State.bCapturePaused)
		{
			return;
		}

		const FPlayerProfileData* PlayerProfile = Context.PlayerState
			? Context.PlayerState->GetPlayerProfileIfReady()
			: nullptr;
		if (!PlayerProfile)
		{
			return;
		}

		TMap<FGoalSnapshotKey, FGoalSnapshotValue> NewSnapshot;
		const TMap<int32, TMap<FPlayerProfileData::GoalMapKey, FPlayerProfileData::SystemGoalRecord>>& AllGoalProgress =
			PlayerProfile->GetAllSystemGoalProgress();
		for (const auto& SystemPair : AllGoalProgress)
		{
			for (const auto& GoalPair : SystemPair.Value)
			{
				FGoalSnapshotKey Key;
				Key.SystemType = SystemPair.Key;
				Key.GoalDataId = GoalPair.Key.GoalDataId;
				Key.ConditionIndex = GoalPair.Key.ConditionIndex;

				FGoalSnapshotValue Value;
				Value.bFinished = GoalPair.Value.Finished;
				Value.Progress = GoalPair.Value.Progress;
				Value.UpdateTime = GoalPair.Value.UpdateTime;
				NewSnapshot.Add(Key, Value);
			}
		}

		if (!State.bHasBaseline)
		{
			State.CurrentSnapshot = MoveTemp(NewSnapshot);
			State.bHasBaseline = true;
			return;
		}

		const double CaptureTime = FPlatformTime::Seconds();
		for (const auto& NewPair : NewSnapshot)
		{
			const FGoalSnapshotValue* OldValue = State.CurrentSnapshot.Find(NewPair.Key);
			if (!OldValue)
			{
				const FGoalSnapshotValue EmptyValue;
				AddHistory(State, MakeHistoryEvent(NewPair.Key, EGoalTransition::Accept, EmptyValue, NewPair.Value, CaptureTime));
				SetChangeInfo(State, NewPair.Key, EGoalTransition::Accept, NewPair.Value.Progress, CaptureTime);
				continue;
			}

			const int64 DeltaProgress = NewPair.Value.Progress - OldValue->Progress;
			if (NewPair.Value.bFinished != OldValue->bFinished)
			{
				const EGoalTransition Transition = NewPair.Value.bFinished
					? EGoalTransition::Finish
					: EGoalTransition::Progress;
				AddHistory(State, MakeHistoryEvent(NewPair.Key, Transition, *OldValue, NewPair.Value, CaptureTime));
				SetChangeInfo(State, NewPair.Key, Transition, DeltaProgress, CaptureTime);
			}
			else if (NewPair.Value.Progress != OldValue->Progress || NewPair.Value.UpdateTime != OldValue->UpdateTime)
			{
				AddHistory(State, MakeHistoryEvent(NewPair.Key, EGoalTransition::Progress, *OldValue, NewPair.Value, CaptureTime));
				SetChangeInfo(State, NewPair.Key, EGoalTransition::Progress, DeltaProgress, CaptureTime);
			}
		}

		for (const auto& OldPair : State.CurrentSnapshot)
		{
			if (NewSnapshot.Contains(OldPair.Key))
			{
				continue;
			}

			const FGoalSnapshotValue RemovedValue;
			AddHistory(State, MakeHistoryEvent(OldPair.Key, EGoalTransition::Remove, OldPair.Value, RemovedValue, CaptureTime));
			State.ChangeInfo.Remove(OldPair.Key);
		}

		State.CurrentSnapshot = MoveTemp(NewSnapshot);
	}

	static void DrawFilters(FGoalPanelState& State)
	{
		ImGui::PushItemWidth(220.0f);
		ImGui::InputText("GoalDataId", State.GoalDataIdFilter, UE_ARRAY_COUNT(State.GoalDataIdFilter));
		ImGui::PopItemWidth();

		TArray<int32> AvailableSystemTypes;
		for (const auto& GoalPair : State.CurrentSnapshot)
		{
			AvailableSystemTypes.AddUnique(GoalPair.Key.SystemType);
		}
		if (State.bShowAllTableGoals)
		{
			TMap<FName, const FGoalTable*> AllGoalData;
			if (TryGetAllGoalData(AllGoalData))
			{
				for (const auto& GoalDataPair : AllGoalData)
				{
					if (GoalDataPair.Value)
					{
						AvailableSystemTypes.AddUnique(GoalDataPair.Value->Type.GetValue());
					}
				}
			}
		}
		AvailableSystemTypes.Sort();

		const FString SelectedSystemTypeText = State.SelectedSystemType == AllSystemTypes
			? TEXT("All")
			: FString::Printf(TEXT("%d: %s"), State.SelectedSystemType, *FormatSystemTypeName(State.SelectedSystemType));
		ImGui::PushItemWidth(220.0f);
		if (ImGui::BeginCombo("SystemType", TCHAR_TO_UTF8(*SelectedSystemTypeText)))
		{
			const bool bAllSelected = State.SelectedSystemType == AllSystemTypes;
			if (ImGui::Selectable("All", bAllSelected))
			{
				State.SelectedSystemType = AllSystemTypes;
			}
			if (bAllSelected)
			{
				ImGui::SetItemDefaultFocus();
			}

			for (const int32 SystemType : AvailableSystemTypes)
			{
				const FString Label = FString::Printf(TEXT("%d: %s"), SystemType, *FormatSystemTypeName(SystemType));
				const bool bSelected = State.SelectedSystemType == SystemType;
				if (ImGui::Selectable(TCHAR_TO_UTF8(*Label), bSelected))
				{
					State.SelectedSystemType = SystemType;
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		const char* StateFilterText = "All";
		switch (State.StateFilter)
		{
		case EGoalStateFilter::Active:
			StateFilterText = "Active";
			break;
		case EGoalStateFilter::Finished:
			StateFilterText = "Finished";
			break;
		default:
			break;
		}

		ImGui::PushItemWidth(150.0f);
		if (ImGui::BeginCombo("State", StateFilterText))
		{
			struct FStateFilterOption
			{
				EGoalStateFilter Value;
				const char* Label;
			};
			static const FStateFilterOption Options[] = {
				{ EGoalStateFilter::All, "All" },
				{ EGoalStateFilter::Active, "Active" },
				{ EGoalStateFilter::Finished, "Finished" },
			};

			for (const FStateFilterOption& Option : Options)
			{
				const bool bSelected = State.StateFilter == Option.Value;
				if (ImGui::Selectable(Option.Label, bSelected))
				{
					State.StateFilter = Option.Value;
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		ImGui::Checkbox("Changed Only", &State.bChangedOnly);
		ImGui::SameLine();
		ImGui::Checkbox("Enable GM Actions", &State.bEnableGmActions);
		ImGui::SameLine();
		ImGui::Checkbox("All Table Goals", &State.bShowAllTableGoals);
	}

	static bool PassesFilters(const FGoalPanelState& State, const FGoalDisplayRow& Row)
	{
		if (State.SelectedSystemType != AllSystemTypes && Row.Key.SystemType != State.SelectedSystemType)
		{
			return false;
		}

		if (State.StateFilter == EGoalStateFilter::Active && (!Row.bHasRuntimeRecord || Row.RuntimeValue.bFinished))
		{
			return false;
		}
		if (State.StateFilter == EGoalStateFilter::Finished && (!Row.bHasRuntimeRecord || !Row.RuntimeValue.bFinished))
		{
			return false;
		}

		if (State.bChangedOnly && !State.ChangeInfo.Contains(Row.Key))
		{
			return false;
		}

		if (State.GoalDataIdFilter[0] != '\0')
		{
			const FString FilterText = UTF8_TO_TCHAR(State.GoalDataIdFilter);
			const bool bMatchesGoalId = Row.Key.GoalDataId.ToString().Contains(FilterText, ESearchCase::IgnoreCase);
			const bool bMatchesRemarks = Row.GoalData && Row.GoalData->Remarks.Contains(FilterText, ESearchCase::IgnoreCase);
			if (!bMatchesGoalId && !bMatchesRemarks)
			{
				return false;
			}
		}

		return true;
	}

	static void SendGoalGmAction(const FGangstarLocalDebugContext& Context, const TCHAR* ActionType, const FName GoalDataId)
	{
		if (!Context.GmHelperActor)
		{
			return;
		}

		struct FGmActionParams
		{
			FString ActionType;
			FGmRpcParams Args;
		};

		UFunction* GmActionFunction = Context.GmHelperActor->FindFunction(TEXT("C2S_GmAction"));
		if (!GmActionFunction)
		{
			return;
		}

		FGmActionParams Params;
		Params.ActionType = ActionType;
		Params.Args.Add(GoalDataId.ToString());
		Context.GmHelperActor->ProcessEvent(GmActionFunction, &Params);
	}

	static FString FormatGoalRowState(const FGoalDisplayRow& Row)
	{
		if (!Row.bHasRuntimeRecord)
		{
			return TEXT("NotAccepted");
		}
		if (Row.RuntimeValue.bFinished)
		{
			return TEXT("Finished");
		}
		if (!Row.GoalData)
		{
			return TEXT("MissingTable");
		}
		if (Row.GoalData->TargetCount > 0 && Row.RuntimeValue.Progress >= Row.GoalData->TargetCount)
		{
			return TEXT("Ready");
		}
		return TEXT("Active");
	}

	static FString FormatGoalRowProgress(const FGoalDisplayRow& Row)
	{
		if (!Row.bHasRuntimeRecord)
		{
			return TEXT("--");
		}
		return Row.GoalData
			? FString::Printf(TEXT("%lld / %lld"), Row.RuntimeValue.Progress, static_cast<int64>(Row.GoalData->TargetCount))
			: FString::Printf(TEXT("%lld / ?"), Row.RuntimeValue.Progress);
	}

	static void DrawDisabledSmallButton(const char* Label)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
		ImGui::SmallButton(Label);
		ImGui::PopStyleVar();
	}

	static void DrawGoalActionButton(
		const FGangstarLocalDebugContext& Context,
		const FGoalPanelState& State,
		const FGoalSnapshotKey& Key,
		const char* Label,
		const TCHAR* ActionType,
		const bool bCanRun)
	{
		const bool bEnabled = State.bEnableGmActions && bCanRun && Context.GmHelperActor;
		if (bEnabled)
		{
			if (ImGui::SmallButton(Label))
			{
				SendGoalGmAction(Context, ActionType, Key.GoalDataId);
			}
		}
		else
		{
			DrawDisabledSmallButton(Label);
		}
	}

	static void DrawGoalActions(
		const FGangstarLocalDebugContext& Context,
		const FGoalPanelState& State,
		const FGoalDisplayRow& Row)
	{
		const bool bHasTarget = Row.GoalData && Row.GoalData->TargetCount > 0;
		const bool bCanAccept = !Row.bHasRuntimeRecord;
		const bool bCanFull = Row.bHasRuntimeRecord && bHasTarget && !Row.RuntimeValue.bFinished && Row.RuntimeValue.Progress < Row.GoalData->TargetCount;
		const bool bCanFinish = Row.bHasRuntimeRecord && !Row.RuntimeValue.bFinished;
		const bool bCanGiveUp = Row.bHasRuntimeRecord && !Row.RuntimeValue.bFinished;

		DrawGoalActionButton(Context, State, Row.Key, "Accept", TEXT("Goal_GoalAccept"), bCanAccept);
		ImGui::SameLine();
		DrawGoalActionButton(Context, State, Row.Key, "Full", TEXT("Goal_GoalComplete"), bCanFull);
		ImGui::SameLine();
		DrawGoalActionButton(Context, State, Row.Key, "Finish", TEXT("Goal_GoalFinish"), bCanFinish);
		ImGui::SameLine();
		DrawGoalActionButton(Context, State, Row.Key, "GiveUp", TEXT("Goal_GoalGiveUp"), bCanGiveUp);
	}

	static void BuildDisplayRows(const FGoalPanelState& State, TArray<FGoalDisplayRow>& OutRows)
	{
		OutRows.Reset();

		if (State.bShowAllTableGoals)
		{
			TMap<FName, const FGoalTable*> AllGoalData;
			if (TryGetAllGoalData(AllGoalData))
			{
				for (const auto& GoalDataPair : AllGoalData)
				{
					if (!GoalDataPair.Value)
					{
						continue;
					}

					FGoalDisplayRow& Row = OutRows.AddDefaulted_GetRef();
					Row.Key.SystemType = GoalDataPair.Value->Type.GetValue();
					Row.Key.GoalDataId = GoalDataPair.Key;
					Row.Key.ConditionIndex = 0;
					Row.GoalData = GoalDataPair.Value;
				}
			}
		}

		for (const auto& GoalPair : State.CurrentSnapshot)
		{
			const FGoalTable* GoalData = FindGoalData(GoalPair.Key.SystemType, GoalPair.Key.GoalDataId);
			if (!GoalData)
			{
				GoalData = FindGoalData(GoalPair.Key.GoalDataId);
			}

			FGoalDisplayRow* ExistingRow = nullptr;
			if (State.bShowAllTableGoals)
			{
				ExistingRow = OutRows.FindByPredicate([&GoalPair](const FGoalDisplayRow& Row)
				{
					return Row.Key.GoalDataId == GoalPair.Key.GoalDataId;
				});
			}

			FGoalDisplayRow& Row = ExistingRow ? *ExistingRow : OutRows.AddDefaulted_GetRef();
			Row.Key = GoalPair.Key;
			Row.RuntimeValue = GoalPair.Value;
			Row.GoalData = GoalData;
			Row.bHasRuntimeRecord = true;
		}
	}

	static void DrawCurrentState(const FGangstarLocalDebugContext& Context, FGoalPanelState& State)
	{
		TArray<FGoalDisplayRow> DisplayRows;
		BuildDisplayRows(State, DisplayRows);

		ImGui::Text("Goal Count: %d runtime / %d display", State.CurrentSnapshot.Num(), DisplayRows.Num());
		if (State.bCapturePaused)
		{
			ImGui::SameLine();
			ImGui::TextUnformatted("(capture paused; runtime table is frozen)");
		}

		ImGui::Columns(6, "GoalCurrentStateColumns", true);
		ImGui::TextUnformatted("Type"); ImGui::NextColumn();
		ImGui::TextUnformatted("SystemName"); ImGui::NextColumn();
		ImGui::TextUnformatted("GoalDataId"); ImGui::NextColumn();
		ImGui::TextUnformatted("State"); ImGui::NextColumn();
		ImGui::TextUnformatted("Progress"); ImGui::NextColumn();
		ImGui::TextUnformatted("Actions"); ImGui::NextColumn();
		ImGui::Separator();

		for (const FGoalDisplayRow& Row : DisplayRows)
		{
			if (!PassesFilters(State, Row))
			{
				continue;
			}

			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Row.Key.SystemType)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FormatSystemTypeName(Row.Key.SystemType)); ImGui::NextColumn();

			const FString GoalDataIdText = GangstarLocalDebugHelpers::FormatName(Row.Key.GoalDataId);
			const FTCHARToUTF8 GoalDataIdUtf8(*GoalDataIdText);
			ImGui::PushID(TCHAR_TO_UTF8(*FString::Printf(TEXT("%d_%s_%d_%d"), Row.Key.SystemType, *GoalDataIdText, Row.Key.ConditionIndex, Row.bHasRuntimeRecord ? 1 : 0)));
			const bool bSelected = State.bHasSelectedGoal && State.SelectedGoalKey == Row.Key;
			if (ImGui::Selectable(GoalDataIdUtf8.Get(), bSelected))
			{
				State.SelectedGoalKey = Row.Key;
				State.bHasSelectedGoal = true;
				ImGui::SetClipboardText(GoalDataIdUtf8.Get());
			}
			ImGui::NextColumn();

			GangstarLocalDebugHelpers::DrawCopyableText(FormatGoalRowState(Row)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FormatGoalRowProgress(Row)); ImGui::NextColumn();
			DrawGoalActions(Context, State, Row); ImGui::NextColumn();
			ImGui::PopID();
		}
		ImGui::Columns(1);
	}

	static void DrawSelectedGoalDetail(FGoalPanelState& State)
	{
		ImGui::TextUnformatted("Selected Goal Detail");
		if (!State.bHasSelectedGoal)
		{
			ImGui::TextUnformatted("Click a GoalDataId row above to inspect details.");
			return;
		}

		const FGoalSnapshotValue* SelectedValue = State.CurrentSnapshot.Find(State.SelectedGoalKey);
		const FGoalTable* GoalData = FindGoalData(State.SelectedGoalKey.SystemType, State.SelectedGoalKey.GoalDataId);
		if (!GoalData)
		{
			GoalData = FindGoalData(State.SelectedGoalKey.GoalDataId);
		}
		if (!SelectedValue && !GoalData)
		{
			ImGui::TextUnformatted("Selected goal is no longer in the current runtime snapshot and no table row was found.");
			if (ImGui::Button("Clear Selection"))
			{
				State.bHasSelectedGoal = false;
				State.SelectedGoalKey = FGoalSnapshotKey();
			}
			return;
		}

		const int64 TargetCount = GoalData ? GoalData->TargetCount : 0;
		const bool bProgressReached = SelectedValue && GoalData && SelectedValue->Progress >= TargetCount;
		const FString RuntimeStateText = SelectedValue
			? (SelectedValue->bFinished ? TEXT("Finished") : (bProgressReached ? TEXT("CompletePendingFinish") : TEXT("Active")))
			: TEXT("NotAccepted");

		ImGui::Columns(2, "SelectedGoalDetailColumns", false);
		ImGui::TextUnformatted("GoalDataId"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(State.SelectedGoalKey.GoalDataId)); ImGui::NextColumn();

		ImGui::TextUnformatted("Runtime SystemType"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d: %s"), State.SelectedGoalKey.SystemType, *FormatSystemTypeName(State.SelectedGoalKey.SystemType))); ImGui::NextColumn();

		ImGui::TextUnformatted("ConditionIndex"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), State.SelectedGoalKey.ConditionIndex)); ImGui::NextColumn();

		ImGui::TextUnformatted("Runtime State"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(RuntimeStateText); ImGui::NextColumn();

		const FString ProgressText = SelectedValue
			? (GoalData
				? FString::Printf(TEXT("%lld / %lld"), SelectedValue->Progress, TargetCount)
				: FString::Printf(TEXT("%lld / <missing table>"), SelectedValue->Progress))
			: (GoalData ? FString::Printf(TEXT("-- / %lld"), TargetCount) : FString(TEXT("-- / <missing table>")));
		const FString UpdateTimeText = SelectedValue
			? GangstarLocalDebugHelpers::FormatUnixTime(SelectedValue->UpdateTime)
			: FString(TEXT("--"));

		ImGui::TextUnformatted("Progress / Target"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(ProgressText); ImGui::NextColumn();

		ImGui::TextUnformatted("UpdateTime"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(UpdateTimeText); ImGui::NextColumn();

		ImGui::TextUnformatted("Table Status"); ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(GoalData ? TEXT("Found") : TEXT("Missing FGoalTable row")); ImGui::NextColumn();

		if (GoalData)
		{
			const int32 ConfiguredSystemType = GoalData->Type.GetValue();
			ImGui::TextUnformatted("Configured Type"); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d: %s"), ConfiguredSystemType, *FormatSystemTypeName(ConfiguredSystemType))); ImGui::NextColumn();

			ImGui::TextUnformatted("Type Match"); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatBool(ConfiguredSystemType == State.SelectedGoalKey.SystemType)); ImGui::NextColumn();

			ImGui::TextUnformatted("Remarks"); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GoalData->Remarks); ImGui::NextColumn();
		}

		if (const FGoalChangeInfo* Change = State.ChangeInfo.Find(State.SelectedGoalKey))
		{
			ImGui::TextUnformatted("Recent Change"); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(
				TEXT("%s (%+lld), %.2fs ago"),
				GetTransitionText(Change->Transition),
				Change->DeltaProgress,
				FMath::Max(0.0, FPlatformTime::Seconds() - Change->CaptureTime))); ImGui::NextColumn();
		}

		ImGui::Columns(1);
	}

	static void DrawHistory(FGoalPanelState& State)
	{
		ImGui::Checkbox("Pause Capture", &State.bCapturePaused);
		ImGui::SameLine();
		if (ImGui::Button("Clear History"))
		{
			State.History.Reset();
			State.HistoryStart = 0;
		}
		ImGui::SameLine();
		ImGui::Text("History: %d / %d", State.History.Num(), MaxHistoryCount);

		ImGui::BeginChild("GoalHistory", ImVec2(0.0f, 240.0f), true);
		ImGui::Columns(9, "GoalHistoryColumns", true);
		ImGui::TextUnformatted("Age"); ImGui::NextColumn();
		ImGui::TextUnformatted("Transition"); ImGui::NextColumn();
		ImGui::TextUnformatted("SystemType"); ImGui::NextColumn();
		ImGui::TextUnformatted("SystemName"); ImGui::NextColumn();
		ImGui::TextUnformatted("GoalDataId"); ImGui::NextColumn();
		ImGui::TextUnformatted("ConditionIndex"); ImGui::NextColumn();
		ImGui::TextUnformatted("Progress"); ImGui::NextColumn();
		ImGui::TextUnformatted("State"); ImGui::NextColumn();
		ImGui::TextUnformatted("UpdateTime"); ImGui::NextColumn();
		ImGui::Separator();

		const double CurrentTime = FPlatformTime::Seconds();
		for (int32 HistoryIndex = 0; HistoryIndex < State.History.Num(); ++HistoryIndex)
		{
			const FGoalHistoryEvent& Event = GetHistoryNewestFirst(State, HistoryIndex);
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%.2fs"), FMath::Max(0.0, CurrentTime - Event.CaptureTime))); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GetTransitionText(Event.Transition)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Event.Key.SystemType)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FormatSystemTypeName(Event.Key.SystemType)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Event.Key.GoalDataId)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Event.Key.ConditionIndex)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%lld -> %lld"), Event.OldProgress, Event.NewProgress)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(
				TEXT("%s -> %s"),
				GetGoalStateText(Event.bOldFinished),
				GetGoalStateText(Event.bNewFinished))); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatUnixTime(Event.UpdateTime)); ImGui::NextColumn();
		}
		ImGui::Columns(1);
		ImGui::EndChild();
	}
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugGoalPanel::Capture(const FGangstarLocalDebugContext& Context)
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	GangstarLocalDebugGoalPanel::CaptureSnapshot(Context, GangstarLocalDebugGoalPanel::GetState());
#else
	(void)Context;
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugGoalPanel::Draw(const FGangstarLocalDebugContext& Context)
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	GangstarLocalDebugGoalPanel::FGoalPanelState& State = GangstarLocalDebugGoalPanel::GetState();

	ImGui::TextUnformatted("Source: ABasePlayerState -> GetPlayerProfileIfReady() -> GetAllSystemGoalProgress()");
	if (!Context.PlayerState)
	{
		ImGui::TextUnformatted("ABasePlayerState not found.");
	}
	else if (!Context.PlayerState->GetPlayerProfileIfReady())
	{
		ImGui::TextUnformatted("Player profile is not ready.");
	}
	ImGui::Separator();

	GangstarLocalDebugGoalPanel::DrawFilters(State);
	ImGui::Separator();
	ImGui::BeginChild("GoalCurrentStateScroll", ImVec2(0.0f, GangstarLocalDebugGoalPanel::CurrentGoalListHeight), true);
	GangstarLocalDebugGoalPanel::DrawCurrentState(Context, State);
	ImGui::EndChild();
	ImGui::Separator();
	GangstarLocalDebugGoalPanel::DrawSelectedGoalDetail(State);
	ImGui::Separator();
	GangstarLocalDebugGoalPanel::DrawHistory(State);
#else
	(void)Context;
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}
