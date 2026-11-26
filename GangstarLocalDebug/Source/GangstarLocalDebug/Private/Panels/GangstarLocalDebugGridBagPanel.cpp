#include "Panels/GangstarLocalDebugGridBagPanel.h"

#include "GangstarLocalDebugContext.h"
#include "GangstarLocalDebugHelpers.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
#include "Characters/BasePlayerState.h"
#include "Gangstar/GridBag/GridBag.h"
#include "PlayerProfile/GridBagStructure.h"
#include <imgui.h>
#endif // WITH_GANGSTAR_LOCAL_IMGUI

namespace GangstarLocalDebugGridBagPanel
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	static FString FormatItemGuid(const FItemGUID& Value)
	{
		return Value.IsValid() ? FString::Printf(TEXT("%lld"), Value.GetId()) : TEXT("None");
	}

	static const FGridItemData* FindItem(const TArray<FGridItemData>& Items, const FItemGUID& ItemGid)
	{
		if (!ItemGid.IsValid())
		{
			return nullptr;
		}

		for (const FGridItemData& Item : Items)
		{
			if (Item.Gid == ItemGid)
			{
				return &Item;
			}
		}

		return nullptr;
	}

	static bool FindItemLocation(const TArray<FGridBagData>& Bags, const FItemGUID& ItemGid, FItemGUID& OutBagGid, int32& OutGridIndex)
	{
		if (!ItemGid.IsValid())
		{
			return false;
		}

		for (const FGridBagData& Bag : Bags)
		{
			for (int32 GridIndex = 0; GridIndex < Bag.Grids.Num(); ++GridIndex)
			{
				if (Bag.Grids[GridIndex] == ItemGid)
				{
					OutBagGid = Bag.Gid;
					OutGridIndex = GridIndex;
					return true;
				}
			}
		}

		return false;
	}

	static int32 CountUsedSlots(const FGridBagData& Bag)
	{
		int32 UsedSlots = 0;
		for (const FItemGUID& ItemGid : Bag.Grids)
		{
			if (ItemGid.IsValid())
			{
				++UsedSlots;
			}
		}

		return UsedSlots;
	}

	static const char* GetGridState(const FGridBagData& Bag, int32 GridIndex, const FGridItemData* Item)
	{
		if (GridIndex >= Bag.UnlockSlots)
		{
			return "Locked";
		}

		if (!Bag.Grids[GridIndex].IsValid())
		{
			return "Empty";
		}

		return Item ? "Item" : "MissingItem";
	}

	static void DrawGridRows(const FGridBagData& Bag, const TArray<FGridItemData>& Items)
	{
		if (Bag.Grids.Num() == 0)
		{
			ImGui::TextUnformatted("No grids.");
			return;
		}

		ImGui::Columns(7, "GridBagGridColumns", true);
		ImGui::TextUnformatted("Index"); ImGui::NextColumn();
		ImGui::TextUnformatted("State"); ImGui::NextColumn();
		ImGui::TextUnformatted("ItemGid"); ImGui::NextColumn();
		ImGui::TextUnformatted("TemplateId"); ImGui::NextColumn();
		ImGui::TextUnformatted("NumberId"); ImGui::NextColumn();
		ImGui::TextUnformatted("Count"); ImGui::NextColumn();
		ImGui::TextUnformatted("DelFlag"); ImGui::NextColumn();
		ImGui::Separator();

		for (int32 GridIndex = 0; GridIndex < Bag.Grids.Num(); ++GridIndex)
		{
			const FItemGUID& ItemGid = Bag.Grids[GridIndex];
			const FGridItemData* Item = FindItem(Items, ItemGid);

			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), GridIndex)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(UTF8_TO_TCHAR(GetGridState(Bag, GridIndex, Item))); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FormatItemGuid(ItemGid)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(Item ? GangstarLocalDebugHelpers::FormatName(Item->TemplateId) : TEXT("")); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(Item ? GangstarLocalDebugHelpers::FormatName(Item->NumberId) : TEXT("")); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(Item ? FString::Printf(TEXT("%lld"), Item->Count) : TEXT("")); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(Item ? GangstarLocalDebugHelpers::FormatBool(Item->DelFlag) : TEXT("")); ImGui::NextColumn();
		}
		ImGui::Columns(1);
	}

	static void DrawBagRows(const TArray<FGridBagData>& Bags, const TArray<FGridItemData>& Items)
	{
		if (Bags.Num() == 0)
		{
			ImGui::TextUnformatted("No bags.");
			return;
		}

		ImGui::Columns(9, "GridBagBagColumns", true);
		ImGui::TextUnformatted("Gid"); ImGui::NextColumn();
		ImGui::TextUnformatted("TemplateId"); ImGui::NextColumn();
		ImGui::TextUnformatted("ModeType"); ImGui::NextColumn();
		ImGui::TextUnformatted("BagType"); ImGui::NextColumn();
		ImGui::TextUnformatted("UnlockSlots"); ImGui::NextColumn();
		ImGui::TextUnformatted("GridCount"); ImGui::NextColumn();
		ImGui::TextUnformatted("UsedSlots"); ImGui::NextColumn();
		ImGui::TextUnformatted("CurrentWeight"); ImGui::NextColumn();
		ImGui::TextUnformatted("Details"); ImGui::NextColumn();
		ImGui::Separator();

		for (const FGridBagData& Bag : Bags)
		{
			GangstarLocalDebugHelpers::DrawCopyableText(FormatItemGuid(Bag.Gid)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Bag.TemplateId)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(Bag.ModeType)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(Bag.BagType)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Bag.UnlockSlots)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Bag.Grids.Num())); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), CountUsedSlots(Bag))); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%.2f"), Bag.CurrentWeight)); ImGui::NextColumn();

			const FString TreeId = FString::Printf(TEXT("Grids##%lld"), Bag.Gid.GetId());
			if (ImGui::TreeNode(TCHAR_TO_UTF8(*TreeId)))
			{
				GangstarLocalDebugGridBagPanel::DrawGridRows(Bag, Items);
				ImGui::TreePop();
			}
			ImGui::NextColumn();
		}
		ImGui::Columns(1);
	}

	static void DrawItemRows(const TArray<FGridBagData>& Bags, const TArray<FGridItemData>& Items)
	{
		if (!ImGui::CollapsingHeader("All Items", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		ImGui::Text("Item Count: %d", Items.Num());
		if (Items.Num() == 0)
		{
			ImGui::TextUnformatted("No items.");
			return;
		}

		ImGui::Columns(7, "GridBagItemColumns", true);
		ImGui::TextUnformatted("Gid"); ImGui::NextColumn();
		ImGui::TextUnformatted("TemplateId"); ImGui::NextColumn();
		ImGui::TextUnformatted("NumberId"); ImGui::NextColumn();
		ImGui::TextUnformatted("Count"); ImGui::NextColumn();
		ImGui::TextUnformatted("DelFlag"); ImGui::NextColumn();
		ImGui::TextUnformatted("LocatedBag"); ImGui::NextColumn();
		ImGui::TextUnformatted("GridIndex"); ImGui::NextColumn();
		ImGui::Separator();

		for (const FGridItemData& Item : Items)
		{
			FItemGUID LocatedBagGid;
			int32 LocatedGridIndex = INDEX_NONE;
			const bool bLocated = FindItemLocation(Bags, Item.Gid, LocatedBagGid, LocatedGridIndex);

			GangstarLocalDebugHelpers::DrawCopyableText(FormatItemGuid(Item.Gid)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Item.TemplateId)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Item.NumberId)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%lld"), Item.Count)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatBool(Item.DelFlag)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(bLocated ? FormatItemGuid(LocatedBagGid) : TEXT("NotInBag")); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(bLocated ? FString::Printf(TEXT("%d"), LocatedGridIndex) : TEXT("")); ImGui::NextColumn();
		}
		ImGui::Columns(1);
	}
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugGridBagPanel::Draw(const FGangstarLocalDebugContext& Context)
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	if (!Context.PlayerState)
	{
		ImGui::TextUnformatted("PlayerState not found.");
		return;
	}

	UGridBag* PlayerGridBag = Context.PlayerState->PlayerGridBag;
	if (!PlayerGridBag)
	{
		ImGui::TextUnformatted("PlayerGridBag not found.");
		return;
	}

	const TArray<FGridBagData>& Bags = PlayerGridBag->GetBags();
	const TArray<FGridItemData>& Items = PlayerGridBag->GetItems();

	ImGui::TextUnformatted("Source: ABasePlayerState::PlayerGridBag");
	ImGui::Text("Bag Count: %d", Bags.Num());
	ImGui::SameLine();
	ImGui::Text("Item Count: %d", Items.Num());
	ImGui::Separator();

	if (ImGui::CollapsingHeader("Bags", ImGuiTreeNodeFlags_DefaultOpen))
	{
		GangstarLocalDebugGridBagPanel::DrawBagRows(Bags, Items);
	}

	GangstarLocalDebugGridBagPanel::DrawItemRows(Bags, Items);
#else
	(void)Context;
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}
