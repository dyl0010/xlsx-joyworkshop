#include "Panels/GangstarLocalDebugProjectMapPanel.h"

#include "GangstarLocalDebugContext.h"
#include "GangstarLocalDebugHelpers.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
#include "Characters/BasePlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GM/GmHelperActor.h"
#include <imgui.h>
#endif // WITH_GANGSTAR_LOCAL_IMGUI

namespace GangstarLocalDebugProjectMapPanel
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	static const TCHAR* LexWorldType(EWorldType::Type WorldType)
	{
		switch (WorldType)
		{
		case EWorldType::None:
			return TEXT("None");
		case EWorldType::Game:
			return TEXT("Game");
		case EWorldType::Editor:
			return TEXT("Editor");
		case EWorldType::PIE:
			return TEXT("PIE");
		case EWorldType::EditorPreview:
			return TEXT("EditorPreview");
		case EWorldType::GamePreview:
			return TEXT("GamePreview");
		case EWorldType::GameRPC:
			return TEXT("GameRPC");
		case EWorldType::Inactive:
			return TEXT("Inactive");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* LexNetMode(ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Standalone:
			return TEXT("Standalone");
		case NM_DedicatedServer:
			return TEXT("DedicatedServer");
		case NM_ListenServer:
			return TEXT("ListenServer");
		case NM_Client:
			return TEXT("Client");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* LexWorkStateType(EWorkStateType WorkStateType)
	{
		switch (WorkStateType)
		{
			case EWorkStateType::EWST_None:
				return TEXT("None");
			case EWorkStateType::EWST_GangBattleWork:
				return TEXT("GangBattleWork");
			case EWorkStateType::EWST_HappyLabWork:
				return TEXT("HappyLabWork");
			case EWorkStateType::EWST_ScrapCarWork:
				return TEXT("ScrapCarWork");
			default:
				return TEXT("Unknown");
		}
	}

	static const TCHAR* LexWorkStateChangeReason(EWorkStateChangeReason Reason)
	{
		switch (Reason)
		{
		case EWorkStateChangeReason::None:
			return TEXT("None");
		case EWorkStateChangeReason::NormalStart:
			return TEXT("NormalStart");
		case EWorkStateChangeReason::NormalStop:
			return TEXT("NormalStop");
		case EWorkStateChangeReason::ForceStart_EnterArea:
			return TEXT("ForceStart_EnterArea");
		case EWorkStateChangeReason::ForceStop_PhaseChange:
			return TEXT("ForceStop_PhaseChange");
		case EWorkStateChangeReason::ForceStop_ActivityEnd:
			return TEXT("ForceStop_ActivityEnd");
		case EWorkStateChangeReason::ForceStop_HappyLabUnSafe:
			return TEXT("ForceStop_HappyLabUnSafe");
		case EWorkStateChangeReason::ForceStop_SessionChange:
			return TEXT("ForceStop_SessionChange");
		case EWorkStateChangeReason::ForceStop_Logout:
			return TEXT("ForceStop_Logout");
		case EWorkStateChangeReason::ForceStop_EnterDungeon:
			return TEXT("ForceStop_EnterDungeon");
		default:
			return TEXT("Unknown");
		}
	}

	static FString FormatInt64(const int64 Value)
	{
		return FString::Printf(TEXT("%lld"), Value);
	}

	static FString FormatObjectName(const UObject* Object)
	{
		return Object ? Object->GetName() : TEXT("None");
	}

	static FString FormatObjectPath(const UObject* Object)
	{
		return Object ? Object->GetPathName() : TEXT("None");
	}

	static void DrawCopyableRow(const char* Label, const FString& Value)
	{
	        ImGui::TextUnformatted(Label);
	        ImGui::NextColumn();
	        GangstarLocalDebugHelpers::DrawCopyableText(Value);
	        ImGui::NextColumn();
	}

	static void DrawComponentRow(const char* Label, const UObject* Component)
	{
		ImGui::TextUnformatted(Label);
		ImGui::NextColumn();
		ImGui::TextColored(Component ? ImVec4(0.20f, 0.90f, 0.20f, 1.0f) : ImVec4(1.0f, 0.25f, 0.25f, 1.0f), Component ? "OK" : "Missing");
		ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(FormatObjectName(Component));
		ImGui::NextColumn();
	}

	static void BeginComponentColumns(const char* Id)
	{
		ImGui::Columns(3, Id, false);
		ImGui::SetColumnWidth(0, 230.0f);
		ImGui::SetColumnWidth(1, 80.0f);
	}

	static void DrawSubsystemRow(const char* Scope, const UObject* Subsystem)
	{
		ImGui::TextUnformatted(Scope);
		ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(Subsystem && Subsystem->GetClass() ? Subsystem->GetClass()->GetName() : TEXT("None"));
		ImGui::NextColumn();
		GangstarLocalDebugHelpers::DrawCopyableText(FormatObjectName(Subsystem));
		ImGui::NextColumn();
	}

	static void BeginSubsystemColumns(const char* Id)
	{
		ImGui::Columns(3, Id, false);
		ImGui::SetColumnWidth(0, 150.0f);
		ImGui::SetColumnWidth(1, 310.0f);
	}

	static int32 GetLocalPlayerCount(const UGameInstance* GameInstance)
	{
		return GameInstance ? GameInstance->GetLocalPlayers().Num() : 0;
	}

	static FString GetViewportName(const UGameInstance* GameInstance)
	{
		const UGameViewportClient* GameViewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
		return FormatObjectName(GameViewport);
	}

	static FString GetRoleIdText(const ABasePlayerState* PlayerState)
	{
		return PlayerState ? FString::Printf(TEXT("%lld"), PlayerState->GetRoleId()) : TEXT("None");
	}

	static void BeginInfoColumns(const char* Id)
	{
		ImGui::Columns(2, Id, false);
		ImGui::SetColumnWidth(0, 170.0f);
	}
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugProjectMapPanel::Draw(const FGangstarLocalDebugContext& Context)
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	if (!Context.World)
	{
		ImGui::TextUnformatted("No active game or PIE world was found.");
		return;
	}

	ImGui::TextUnformatted("World / Net / PIE");
	ImGui::Separator();

	GangstarLocalDebugProjectMapPanel::BeginInfoColumns("GangstarLocalDebugWorldContextColumns");
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("World", GangstarLocalDebugProjectMapPanel::FormatObjectName(Context.World));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("World Path", GangstarLocalDebugProjectMapPanel::FormatObjectPath(Context.World));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("World Type", GangstarLocalDebugProjectMapPanel::LexWorldType(Context.World->WorldType));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("Net Mode", GangstarLocalDebugProjectMapPanel::LexNetMode(Context.World->GetNetMode()));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("Map", Context.World->GetMapName());
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("Package", Context.World->GetOutermost() ? Context.World->GetOutermost()->GetName() : TEXT("None"));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("GameInstance", GangstarLocalDebugProjectMapPanel::FormatObjectName(Context.GameInstance));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("GameViewport", GangstarLocalDebugProjectMapPanel::GetViewportName(Context.GameInstance));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("GameMode", GangstarLocalDebugProjectMapPanel::FormatObjectName(Context.GameMode));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("GameState", GangstarLocalDebugProjectMapPanel::FormatObjectName(Context.GameState));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("Local Players", FString::FromInt(GangstarLocalDebugProjectMapPanel::GetLocalPlayerCount(Context.GameInstance)));
	ImGui::Columns(1);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextUnformatted("Player Object Chain");
	ImGui::Separator();

	GangstarLocalDebugProjectMapPanel::BeginInfoColumns("GangstarLocalDebugPlayerChainColumns");
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("PlayerController", GangstarLocalDebugProjectMapPanel::FormatObjectName(Context.PlayerController));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("PlayerController Path", GangstarLocalDebugProjectMapPanel::FormatObjectPath(Context.PlayerController));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("Pawn", GangstarLocalDebugProjectMapPanel::FormatObjectName(Context.Pawn));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("Pawn Path", GangstarLocalDebugProjectMapPanel::FormatObjectPath(Context.Pawn));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("Character", GangstarLocalDebugProjectMapPanel::FormatObjectName(Context.Character));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("PlayerState", GangstarLocalDebugProjectMapPanel::FormatObjectName(Context.PlayerState));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("PlayerState Path", GangstarLocalDebugProjectMapPanel::FormatObjectPath(Context.PlayerState));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("RoleId", GangstarLocalDebugProjectMapPanel::GetRoleIdText(Context.PlayerState));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("GmHelperActor", GangstarLocalDebugProjectMapPanel::FormatObjectName(Context.GmHelperActor));
	GangstarLocalDebugProjectMapPanel::DrawCopyableRow("GmHelperActor Path", GangstarLocalDebugProjectMapPanel::FormatObjectPath(Context.GmHelperActor));
	ImGui::Columns(1);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextUnformatted("Player Work State");
	ImGui::Separator();

	GangstarLocalDebugProjectMapPanel::BeginInfoColumns("GangstarLocalDebugPlayerWorkStateColumns");
	if (Context.PlayerState)
	{
		const FWorkStateInfo& WorkStateInfo = Context.PlayerState->GetCurrentWorkStateInfo();
		GangstarLocalDebugProjectMapPanel::DrawCopyableRow("IsWorking", WorkStateInfo.IsValid() ? TEXT("True") : TEXT("False"));
		GangstarLocalDebugProjectMapPanel::DrawCopyableRow("WorkStateID", WorkStateInfo.WorkStateID.ToString());
		GangstarLocalDebugProjectMapPanel::DrawCopyableRow("WorkStateType", GangstarLocalDebugProjectMapPanel::LexWorkStateType(WorkStateInfo.WorkStateType));
		GangstarLocalDebugProjectMapPanel::DrawCopyableRow("ChangeReason", GangstarLocalDebugProjectMapPanel::LexWorkStateChangeReason(WorkStateInfo.ChangeReason));
		GangstarLocalDebugProjectMapPanel::DrawCopyableRow("StartTimeStamp", GangstarLocalDebugProjectMapPanel::FormatInt64(WorkStateInfo.WorkStartTimeStamp));
		GangstarLocalDebugProjectMapPanel::DrawCopyableRow("StopTimeStamp", GangstarLocalDebugProjectMapPanel::FormatInt64(WorkStateInfo.WorkStopTimeStamp));
		GangstarLocalDebugProjectMapPanel::DrawCopyableRow("CurWorkGangBattleID", Context.PlayerState->GetCurrentGangBattleID().ToString());
	}
	else
	{
		GangstarLocalDebugProjectMapPanel::DrawCopyableRow("PlayerState", TEXT("None"));
	}
	ImGui::Columns(1);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextUnformatted("Important Components");
	ImGui::Separator();

	GangstarLocalDebugProjectMapPanel::BeginComponentColumns("GangstarLocalDebugImportantComponentColumns");
	GangstarLocalDebugProjectMapPanel::DrawComponentRow("PlayerState / AbilitySystem", Context.AbilitySystemComponent);
	GangstarLocalDebugProjectMapPanel::DrawComponentRow("PlayerState / LightQuest", Context.LightQuestComponent);
	GangstarLocalDebugProjectMapPanel::DrawComponentRow("PlayerState / ItemContainerManager", Context.ItemContainerManager);
	GangstarLocalDebugProjectMapPanel::DrawComponentRow("PlayerState / OwnerOnlyReplicator", Context.OwnerOnlyItemContainerReplicator);
	GangstarLocalDebugProjectMapPanel::DrawComponentRow("PlayerState / Procurement", Context.ProcurementComponent);
	GangstarLocalDebugProjectMapPanel::DrawComponentRow("World / DeliveryPackageActor", Context.CurrentHappyLabFactory);
	GangstarLocalDebugProjectMapPanel::DrawComponentRow("DeliveryPackageActor / ItemReplicator", Context.FactoryItemContainerReplicator);
	ImGui::Columns(1);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextUnformatted("Runtime Subsystems / Managers");
	ImGui::Separator();

	GangstarLocalDebugProjectMapPanel::BeginSubsystemColumns("GangstarLocalDebugRuntimeSubsystemColumns");
	if (Context.GameInstance)
	{
		for (UGameInstanceSubsystem* Subsystem : Context.GameInstance->GetSubsystemArray<UGameInstanceSubsystem>())
		{
			GangstarLocalDebugProjectMapPanel::DrawSubsystemRow("GameInstance", Subsystem);
		}
	}

	if (Context.World)
	{
		for (UWorldSubsystem* Subsystem : Context.World->GetSubsystemArray<UWorldSubsystem>())
		{
			GangstarLocalDebugProjectMapPanel::DrawSubsystemRow("World", Subsystem);
		}
	}
	ImGui::Columns(1);

	ImGui::Spacing();
	ImGui::TextUnformatted("Tip: values in the right column can be clicked to copy.");
#else
	(void)Context;
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}
