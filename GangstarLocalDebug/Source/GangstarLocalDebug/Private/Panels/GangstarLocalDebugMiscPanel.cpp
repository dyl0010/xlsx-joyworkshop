#include "Panels/GangstarLocalDebugMiscPanel.h"

#include "GangstarLocalDebugContext.h"
#include "GangstarLocalDebugHelpers.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
#include "ActivityManager/ActivityManagerDef.h"
#include "Characters/BasePlayerController.h"
#include "Characters/BasePlayerState.h"
#include "CoreGlobals.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GM/GmHelperActor.h"
#include "GM/GmRpcParams.h"
#include "GSTMGameFramework/Table/TableManager.h"
#include "ImGuiModule.h"
#include "ImGuiModuleProperties.h"
#include <imgui.h>
#endif // WITH_GANGSTAR_LOCAL_IMGUI

namespace GangstarLocalDebugMiscPanel
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	struct FWorkStateOption
	{
		FName RowName;
		FString Label;
	};

	static bool IsStartWorkSupportedStateType(EWorkStateType StateType)
	{
		return StateType == EWorkStateType::EWST_GangBattleWork
			|| StateType == EWorkStateType::EWST_HappyLabWork
			|| StateType == EWorkStateType::EWST_ScrapCarWork;
	}

	static FString FormatTeleportLocation(const FVector& Location)
	{
		return FString::Printf(TEXT("(X=%.2f,Y=%.2f,Z=%.2f)"), Location.X, Location.Y, Location.Z);
	}

	static bool TryParseTeleportLocation(const FString& Input, FVector& OutLocation)
	{
		FString FixedInput = Input.TrimStartAndEnd();
		FixedInput.RemoveFromStart(TEXT("("));
		FixedInput.RemoveFromEnd(TEXT(")"));

		TArray<FString> Parts;
		FixedInput.ParseIntoArray(Parts, TEXT(","), true);
		if (Parts.Num() != 3)
		{
			return false;
		}

		bool bHasX = false;
		bool bHasY = false;
		bool bHasZ = false;
		for (FString& Part : Parts)
		{
			Part.TrimStartAndEndInline();
			FString Key;
			FString Value;
			if (!Part.Split(TEXT("="), &Key, &Value))
			{
				return false;
			}

			Key.TrimStartAndEndInline();
			Value.TrimStartAndEndInline();
			if (Key.Equals(TEXT("X"), ESearchCase::IgnoreCase))
			{
				OutLocation.X = FCString::Atof(*Value);
				bHasX = true;
			}
			else if (Key.Equals(TEXT("Y"), ESearchCase::IgnoreCase))
			{
				OutLocation.Y = FCString::Atof(*Value);
				bHasY = true;
			}
			else if (Key.Equals(TEXT("Z"), ESearchCase::IgnoreCase))
			{
				OutLocation.Z = FCString::Atof(*Value);
				bHasZ = true;
			}
		}

		return bHasX && bHasY && bHasZ;
	}

	static void BuildWorkStateOptions(TArray<FWorkStateOption>& OutOptions)
	{
		OutOptions.Reset();

		const UTableManager* TableManager = UTableManager::Get();
		const UDataTable* WorkStateTable = TableManager ? TableManager->GetDataTable<FWorkStateConfig>() : nullptr;
		if (!WorkStateTable)
		{
			return;
		}

		for (const auto& RowPair : WorkStateTable->GetRowMap())
		{
			const FWorkStateConfig* Config = WorkStateTable->FindRow<FWorkStateConfig>(RowPair.Key, TEXT(""));
			if (!Config || !IsStartWorkSupportedStateType(Config->StateType))
			{
				continue;
			}

			FWorkStateOption& Option = OutOptions.AddDefaulted_GetRef();
			Option.RowName = RowPair.Key;
			Option.Label = GangstarLocalDebugHelpers::FormatName(RowPair.Key);
		}
	}

	static void SendGmRpcToServer(AGmHelperActor& GmHelperActor, const FString& ActionType, const FGmRpcParams& Args)
	{
		struct FGmActionParams
		{
			FString ActionType;
			FGmRpcParams Args;
		};

		UFunction* GmActionFunction = GmHelperActor.FindFunction(TEXT("C2S_GmAction"));
		if (!GmActionFunction)
		{
			return;
		}

		FGmActionParams Params;
		Params.ActionType = ActionType;
		Params.Args = Args;
		GmHelperActor.ProcessEvent(GmActionFunction, &Params);
	}

	static bool CheatTeleportToLocation(APlayerController& PlayerController, const FVector& Location)
	{
		UClass* PlayerCheatClass = LoadObject<UClass>(nullptr, TEXT("/Game/Globals/BC_PlayerCheat.BC_PlayerCheat_C"));
		if (!PlayerCheatClass)
		{
			return false;
		}

		TArray<UActorComponent*> PlayerCheatComponents;
		PlayerController.GetComponents(PlayerCheatClass, PlayerCheatComponents);
		if (PlayerCheatComponents.Num() == 0 || !PlayerCheatComponents[0])
		{
			return false;
		}

		UFunction* TeleportFunction = PlayerCheatComponents[0]->FindFunction(TEXT("Cheat_TeleportToLocation"));
		if (!TeleportFunction)
		{
			return false;
		}

		struct FCheatTeleportToLocationParams
		{
			FVector NewLocation;
			bool KeepLocation;
		};

		FCheatTeleportToLocationParams Params;
		Params.NewLocation = Location;
		Params.KeepLocation = false;
		PlayerCheatComponents[0]->ProcessEvent(TeleportFunction, &Params);
		return true;
	}

	static void DrawCopyableValue(const char* Label, const FString& Value)
	{
		ImGui::TextUnformatted(Label);
		ImGui::SameLine();
		GangstarLocalDebugHelpers::DrawCopyableText(Value);
	}

	template <typename ReturnType>
	static bool CallRealPropertyFunction(UObject* WorldContextObject, FName FunctionName, ReturnType& OutResult)
	{
		UClass* FunctionLibraryClass = FindObject<UClass>(nullptr, TEXT("/Script/Gangstar.RealPropertyFunctionLibrary"));
		if (!FunctionLibraryClass)
		{
			FunctionLibraryClass = LoadObject<UClass>(nullptr, TEXT("/Script/Gangstar.RealPropertyFunctionLibrary"));
		}
		if (!FunctionLibraryClass)
		{
			return false;
		}

		UFunction* Function = FunctionLibraryClass->FindFunctionByName(FunctionName);
		if (!Function)
		{
			return false;
		}

		struct FRealPropertyFunctionParams
		{
			UObject* WorldContextObject;
			ReturnType ReturnValue;
		};

		FRealPropertyFunctionParams Params;
		Params.WorldContextObject = WorldContextObject;
		Params.ReturnValue = ReturnType();
		FunctionLibraryClass->GetDefaultObject()->ProcessEvent(Function, &Params);
		OutResult = Params.ReturnValue;
		return true;
	}

	static void DrawPlayerTextInfo(const FGangstarLocalDebugContext& Context, const FVector& CurrentLocation, const FVector& TransferTargetPos)
	{
		ImGui::TextUnformatted("Player");
		ImGui::Separator();
		ImGui::Text("World: %s", Context.World ? TCHAR_TO_UTF8(*Context.World->GetName()) : "None");
		ImGui::Text("PlayerController: %s", Context.PlayerController ? TCHAR_TO_UTF8(*Context.PlayerController->GetName()) : "None");
		ImGui::Text("PlayerState: %s", Context.PlayerState ? TCHAR_TO_UTF8(*Context.PlayerState->GetName()) : "None");
		ImGui::Text("GmHelperActor: %s", Context.GmHelperActor ? TCHAR_TO_UTF8(*Context.GmHelperActor->GetName()) : "None");

		DrawCopyableValue("CurrentLocation:", FormatTeleportLocation(CurrentLocation));
		DrawCopyableValue("MapMarker:", FormatTeleportLocation(TransferTargetPos));

		EInteriorKeyType CurInteriorKeyType = EInteriorKeyType::none;
		FName CurrentInteriorKey = NAME_None;
		FName CurrentRealPropertyKey = NAME_None;
		CallRealPropertyFunction(Context.World, TEXT("GetCurrentRealPropertyInteriorType"), CurInteriorKeyType);
		CallRealPropertyFunction(Context.World, TEXT("GetCurrentRealPropertyInteriorKey"), CurrentInteriorKey);
		CallRealPropertyFunction(Context.World, TEXT("GetCurrentRealPropertyTableKey"), CurrentRealPropertyKey);
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("CurInteriorKeyType: %s"), *GangstarLocalDebugHelpers::FormatEnum(CurInteriorKeyType)));
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("CurrentInteriorKey: %s"), *GangstarLocalDebugHelpers::FormatName(CurrentInteriorKey)));
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("CurrentRealPropertyKey: %s"), *GangstarLocalDebugHelpers::FormatName(CurrentRealPropertyKey)));
	}

	static void DrawPlayerActions(const FGangstarLocalDebugContext& Context, const FVector& CurrentLocation, const FVector& TransferTargetPos)
	{
		static char TeleportLocationInput[128] = "";
		static FString TeleportStatus;

		ImGui::Separator();
		ImGui::TextUnformatted("Actions");

		const FString CurrentLocationText = FormatTeleportLocation(CurrentLocation);
		ImGui::PushItemWidth(270.0f);
		ImGui::InputText("Teleport Location", TeleportLocationInput, UE_ARRAY_COUNT(TeleportLocationInput));
		ImGui::PopItemWidth();
		if (ImGui::Button("Fill Current"))
		{
			const FTCHARToUTF8 CurrentLocationUtf8(*CurrentLocationText);
			FCStringAnsi::Strncpy(TeleportLocationInput, CurrentLocationUtf8.Get(), UE_ARRAY_COUNT(TeleportLocationInput));
		}
		ImGui::SameLine();

		FVector ParsedLocation = FVector::ZeroVector;
		const FString TeleportLocationText = UTF8_TO_TCHAR(TeleportLocationInput);
		const bool bParsedLocation = TryParseTeleportLocation(TeleportLocationText, ParsedLocation);
		if (ImGui::Button("Teleport To Input"))
		{
			if (!Context.PlayerController)
			{
				TeleportStatus = TEXT("Teleport failed: PlayerController not found.");
			}
			else if (!bParsedLocation)
			{
				TeleportStatus = TEXT("Teleport failed: invalid location format.");
			}
			else
			{
				TeleportStatus = CheatTeleportToLocation(*Context.PlayerController, ParsedLocation) ? TEXT("Teleport requested.") : TEXT("Teleport failed: BC_PlayerCheat not found.");
			}
		}
		if (ImGui::Button("Teleport To Map Marker"))
		{
			if (!Context.PlayerController)
			{
				TeleportStatus = TEXT("Teleport failed: PlayerController not found.");
			}
			else if (!Context.PlayerState)
			{
				TeleportStatus = TEXT("Teleport failed: PlayerState not found.");
			}
			else
			{
				TeleportStatus = CheatTeleportToLocation(*Context.PlayerController, TransferTargetPos) ? TEXT("Teleport to map marker requested.") : TEXT("Teleport failed: BC_PlayerCheat not found.");
			}
		}

		if (!TeleportStatus.IsEmpty())
		{
			const FTCHARToUTF8 TeleportStatusUtf8(*TeleportStatus);
			ImGui::TextUnformatted(TeleportStatusUtf8.Get());
		}
	}

	static void DrawStartWorkGmControls(const FGangstarLocalDebugContext& Context)
	{
		static FName SelectedWorkStateID;
		static int32 SelectedReasonIndex = static_cast<int32>(EWorkStateChangeReason::NormalStart);
		static const char* ReasonLabels[] =
		{
			"None",
			"NormalStart",
			"NormalStop",
			"ForceStart_EnterArea",
			"ForceStop_PhaseChange",
			"ForceStop_ActivityEnd",
			"ForceStop_SessionChange",
			"ForceStop_Logout",
			"ForceStop_EnterDungeon",
		};

		TArray<FWorkStateOption> WorkStateOptions;
		BuildWorkStateOptions(WorkStateOptions);

		if (!SelectedWorkStateID.IsNone() && !WorkStateOptions.ContainsByPredicate([](const FWorkStateOption& Option) { return Option.RowName == SelectedWorkStateID; }))
		{
			SelectedWorkStateID = NAME_None;
		}
		if (SelectedWorkStateID.IsNone() && WorkStateOptions.Num() > 0)
		{
			const int32 DefaultWorkStateIndex = WorkStateOptions.IsValidIndex(2) ? 2 : 0;
			SelectedWorkStateID = WorkStateOptions[DefaultWorkStateIndex].RowName;
		}

		const FWorkStateOption* SelectedOption = WorkStateOptions.FindByPredicate([](const FWorkStateOption& Option) { return Option.RowName == SelectedWorkStateID; });
		const FString SelectedLabel = SelectedOption ? SelectedOption->Label : TEXT("None");
		const FTCHARToUTF8 SelectedLabelUtf8(*SelectedLabel);
		ImGui::PushItemWidth(220.0f);
		if (ImGui::BeginCombo("WorkState", SelectedLabelUtf8.Get()))
		{
			for (const FWorkStateOption& Option : WorkStateOptions)
			{
				const bool bSelected = Option.RowName == SelectedWorkStateID;
				const FTCHARToUTF8 OptionLabelUtf8(*Option.Label);
				if (ImGui::Selectable(OptionLabelUtf8.Get(), bSelected))
				{
					SelectedWorkStateID = Option.RowName;
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
		ImGui::PushItemWidth(180.0f);
		ImGui::Combo("Reason", &SelectedReasonIndex, ReasonLabels, UE_ARRAY_COUNT(ReasonLabels));
		ImGui::PopItemWidth();

		const bool bCanSendStartWork = Context.GmHelperActor && !SelectedWorkStateID.IsNone();
		if (ImGui::Button("Send StartWork GM") && bCanSendStartWork)
		{
			FGmRpcParams Args;
			Args.Add<FName>(SelectedWorkStateID);
			Args.Add<int32>(SelectedReasonIndex);
			GangstarLocalDebugMiscPanel::SendGmRpcToServer(*Context.GmHelperActor, TEXT("GamePlayCampaign_StartWork"), Args);
		}

		if (!bCanSendStartWork)
		{
			ImGui::TextUnformatted(WorkStateOptions.Num() == 0 ? "No supported WorkStateConfig rows found." : "Select a valid WorkState and make sure GmHelperActor exists.");
		}
	}

	static void DrawPlayerSection(const FGangstarLocalDebugContext& Context)
	{
		APawn* ControlledPawn = Context.PlayerController ? Context.PlayerController->GetPawn() : nullptr;
		const FVector CurrentLocation = ControlledPawn ? ControlledPawn->GetActorLocation() : FVector::ZeroVector;
		const FVector TransferTargetPos = Context.PlayerState ? Context.PlayerState->GetTransferTargetPos() : FVector::ZeroVector;

		DrawPlayerTextInfo(Context, CurrentLocation, TransferTargetPos);
		DrawPlayerActions(Context, CurrentLocation, TransferTargetPos);
		DrawStartWorkGmControls(Context);
	}

	static void DrawImGuiInputSettings()
	{
		bool bShowScreenMessages = GAreScreenMessagesEnabled;
		if (ImGui::Checkbox("Show UE Screen Messages", &bShowScreenMessages))
		{
			GAreScreenMessagesEnabled = bShowScreenMessages;
		}

		FImGuiModuleProperties& Properties = FImGuiModule::Get().GetProperties();
		ImGui::Text("Input Enabled: %s", Properties.IsInputEnabled() ? "true" : "false");

		bool bMouseInputShared = Properties.IsMouseInputShared();
		if (ImGui::Checkbox("Mouse Input Shared", &bMouseInputShared))
		{
			Properties.SetMouseInputShared(bMouseInputShared);
		}

		bool bKeyboardInputShared = Properties.IsKeyboardInputShared();
		if (ImGui::Checkbox("Keyboard Input Shared", &bKeyboardInputShared))
		{
			Properties.SetKeyboardInputShared(bKeyboardInputShared);
		}

		bool bGamepadInputShared = Properties.IsGamepadInputShared();
		if (ImGui::Checkbox("Gamepad Input Shared", &bGamepadInputShared))
		{
			Properties.SetGamepadInputShared(bGamepadInputShared);
		}
	}
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugMiscPanel::DrawGeneral(const FGangstarLocalDebugContext& Context)
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	GangstarLocalDebugMiscPanel::DrawPlayerSection(Context);
#else
	(void)Context;
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugMiscPanel::DrawOptions()
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	GangstarLocalDebugMiscPanel::DrawImGuiInputSettings();
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}
