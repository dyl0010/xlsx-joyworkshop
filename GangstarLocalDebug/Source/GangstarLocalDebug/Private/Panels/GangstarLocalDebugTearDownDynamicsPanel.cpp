#include "Panels/GangstarLocalDebugTearDownDynamicsPanel.h"

#include "GangstarLocalDebugContext.h"
#include "GangstarLocalDebugHelpers.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
#include "Characters/BasePlayerState.h"
#include "GSTMGameFramework/Table/TableManager.h"
#include "Inventory/InventoryRacingVehicle.h"
#include "PlayerProfile/InventoryStructure.h"
#include "PlayerProfile/PlayerProfileContainer.h"
#include "PlayerProfile/TearDownDynamicsRaceStructure.h"
#include "RealProperty/TearDownDynamics/AutoWorkshopRaceViewService.h"
#include "RealProperty/TearDownDynamics/Race/ScrapRaceDefine.h"
#include "RealProperty/TearDownDynamics/TearDownDynamicsFunctionLibrary.h"
#include <imgui.h>
#endif // WITH_GANGSTAR_LOCAL_IMGUI

namespace GangstarLocalDebugTearDownDynamicsPanel
{
#if WITH_GANGSTAR_LOCAL_IMGUI
    static FString FormatItemGuid(const FItemGUID& Value)
    {
        return Value.ToString();
    }

    static FString FormatMaintenance(int64 MaintenanceEndTimestamp)
    {
        if (MaintenanceEndTimestamp <= 0)
        {
            return TEXT("None");
        }

        const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
        const int64 RemainSeconds = FMath::Max<int64>(0, MaintenanceEndTimestamp - Now);
        return FString::Printf(TEXT("%s (%llds left)"), *GangstarLocalDebugHelpers::FormatUnixTime(MaintenanceEndTimestamp), RemainSeconds);
    }

    static FString FormatTraits(const TArray<FInventoryRacingVehicleTraitData>& Traits)
    {
        if (Traits.Num() <= 0)
        {
            return TEXT("None");
        }

        TArray<FString> Parts;
        Parts.Reserve(Traits.Num());
        for (const FInventoryRacingVehicleTraitData& Trait : Traits)
        {
            Parts.Add(FString::Printf(TEXT("%s Lv.%d"), *Trait.TraitId.ToString(), Trait.Level));
        }
        return FString::Join(Parts, TEXT(", "));
    }

    static FString FormatModifications(const TArray<FInventoryRacingVehicleModificationData>& Modifications)
    {
        if (Modifications.Num() <= 0)
        {
            return TEXT("None");
        }

        TArray<FString> Groups;
        Groups.Reserve(Modifications.Num());
        for (const FInventoryRacingVehicleModificationData& Modification : Modifications)
        {
            TArray<FString> Attributes;
            Attributes.Reserve(Modification.Attributes.Num());
            for (const FInventoryRacingVehicleModificationAttributeData& Attribute : Modification.Attributes)
            {
                Attributes.Add(FString::Printf(TEXT("%s=%+d"),
                    *GangstarLocalDebugHelpers::FormatEnum(Attribute.AttributeType),
                    Attribute.DeltaValue));
            }

            Groups.Add(FString::Printf(TEXT("%s[%s]"),
                *Modification.ModificationGroupId.ToString(),
                *FString::Join(Attributes, TEXT(", "))));
        }
        return FString::Join(Groups, TEXT("; "));
    }

    static FString FormatAttributes(const FRaceCarAttribute& Attribute)
    {
        return FString::Printf(TEXT("Top=%.1f Acc=%.1f Corner=%.1f Stability=%.1f Brake=%.1f"),
            Attribute.TopSpeed,
            Attribute.Accelerate,
            Attribute.Cornering,
            Attribute.Stability,
            Attribute.Brake);
    }

    static void DrawCopyButton(const char* Id, const FString& Text)
    {
        if (ImGui::SmallButton(Id))
        {
            ImGui::SetClipboardText(TCHAR_TO_UTF8(*Text));
        }
    }

    static void DrawBoolText(bool bValue)
    {
        ImGui::TextColored(
            bValue ? ImVec4(0.20f, 0.90f, 0.20f, 1.0f) : ImVec4(1.0f, 0.35f, 0.25f, 1.0f),
            bValue ? "true" : "false");
    }

    static const FPlayerRaceCarTable* FindRaceCarConfig(FName TemplateId)
    {
        const UTableManager* TableManager = UTableManager::Get();
        return TableManager ? TableManager->FindTableRow<FPlayerRaceCarTable>(TemplateId, TEXT("GangstarLocalDebugTearDownDynamicsPanel")) : nullptr;
    }

    static FString GetLocalOperationGate(
        const FInventoryRacingVehicleData& Vehicle,
        const FPlayerRaceCarTable* Config,
        int64 Now,
        bool bRequireNotEvaluated,
        bool bCheckMaintenance)
    {
        if (!Config)
        {
            return TEXT("InvalidConfig");
        }
        if (!Vehicle.LockedRaceId.IsNone())
        {
            return TEXT("RaceLocked");
        }
        if (!Vehicle.WorkingStationId.IsNone())
        {
            return TEXT("Working");
        }
        if (bCheckMaintenance && Vehicle.MaintenanceEndTimestamp > Now)
        {
            return TEXT("InMaintenance");
        }
        if (bRequireNotEvaluated && Vehicle.bEvaluated)
        {
            return TEXT("AlreadyEvaluated");
        }
        if (Vehicle.CurrentDurability <= 0)
        {
            return TEXT("ZeroDurability");
        }
        return TEXT("Success");
    }

    static void DrawGateText(const char* Label, const FString& Result)
    {
        const bool bSuccess = Result == TEXT("Success");
        ImGui::TextUnformatted(Label);
        ImGui::NextColumn();
        ImGui::TextColored(
            bSuccess ? ImVec4(0.20f, 0.90f, 0.20f, 1.0f) : ImVec4(1.0f, 0.35f, 0.25f, 1.0f),
            "%s",
            TCHAR_TO_UTF8(*Result));
        ImGui::NextColumn();
    }

    static bool IsTimestampPropertyName(const FName PropertyName)
    {
        const FString Name = PropertyName.ToString();
        if (Name.EndsWith(TEXT("TimeMs")))
        {
            return false;
        }
        return Name.Contains(TEXT("Timestamp"))
            || Name.EndsWith(TEXT("At"))
            || Name.EndsWith(TEXT("RealTime"));
    }

    static FString FormatInt64PropertyValue(const FProperty* Property, int64 Value)
    {
        if (Value > 0 && IsTimestampPropertyName(Property->GetFName()))
        {
            return FString::Printf(TEXT("%lld (%s)"), Value, *GangstarLocalDebugHelpers::FormatUnixTime(Value));
        }
        return FString::Printf(TEXT("%lld"), Value);
    }

    static FString ExportPropertyValue(const FProperty* Property, const void* ValuePtr)
    {
        if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
        {
            return NameProperty->GetPropertyValue(ValuePtr).ToString();
        }
        if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
        {
            return StringProperty->GetPropertyValue(ValuePtr);
        }
        if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
        {
            return TextProperty->GetPropertyValue(ValuePtr).ToString();
        }
        if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
        {
            return BoolProperty->GetPropertyValue(ValuePtr) ? TEXT("true") : TEXT("false");
        }
        if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
        {
            const int64 Value = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
            return EnumProperty->GetEnum()->GetNameStringByValue(Value);
        }
        if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
        {
            if (NumericProperty->IsFloatingPoint())
            {
                return FString::Printf(TEXT("%.4f"), NumericProperty->GetFloatingPointPropertyValue(ValuePtr));
            }
            if (NumericProperty->IsInteger())
            {
                return FormatInt64PropertyValue(Property, NumericProperty->GetSignedIntPropertyValue(ValuePtr));
            }
        }
        if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
        {
            if (ByteProperty->Enum)
            {
                return ByteProperty->Enum->GetNameStringByValue(ByteProperty->GetPropertyValue(ValuePtr));
            }
            return FString::Printf(TEXT("%u"), ByteProperty->GetPropertyValue(ValuePtr));
        }

        FString Exported;
        Property->ExportTextItem(Exported, ValuePtr, nullptr, nullptr, PPF_None);
        return Exported;
    }

    static FString BuildStructSummary(const UScriptStruct* Struct, const void* StructData)
    {
        if (Struct == FItemGUID::StaticStruct())
        {
            return static_cast<const FItemGUID*>(StructData)->ToString();
        }
        if (Struct == TBaseStructure<FColor>::Get())
        {
            const FColor* Color = static_cast<const FColor*>(StructData);
            return FString::Printf(TEXT("R=%u G=%u B=%u A=%u"), Color->R, Color->G, Color->B, Color->A);
        }
        if (Struct == FRacingRaceData::StaticStruct())
        {
            const FRacingRaceData* Race = static_cast<const FRacingRaceData*>(StructData);
            return FString::Printf(TEXT("RaceId=%s State=%s Vehicles=%d RewardState=%d FrozenRewards=%d"),
                *Race->RaceId.ToString(),
                *UEnum::GetValueAsString(Race->State),
                Race->Vehicles.Num(),
                static_cast<int32>(Race->Reward.State),
                Race->Reward.FrozenRewards.Num());
        }
        if (Struct == FRacingVehicleSnapshotData::StaticStruct())
        {
            const FRacingVehicleSnapshotData* Vehicle = static_cast<const FRacingVehicleSnapshotData*>(StructData);
            return FString::Printf(TEXT("VehicleId=%s Guid=%s Rank=%d FinishTimeMs=%d Dnf=%s"),
                *Vehicle->VehicleId.ToString(),
                *Vehicle->VehicleGid.ToString(),
                Vehicle->Rank,
                Vehicle->FinishTimeMs,
                Vehicle->bDnf ? TEXT("true") : TEXT("false"));
        }
        if (Struct == FCarInfo::StaticStruct())
        {
            const FCarInfo* Vehicle = static_cast<const FCarInfo*>(StructData);
            return FString::Printf(TEXT("Name=%s Guid=%s Rank=%d Score=%.2f FinishTimeMs=%d Dnf=%s"),
                *Vehicle->Name.ToString(),
                *Vehicle->Guid.ToString(),
                Vehicle->Rank,
                Vehicle->Score,
                Vehicle->FinishTimeMs,
                Vehicle->bDnf ? TEXT("true") : TEXT("false"));
        }
        return FString();
    }

    static void DrawStructTree(const char* Label, const UScriptStruct* Struct, const void* StructData, int32 Depth);
    static void DrawPropertyValueTree(const FString& Label, const FProperty* Property, const void* ValuePtr, int32 Depth);

    static void DrawLeafPropertyValue(const FString& Label, const FString& Value)
    {
        ImGui::TextUnformatted(TCHAR_TO_UTF8(*Label));
        ImGui::SameLine();
        GangstarLocalDebugHelpers::DrawCopyableText(Value);
    }

    static void DrawArrayPropertyTree(const FString& Label, const FArrayProperty* ArrayProperty, const void* ValuePtr, int32 Depth)
    {
        FScriptArrayHelper Helper(ArrayProperty, ValuePtr);
        const FString Header = FString::Printf(TEXT("%s [%d]"), *Label, Helper.Num());
        if (!ImGui::TreeNode(TCHAR_TO_UTF8(*Header)))
        {
            return;
        }

        for (int32 Index = 0; Index < Helper.Num(); ++Index)
        {
            const FString ElementLabel = FString::Printf(TEXT("[%d]"), Index);
            DrawPropertyValueTree(ElementLabel, ArrayProperty->Inner, Helper.GetRawPtr(Index), Depth + 1);
        }
        ImGui::TreePop();
    }

    static void DrawMapPropertyTree(const FString& Label, const FMapProperty* MapProperty, const void* ValuePtr, int32 Depth)
    {
        FScriptMapHelper Helper(MapProperty, ValuePtr);
        const FString Header = FString::Printf(TEXT("%s [%d]"), *Label, Helper.Num());
        if (!ImGui::TreeNode(TCHAR_TO_UTF8(*Header)))
        {
            return;
        }

        int32 DisplayIndex = 0;
        for (int32 MapIndex = 0; MapIndex < Helper.GetMaxIndex(); ++MapIndex)
        {
            if (!Helper.IsValidIndex(MapIndex))
            {
                continue;
            }

            const FString EntryLabel = FString::Printf(TEXT("[%d]"), DisplayIndex++);
            if (ImGui::TreeNode(TCHAR_TO_UTF8(*EntryLabel)))
            {
                DrawPropertyValueTree(TEXT("Key"), MapProperty->KeyProp, Helper.GetKeyPtr(MapIndex), Depth + 1);
                DrawPropertyValueTree(TEXT("Value"), MapProperty->ValueProp, Helper.GetValuePtr(MapIndex), Depth + 1);
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    static void DrawPropertyValueTree(const FString& Label, const FProperty* Property, const void* ValuePtr, int32 Depth)
    {
        if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
        {
            if (StructProperty->Struct == FItemGUID::StaticStruct()
                || StructProperty->Struct == TBaseStructure<FColor>::Get())
            {
                DrawLeafPropertyValue(Label, BuildStructSummary(StructProperty->Struct, ValuePtr));
                return;
            }

            const FString Summary = BuildStructSummary(StructProperty->Struct, ValuePtr);
            const FString Header = Summary.IsEmpty()
                ? FString::Printf(TEXT("%s (%s)"), *Label, *StructProperty->Struct->GetName())
                : FString::Printf(TEXT("%s (%s)  %s"), *Label, *StructProperty->Struct->GetName(), *Summary);
            DrawStructTree(TCHAR_TO_UTF8(*Header), StructProperty->Struct, ValuePtr, Depth + 1);
            return;
        }

        if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
        {
            DrawArrayPropertyTree(Label, ArrayProperty, ValuePtr, Depth + 1);
            return;
        }

        if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
        {
            DrawMapPropertyTree(Label, MapProperty, ValuePtr, Depth + 1);
            return;
        }

        DrawLeafPropertyValue(Label, ExportPropertyValue(Property, ValuePtr));
    }

    static void DrawStructTree(const char* Label, const UScriptStruct* Struct, const void* StructData, int32 Depth)
    {
        if (!Struct || !StructData)
        {
            ImGui::TextUnformatted("<invalid struct>");
            return;
        }

        const ImGuiTreeNodeFlags Flags = Depth == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0;
        if (!ImGui::TreeNodeEx(Label, Flags))
        {
            return;
        }

        for (TFieldIterator<FProperty> It(Struct); It; ++It)
        {
            const FProperty* Property = *It;
            const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(StructData);
            DrawPropertyValueTree(Property->GetName(), Property, ValuePtr, Depth + 1);
        }

        ImGui::TreePop();
    }

    static void DrawRacingSeasonSummary(const FRacingSeasonStateData& Season)
    {
        int32 RaceCount = 0;
        for (const FRacingDayData& Day : Season.Week.Days)
        {
            RaceCount += Day.Races.Num();
        }

        const FString LastResetTimestampText = Season.LastResetTimestamp > 0
            ? FString::Printf(TEXT("%lld (%s)"), Season.LastResetTimestamp, *GangstarLocalDebugHelpers::FormatUnixTime(Season.LastResetTimestamp))
            : FString::Printf(TEXT("%lld"), Season.LastResetTimestamp);

        ImGui::Columns(2, "TDDRacingSeasonSummaryColumns", false);
        ImGui::SetColumnWidth(0, 240.0f);
        ImGui::TextUnformatted("IsEmpty"); ImGui::NextColumn(); DrawBoolText(Season.IsEmpty()); ImGui::NextColumn();
        ImGui::TextUnformatted("LastResetTimestamp"); ImGui::NextColumn(); GangstarLocalDebugHelpers::DrawCopyableText(LastResetTimestampText); ImGui::NextColumn();
        ImGui::TextUnformatted("Week Days"); ImGui::NextColumn(); ImGui::Text("%d", Season.Week.Days.Num()); ImGui::NextColumn();
        ImGui::TextUnformatted("Week Races"); ImGui::NextColumn(); ImGui::Text("%d", RaceCount); ImGui::NextColumn();
        ImGui::TextUnformatted("VehicleQualifications"); ImGui::NextColumn(); ImGui::Text("%d", Season.Week.VehicleQualifications.Num()); ImGui::NextColumn();
        ImGui::TextUnformatted("PendingRewardDeliveries"); ImGui::NextColumn(); ImGui::Text("%d", Season.PermanentProgress.PendingRewardDeliveries.Num()); ImGui::NextColumn();
        ImGui::TextUnformatted("VehicleCollection"); ImGui::NextColumn(); ImGui::Text("%d", Season.PermanentProgress.VehicleCollection.Num()); ImGui::NextColumn();
        ImGui::TextUnformatted("UnlockedRaceIds"); ImGui::NextColumn(); ImGui::Text("%d", Season.PermanentProgress.UnlockedRaceIds.Num()); ImGui::NextColumn();
        ImGui::Columns(1);
    }

    static void DrawRacingSeasonStateTree(const FGangstarLocalDebugContext& Context)
    {
        if (!Context.PlayerState || !Context.PlayerState->IsPlayerProfileReady())
        {
            ImGui::TextUnformatted("PlayerProfile is not ready.");
            return;
        }

        const TSharedPtr<FPlayerProfileData> Profile = Context.PlayerState->GetPlayerProfileData();
        if (!Profile.IsValid())
        {
            ImGui::TextUnformatted("PlayerProfile is not ready.");
            return;
        }

        const FRacingSeasonStateData& Season = Profile->GetRacingSeasonState();
        if (ImGui::CollapsingHeader("Client RacingSeasonState", ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawRacingSeasonSummary(Season);
            ImGui::Spacing();
            DrawStructTree("FRacingSeasonStateData", FRacingSeasonStateData::StaticStruct(), &Season, 0);
        }
    }

    static void DrawVehicleEntryView(UObject* WorldContext, const FName RaceId, const FInventoryRacingVehicleData& Vehicle)
    {
        if (RaceId.IsNone())
        {
            ImGui::TextUnformatted("EntryView: RaceID is empty.");
            return;
        }

        FAutoWorkshopRaceEntryView EntryView;
        const bool bHasEntryView = UTearDownDynamicsFunctionLibrary::GetAutoWorkshopRaceEntryView(
            WorldContext,
            RaceId,
            Vehicle.Gid,
            EntryView);

        ImGui::TextUnformatted("Race Entry View");
        ImGui::Separator();
        ImGui::Columns(2, "TDDEntryViewColumns", false);
        ImGui::SetColumnWidth(0, 180.0f);
        ImGui::TextUnformatted("RaceID"); ImGui::NextColumn(); GangstarLocalDebugHelpers::DrawCopyableText(RaceId.ToString()); ImGui::NextColumn();
        ImGui::TextUnformatted("Available"); ImGui::NextColumn(); DrawBoolText(bHasEntryView); ImGui::NextColumn();
        if (bHasEntryView)
        {
            ImGui::TextUnformatted("bQualified"); ImGui::NextColumn(); DrawBoolText(EntryView.bQualified); ImGui::NextColumn();
            ImGui::TextUnformatted("ConditionCount"); ImGui::NextColumn(); ImGui::Text("%d", EntryView.QualificationConditions.Num()); ImGui::NextColumn();
        }
        ImGui::Columns(1);

        if (!bHasEntryView || EntryView.QualificationConditions.Num() <= 0)
        {
            return;
        }

        ImGui::Spacing();
        if (ImGui::TreeNode("Qualification Conditions"))
        {
            ImGui::Columns(7, "TDDQualificationConditionColumns", true);
            ImGui::TextUnformatted("Done"); ImGui::NextColumn();
            ImGui::TextUnformatted("Type"); ImGui::NextColumn();
            ImGui::TextUnformatted("TextKey"); ImGui::NextColumn();
            ImGui::TextUnformatted("Current"); ImGui::NextColumn();
            ImGui::TextUnformatted("Required"); ImGui::NextColumn();
            ImGui::TextUnformatted("ShowProgress"); ImGui::NextColumn();
            ImGui::TextUnformatted("Args"); ImGui::NextColumn();
            ImGui::Separator();

            for (const FAutoWorkshopRaceConditionView& Condition : EntryView.QualificationConditions)
            {
                DrawBoolText(Condition.bCompleted); ImGui::NextColumn();
                GangstarLocalDebugHelpers::DrawCopyableText(Condition.ConditionType.ToString()); ImGui::NextColumn();
                GangstarLocalDebugHelpers::DrawCopyableText(Condition.TextKey.ToString()); ImGui::NextColumn();
                ImGui::Text("%lld", Condition.CurrentValue); ImGui::NextColumn();
                ImGui::Text("%lld", Condition.RequiredValue); ImGui::NextColumn();
                ImGui::TextUnformatted(Condition.bShowProgress ? "true" : "false"); ImGui::NextColumn();
                GangstarLocalDebugHelpers::DrawCopyableText(FString::Join(Condition.TextArgs, TEXT(", "))); ImGui::NextColumn();
            }

            ImGui::Columns(1);
            ImGui::TreePop();
        }
    }

    static void DrawVehicleDetail(const FGangstarLocalDebugContext& Context, const FInventoryRacingVehicleData& Vehicle, const FName RaceId)
    {
        const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
        const FPlayerRaceCarTable* Config = FindRaceCarConfig(Vehicle.TemplateId);

        ImGui::Columns(2, "TDDVehicleDetailColumns", false);
        ImGui::SetColumnWidth(0, 210.0f);
        ImGui::TextUnformatted("Guid"); ImGui::NextColumn(); GangstarLocalDebugHelpers::DrawCopyableText(FormatItemGuid(Vehicle.Gid)); ImGui::NextColumn();
        ImGui::TextUnformatted("TemplateId"); ImGui::NextColumn(); GangstarLocalDebugHelpers::DrawCopyableText(Vehicle.TemplateId.ToString()); ImGui::NextColumn();
        ImGui::TextUnformatted("NumberId"); ImGui::NextColumn(); GangstarLocalDebugHelpers::DrawCopyableText(Vehicle.NumberId.ToString()); ImGui::NextColumn();
        ImGui::TextUnformatted("CustomName"); ImGui::NextColumn(); GangstarLocalDebugHelpers::DrawCopyableText(Vehicle.CustomName); ImGui::NextColumn();
        ImGui::TextUnformatted("Durability"); ImGui::NextColumn(); ImGui::Text("%d / %d", Vehicle.CurrentDurability, Config ? Config->MaxDurability : 0); ImGui::NextColumn();
        ImGui::TextUnformatted("bEvaluated"); ImGui::NextColumn(); DrawBoolText(Vehicle.bEvaluated); ImGui::NextColumn();
        ImGui::TextUnformatted("WorkingStationId"); ImGui::NextColumn(); GangstarLocalDebugHelpers::DrawCopyableText(Vehicle.WorkingStationId.ToString()); ImGui::NextColumn();
        ImGui::TextUnformatted("LockedRaceId"); ImGui::NextColumn(); GangstarLocalDebugHelpers::DrawCopyableText(Vehicle.LockedRaceId.ToString()); ImGui::NextColumn();
        ImGui::TextUnformatted("LastSettledRaceId"); ImGui::NextColumn(); GangstarLocalDebugHelpers::DrawCopyableText(Vehicle.LastSettledRaceId.ToString()); ImGui::NextColumn();
        ImGui::TextUnformatted("DurabilitySettledRaceId"); ImGui::NextColumn(); GangstarLocalDebugHelpers::DrawCopyableText(Vehicle.DurabilitySettledRaceId.ToString()); ImGui::NextColumn();
        ImGui::TextUnformatted("MaintenanceEnd"); ImGui::NextColumn(); GangstarLocalDebugHelpers::DrawCopyableText(FormatMaintenance(Vehicle.MaintenanceEndTimestamp)); ImGui::NextColumn();
        ImGui::TextUnformatted("Traits"); ImGui::NextColumn(); GangstarLocalDebugHelpers::DrawCopyableText(FormatTraits(Vehicle.Traits)); ImGui::NextColumn();
        ImGui::TextUnformatted("Modifications"); ImGui::NextColumn(); GangstarLocalDebugHelpers::DrawCopyableText(FormatModifications(Vehicle.Modifications)); ImGui::NextColumn();
        ImGui::TextUnformatted("Config"); ImGui::NextColumn(); ImGui::TextColored(Config ? ImVec4(0.20f, 0.90f, 0.20f, 1.0f) : ImVec4(1.0f, 0.35f, 0.25f, 1.0f), Config ? "OK" : "Missing"); ImGui::NextColumn();
        ImGui::TextUnformatted("Config Attributes"); ImGui::NextColumn(); GangstarLocalDebugHelpers::DrawCopyableText(Config ? FormatAttributes(Config->Attribute) : TEXT("Unavailable")); ImGui::NextColumn();
        ImGui::Columns(1);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("Local Operation Gate (read-only field diagnosis; avoids non-exported inventory methods)");
        ImGui::Columns(2, "TDDOperationColumns", false);
        ImGui::SetColumnWidth(0, 210.0f);
        DrawGateText("Race", GetLocalOperationGate(Vehicle, Config, Now, false, true));
        DrawGateText("Evaluate", GetLocalOperationGate(Vehicle, Config, Now, true, false));
        DrawGateText("Modify", GetLocalOperationGate(Vehicle, Config, Now, false, false));
        DrawGateText("Dismantle", GetLocalOperationGate(Vehicle, Config, Now, false, false));
        ImGui::Columns(1);

        ImGui::Spacing();
        DrawVehicleEntryView(Context.World ? static_cast<UObject*>(Context.World) : static_cast<UObject*>(Context.PlayerController), RaceId, Vehicle);

        ImGui::Spacing();
        ImGui::Separator();
        DrawCopyButton("Copy Guid", Vehicle.Gid.ToString());
        ImGui::SameLine();
        DrawCopyButton("Copy TemplateId", Vehicle.TemplateId.ToString());
    }
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugTearDownDynamicsPanel::Draw(const FGangstarLocalDebugContext& Context)
{
#if WITH_GANGSTAR_LOCAL_IMGUI
    if (!Context.PlayerState)
    {
        ImGui::TextUnformatted("PlayerState is null.");
        return;
    }

    static char RaceIdBuffer[128] = "";
    ImGui::TextUnformatted("Read-only TearDownDynamics racing vehicle diagnostics.");
    ImGui::PushItemWidth(420.0f);
    ImGui::InputText("RaceID for EntryView", RaceIdBuffer, UE_ARRAY_COUNT(RaceIdBuffer));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear##TDDRaceID"))
    {
        RaceIdBuffer[0] = '\0';
    }

    const FName RaceId = RaceIdBuffer[0] != '\0' ? FName(UTF8_TO_TCHAR(RaceIdBuffer)) : NAME_None;
    TArray<FInventoryRacingVehicleData> Vehicles = Context.PlayerState->My_RacingVehicle.GetAllElements();
    Vehicles.Sort([](const FInventoryRacingVehicleData& Left, const FInventoryRacingVehicleData& Right)
    {
        return Left.Gid.GetId() < Right.Gid.GetId();
    });

    ImGui::Separator();
    ImGui::Text("RoleId: %lld", Context.PlayerState->GetRoleId());
    ImGui::Text("Vehicle Count: %d", Vehicles.Num());
    ImGui::Text("UtcNow: %s", TCHAR_TO_UTF8(*GangstarLocalDebugHelpers::FormatUnixTime(FDateTime::UtcNow().ToUnixTimestamp())));
    ImGui::Separator();

    GangstarLocalDebugTearDownDynamicsPanel::DrawRacingSeasonStateTree(Context);
    ImGui::Separator();

    if (Vehicles.Num() <= 0)
    {
        ImGui::TextUnformatted("No racing vehicles in My_RacingVehicle.");
        return;
    }

    for (int32 Index = 0; Index < Vehicles.Num(); ++Index)
    {
        const FInventoryRacingVehicleData& Vehicle = Vehicles[Index];
        const FPlayerRaceCarTable* Config = GangstarLocalDebugTearDownDynamicsPanel::FindRaceCarConfig(Vehicle.TemplateId);
        const FString Header = FString::Printf(TEXT("%d. %s  Guid=%s  Dur=%d/%d  Eval=%s  WS=%s  Lock=%s##TDDVehicle%d"),
            Index + 1,
            *Vehicle.TemplateId.ToString(),
            *Vehicle.Gid.ToString(),
            Vehicle.CurrentDurability,
            Config ? Config->MaxDurability : 0,
            Vehicle.bEvaluated ? TEXT("true") : TEXT("false"),
            *Vehicle.WorkingStationId.ToString(),
            *Vehicle.LockedRaceId.ToString(),
            Index);

        if (ImGui::CollapsingHeader(TCHAR_TO_UTF8(*Header), Index == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0))
        {
            GangstarLocalDebugTearDownDynamicsPanel::DrawVehicleDetail(Context, Vehicle, RaceId);
        }
    }
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}
