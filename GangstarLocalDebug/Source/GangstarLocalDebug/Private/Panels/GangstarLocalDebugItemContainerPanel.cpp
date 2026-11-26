#include "Panels/GangstarLocalDebugItemContainerPanel.h"

#include "GangstarLocalDebugContext.h"
#include "GangstarLocalDebugHelpers.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
#include "Characters/BasePlayerState.h"
#include "ItemContainer/ItemContainerReplicatorComponent.h"
#include "PlayerProfile/PlayerProfileContainer.h"
#include <imgui.h>
#endif // WITH_GANGSTAR_LOCAL_IMGUI

namespace GangstarLocalDebugItemContainerPanel
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	static FItemStackSaveData ConvertStack(const FItemStack& RuntimeStack)
	{
		FItemStackSaveData SaveStack;
		SaveStack.TemplateId = RuntimeStack.TemplateId;
		SaveStack.ItemId = RuntimeStack.ItemId;
		SaveStack.Count = RuntimeStack.Count;
		SaveStack.Quality = RuntimeStack.Quality;
		SaveStack.Flags = RuntimeStack.Flags;
		return SaveStack;
	}

	static FItemContainerSaveData ConvertContainer(const FItemContainer& Container)
	{
		FItemContainerSaveData SaveData;
		SaveData.ContainerId = Container.ContainerId;
		SaveData.Owner.OwnerType = static_cast<EItemContainerSaveOwnerType>(Container.Owner.OwnerType);
		SaveData.Owner.OwnerId = Container.Owner.OwnerId;
		SaveData.ContainerType = static_cast<EItemContainerSaveType>(Container.ContainerType);
		SaveData.LayoutId = Container.LayoutId;
		SaveData.SlotCount = Container.Slots.Num();
		SaveData.Version = Container.Version;

		for (int32 SlotIndex = 0; SlotIndex < Container.Slots.Num(); ++SlotIndex)
		{
			const FItemSlot& Slot = Container.Slots[SlotIndex];
			if (Slot.Stack.IsEmpty() && Slot.State == 0)
			{
				continue;
			}

			FItemSlotSaveData SlotSaveData;
			SlotSaveData.SlotIndex = SlotIndex;
			SlotSaveData.Stack = ConvertStack(Slot.Stack);
			SlotSaveData.State = Slot.State;
			SaveData.NonEmptySlots.Add(SlotSaveData);
		}

		return SaveData;
	}

	static void AppendProfileContainers(const FPlayerItemContainerData& PlayerItemContainerData, TArray<FItemContainerSaveData>& OutContainers)
	{
		for (const FItemContainerOwnerContainersSaveData& OwnerContainerData : PlayerItemContainerData.Owners)
		{
			OutContainers.Append(OwnerContainerData.Containers);
		}
	}

	static void AppendReplicatedContainers(const UItemContainerReplicatorComponent* Replicator, TArray<FItemContainerSaveData>& OutContainers)
	{
		if (!Replicator)
		{
			return;
		}

		for (const FRepItemContainerSnapshot& Snapshot : Replicator->GetRepContainers().Items)
		{
			if (Snapshot.Container.ContainerId.IsValid())
			{
				OutContainers.Add(ConvertContainer(Snapshot.Container));
			}
		}
	}

	static void DrawSlotRows(const FItemContainerSaveData& Container)
	{
		if (Container.NonEmptySlots.Num() == 0)
		{
			ImGui::TextUnformatted("No non-empty slots.");
			return;
		}

		ImGui::Columns(7, "ItemContainerSlotColumns", true);
		ImGui::TextUnformatted("Slot"); ImGui::NextColumn();
		ImGui::TextUnformatted("TemplateId"); ImGui::NextColumn();
		ImGui::TextUnformatted("ItemId"); ImGui::NextColumn();
		ImGui::TextUnformatted("Count"); ImGui::NextColumn();
		ImGui::TextUnformatted("Quality"); ImGui::NextColumn();
		ImGui::TextUnformatted("Flags"); ImGui::NextColumn();
		ImGui::TextUnformatted("State"); ImGui::NextColumn();
		ImGui::Separator();

		for (const FItemSlotSaveData& Slot : Container.NonEmptySlots)
		{
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Slot.SlotIndex)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Slot.Stack.TemplateId)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Slot.Stack.ItemId)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Slot.Stack.Count)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%u"), Slot.Stack.Quality)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Slot.Stack.Flags)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%u"), Slot.State)); ImGui::NextColumn();
		}
		ImGui::Columns(1);
	}

	static void DrawContainerHeader(const char* ColumnsId)
	{
		ImGui::Columns(9, ColumnsId, true);
		ImGui::TextUnformatted("ContainerId"); ImGui::NextColumn();
		ImGui::TextUnformatted("OwnerType"); ImGui::NextColumn();
		ImGui::TextUnformatted("OwnerId"); ImGui::NextColumn();
		ImGui::TextUnformatted("Type"); ImGui::NextColumn();
		ImGui::TextUnformatted("LayoutId"); ImGui::NextColumn();
		ImGui::TextUnformatted("SlotCount"); ImGui::NextColumn();
		ImGui::TextUnformatted("Version"); ImGui::NextColumn();
		ImGui::TextUnformatted("NonEmpty"); ImGui::NextColumn();
		ImGui::TextUnformatted("Details"); ImGui::NextColumn();
		ImGui::Separator();
	}

	static void DrawContainerRows(const char* SectionId, const TArray<FItemContainerSaveData>& Containers)
	{
		DrawContainerHeader(TCHAR_TO_UTF8(*FString::Printf(TEXT("ItemContainerColumns_%s"), UTF8_TO_TCHAR(SectionId))));
		for (const FItemContainerSaveData& Container : Containers)
		{
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatGuid(Container.ContainerId)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(Container.Owner.OwnerType)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatGuid(Container.Owner.OwnerId)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(Container.ContainerType)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Container.LayoutId)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Container.SlotCount)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Container.Version)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Container.NonEmptySlots.Num())); ImGui::NextColumn();

			const FString TreeId = FString::Printf(TEXT("Slots##%s_%s"), UTF8_TO_TCHAR(SectionId), *Container.ContainerId.ToString(EGuidFormats::Digits));
			if (ImGui::TreeNode(TCHAR_TO_UTF8(*TreeId)))
			{
				GangstarLocalDebugItemContainerPanel::DrawSlotRows(Container);
				ImGui::TreePop();
			}
			ImGui::NextColumn();
		}
		ImGui::Columns(1);
	}

	static void DrawContainerSection(const char* Header, const char* SectionId, const TCHAR* Source, const TArray<FItemContainerSaveData>& Containers, bool bDefaultOpen, const ImVec4* HeaderTextColor = nullptr)
	{
		const ImGuiTreeNodeFlags HeaderFlags = bDefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0;
		if (HeaderTextColor)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, *HeaderTextColor);
		}
		const bool bOpen = ImGui::CollapsingHeader(Header, HeaderFlags);
		if (HeaderTextColor)
		{
			ImGui::PopStyleColor();
		}
		if (!bOpen)
		{
			return;
		}

		ImGui::Text("Container Count: %d", Containers.Num());
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("Source: %s"), Source));
		ImGui::Separator();
		DrawContainerRows(SectionId, Containers);
	}
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugItemContainerPanel::Draw(const FGangstarLocalDebugContext& Context)
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	TArray<FItemContainerSaveData> ProfileContainers;
	if (Context.PlayerState)
	{
		if (const FPlayerProfileData* PlayerProfile = Context.PlayerState->GetPlayerProfileIfReady())
		{
			GangstarLocalDebugItemContainerPanel::AppendProfileContainers(PlayerProfile->PlayerItemContainerData, ProfileContainers);
		}
	}

	TArray<FItemContainerSaveData> OwnerOnlyContainers;
	GangstarLocalDebugItemContainerPanel::AppendReplicatedContainers(Context.OwnerOnlyItemContainerReplicator, OwnerOnlyContainers);

	TArray<FItemContainerSaveData> FactoryContainers;
	GangstarLocalDebugItemContainerPanel::AppendReplicatedContainers(Context.FactoryItemContainerReplicator, FactoryContainers);

	ImGui::TextUnformatted("Runtime UItemContainerManager::Containers is not replicated and is intentionally not used as the client-side source.");
	ImGui::Separator();

	const ImVec4 DeprecatedReplicatedSnapshotHeaderColor(1.0f, 0.0f, 0.0f, 1.0f);

	GangstarLocalDebugItemContainerPanel::DrawContainerSection(
		"Profile Save Data",
		"Profile",
		TEXT("PlayerProfileData->PlayerItemContainerData"),
		ProfileContainers,
		true);

	GangstarLocalDebugItemContainerPanel::DrawContainerSection(
		"OwnerOnly Replicated Snapshots",
		"OwnerOnlyRep",
		TEXT("ABasePlayerState::OwnerOnlyItemContainerReplicatorComponent"),
		OwnerOnlyContainers,
		true,
		&DeprecatedReplicatedSnapshotHeaderColor);

	GangstarLocalDebugItemContainerPanel::DrawContainerSection(
		"Factory Replicated Snapshots",
		"FactoryRep",
		TEXT("ARealPropertyHappyLabFactory::ItemContainerReplicator"),
		FactoryContainers,
		true,
		&DeprecatedReplicatedSnapshotHeaderColor);
#else
	(void)Context;
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}
