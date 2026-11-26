#include "Panels/GangstarLocalDebugTradeCalcPanel.h"

#include "GangstarLocalDebugContext.h"
#include "GangstarLocalDebugHelpers.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
#include "Engine/Texture2D.h"
#include "RealProperty/HappyLab/HappyLabTradeCalcDebugTypes.h"
#include "ImGuiModule.h"
#include "ImGuiTextureHandle.h"
#include <imgui.h>
#endif // WITH_GANGSTAR_LOCAL_IMGUI

namespace GangstarLocalDebugTradeCalcPanel
{
#if WITH_GANGSTAR_LOCAL_IMGUI && HAPPYLAB_IMGUI_TRADE_CALC_DEBUG
	static ImVec4 GetTagColor(bool bMatched, float AffectPercent)
	{
		if (!bMatched)
		{
			return ImVec4(0.35f, 0.35f, 0.35f, 0.85f);
		}
		return AffectPercent >= 0.0f ? ImVec4(0.12f, 0.55f, 0.20f, 0.95f) : ImVec4(0.65f, 0.16f, 0.16f, 0.95f);
	}

	static void DrawTagChip(const FString& Text, const ImVec4& Color)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, Color);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Color);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, Color);
		ImGui::SmallButton(TCHAR_TO_UTF8(*Text));
		ImGui::PopStyleColor(3);
	}

	static void DrawTagList(const TArray<FName>& Tags, const ImVec4& Color)
	{
		if (Tags.Num() <= 0)
		{
			ImGui::TextUnformatted("None");
			return;
		}

		for (int32 Index = 0; Index < Tags.Num(); ++Index)
		{
			if (Index > 0)
			{
				ImGui::SameLine();
			}
			DrawTagChip(GangstarLocalDebugHelpers::FormatName(Tags[Index]), Color);
		}
	}

	static FImGuiTextureHandle FindOrRegisterTexture(const FSoftObjectPath& TexturePath)
	{
		if (!TexturePath.IsValid())
		{
			return FImGuiTextureHandle();
		}

		const FName TextureName(*TexturePath.ToString());
		FImGuiTextureHandle TextureHandle = FImGuiModule::Get().FindTextureHandle(TextureName);
		if (TextureHandle.IsValid())
		{
			return TextureHandle;
		}

		UTexture2D* Texture = Cast<UTexture2D>(TexturePath.TryLoad());
		return Texture ? FImGuiModule::Get().RegisterTexture(TextureName, Texture) : FImGuiTextureHandle();
	}

	static void DrawItemIcon(const FHappyLabTradeCalcSubmittedItemDebug& Item)
	{
		FImGuiTextureHandle TextureHandle = FindOrRegisterTexture(Item.ItemIconPath);
		if (TextureHandle.IsValid())
		{
			ImGui::Image(TextureHandle, ImVec2(64.0f, 64.0f));
			ImGui::SameLine();
			return;
		}

		ImGui::TextUnformatted("[NoIcon]");
		ImGui::SameLine();
	}

	static void DrawTagMatches(const FHappyLabTradeCalcSubmittedItemDebug& Item)
	{
		if (Item.TagMatches.Num() <= 0)
		{
			ImGui::TextUnformatted("No props tags.");
			return;
		}

		ImGui::Columns(6, "TradeCalcTagMatchColumns", true);
		ImGui::TextUnformatted("PropsTag"); ImGui::NextColumn();
		ImGui::TextUnformatted("MatchingCustomerTag"); ImGui::NextColumn();
		ImGui::TextUnformatted("HasCustomerTag"); ImGui::NextColumn();
		ImGui::TextUnformatted("Config"); ImGui::NextColumn();
		ImGui::TextUnformatted("Affect"); ImGui::NextColumn();
		ImGui::TextUnformatted("Result"); ImGui::NextColumn();
		ImGui::Separator();

		for (const FHappyLabTradeCalcTagMatchDebug& Match : Item.TagMatches)
		{
			const bool bMatched = Match.bCustomerHasTag && Match.bHasPriceAffect;
			DrawTagChip(GangstarLocalDebugHelpers::FormatName(Match.PropsTagID), ImVec4(0.10f, 0.32f, 0.80f, 0.95f)); ImGui::NextColumn();
			DrawTagChip(GangstarLocalDebugHelpers::FormatName(Match.MatchingCustomerTagID), bMatched ? ImVec4(0.55f, 0.20f, 0.85f, 0.95f) : ImVec4(0.35f, 0.35f, 0.35f, 0.85f)); ImGui::NextColumn();
			ImGui::TextUnformatted(Match.bCustomerHasTag ? "true" : "false"); ImGui::NextColumn();
			ImGui::Text("Props=%d Customer=%d Price=%d", Match.bHasPropsTagConfig ? 1 : 0, Match.bHasCustomerTagConfig ? 1 : 0, Match.bHasPriceAffect ? 1 : 0); ImGui::NextColumn();
			ImGui::Text("%+.2f%%", Match.AffectPercent * 100.0f); ImGui::NextColumn();
			DrawTagChip(bMatched ? TEXT("MATCH") : TEXT("MISS"), GetTagColor(bMatched, Match.AffectPercent)); ImGui::NextColumn();
		}
		ImGui::Columns(1);
	}

	static void DrawSubmittedItem(const FHappyLabTradeCalcSubmittedItemDebug& Item, int32 Index)
	{
		const FString Header = FString::Printf(TEXT("%s x%d##TradeCalcItem%d"), *GangstarLocalDebugHelpers::FormatName(Item.GoodsID), Item.Count, Index);
		if (!ImGui::TreeNode(TCHAR_TO_UTF8(*Header)))
		{
			return;
		}

		DrawItemIcon(Item);
		ImGui::Text("%s (%s)", TCHAR_TO_UTF8(*GangstarLocalDebugHelpers::FormatText(Item.ItemName)), TCHAR_TO_UTF8(*GangstarLocalDebugHelpers::FormatName(Item.GoodsID)));
		ImGui::Text("IconPath: %s", TCHAR_TO_UTF8(*Item.ItemIconPath.ToString()));
		ImGui::Text("SubType: %s", TCHAR_TO_UTF8(*GangstarLocalDebugHelpers::FormatEnum(Item.ItemSubType)));

		ImGui::Separator();
		ImGui::TextUnformatted("Item Tags:");
		DrawTagList(Item.PropsTags, ImVec4(0.10f, 0.32f, 0.80f, 0.95f));

		ImGui::Separator();
		ImGui::TextUnformatted("Tag Match Process:");
		DrawTagMatches(Item);

		ImGui::Separator();
		ImGui::Text("BasePrice = %d", Item.BasePrice);
		ImGui::Text("ItemPriceRandRatio = %.4f", Item.ItemPriceRandRatio);
		ImGui::Text("NegotiatedUnitPrice = Ceil(%d * %.4f) = %d", Item.BasePrice, Item.ItemPriceRandRatio, Item.NegotiatedUnitPrice);
		ImGui::Text("TagAffect = %+.2f%%", Item.TagAffectPercent * 100.0f);
		ImGui::Text("CustomerPriceRatio = %.4f", Item.CustomerPriceRatio);
		ImGui::Text("FinalUnitPrice = Ceil(%d * (1 %+0.4f) * %.4f) = %d", Item.NegotiatedUnitPrice, Item.TagAffectPercent, Item.CustomerPriceRatio, Item.FinalUnitPrice);
		ImGui::Text("Subtotal = %d * %d = %lld", Item.Count, Item.FinalUnitPrice, Item.FinalSubtotal);

		ImGui::TreePop();
	}
#endif // WITH_GANGSTAR_LOCAL_IMGUI && HAPPYLAB_IMGUI_TRADE_CALC_DEBUG
}

void FGangstarLocalDebugTradeCalcPanel::Draw(const FGangstarLocalDebugContext& Context)
{
#if WITH_GANGSTAR_LOCAL_IMGUI && HAPPYLAB_IMGUI_TRADE_CALC_DEBUG
	(void)Context;

	FHappyLabTradeCalcDebugSnapshot Snapshot;
	if (!HappyLabTradeCalcDebug::GetLatestSnapshot(Snapshot))
	{
		ImGui::TextUnformatted("No HappyLab trade calculation snapshot yet. Submit or preview a customer trade to populate this panel.");
		return;
	}

	if (ImGui::Button("Clear Snapshot"))
	{
		HappyLabTradeCalcDebug::ClearLatestSnapshot();
		return;
	}

	ImGui::SameLine();
	ImGui::Text("CaptureTimeSeconds: %.3f", Snapshot.CaptureTimeSeconds);
	ImGui::Separator();

	ImGui::Text("CustomerID: %s", TCHAR_TO_UTF8(*GangstarLocalDebugHelpers::FormatName(Snapshot.CustomerID)));
	ImGui::Text("DemandGoodsType: %s", TCHAR_TO_UTF8(*GangstarLocalDebugHelpers::FormatEnum(Snapshot.DemandGoodsType)));
	ImGui::Text("DemandCount: %d | SubmittedCount: %d", Snapshot.DemandCount, Snapshot.SubmittedCount);
	ImGui::Text("Demand.UnitPrice: %d (display/log reference only)", Snapshot.DemandUnitPrice);
	ImGui::Text("ItemPriceRandRatio: %.4f | CustomerPriceRatio: %.4f", Snapshot.DemandItemPriceRandRatio, Snapshot.CustomerPriceRatio);
	ImGui::Text("NegotiatedTotalPrice: %lld", Snapshot.NegotiatedTotalPrice);
	ImGui::Text("FinalTotalPrice: %lld", Snapshot.FinalTotalPrice);
	ImGui::Text("TotalAffectPercent: %+.2f%%", Snapshot.TotalAffectPercent * 100.0f);

	ImGui::Separator();
	ImGui::TextUnformatted("Customer Tags:");
	GangstarLocalDebugTradeCalcPanel::DrawTagList(Snapshot.CustomerTags, ImVec4(0.55f, 0.20f, 0.85f, 0.95f));

	ImGui::Separator();
	ImGui::Text("Submitted Items: %d", Snapshot.SubmittedItems.Num());
	for (int32 Index = 0; Index < Snapshot.SubmittedItems.Num(); ++Index)
	{
		GangstarLocalDebugTradeCalcPanel::DrawSubmittedItem(Snapshot.SubmittedItems[Index], Index);
	}
#else
	(void)Context;
#endif // WITH_GANGSTAR_LOCAL_IMGUI && HAPPYLAB_IMGUI_TRADE_CALC_DEBUG
}
