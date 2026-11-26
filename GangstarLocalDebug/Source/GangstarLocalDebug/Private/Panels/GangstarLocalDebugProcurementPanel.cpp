#include "Panels/GangstarLocalDebugProcurementPanel.h"

#include "GangstarLocalDebugContext.h"
#include "GangstarLocalDebugHelpers.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
#include "RealProperty/HappyLab/ViceTonicsProcurementComponent.h"
#include "RealProperty/HappyLab/ViceTonicsProcurementTypes.h"    
#include <imgui.h>
#endif // WITH_GANGSTAR_LOCAL_IMGUI

namespace GangstarLocalDebugProcurementPanel
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	template <typename ValueType>
	static bool ReadObjectStructProperty(const UObject& Object, FName PropertyName, ValueType& OutValue)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Object.GetClass()->FindPropertyByName(PropertyName));
		if (StructProperty && StructProperty->Struct == ValueType::StaticStruct())
		{
			OutValue = *StructProperty->ContainerPtrToValuePtr<ValueType>(&Object);
			return true;
		}

		return false;
	}

	template <typename ElementType>
	static void ReadObjectArrayProperty(const UObject& Object, FName PropertyName, TArray<ElementType>& OutValues)
	{
		OutValues.Reset();
		const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Object.GetClass()->FindPropertyByName(PropertyName));
		const FStructProperty* InnerStructProperty = ArrayProperty ? CastField<FStructProperty>(ArrayProperty->Inner) : nullptr;
		if (!InnerStructProperty || InnerStructProperty->Struct != ElementType::StaticStruct())
		{
			return;
		}

		FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(&Object));
		OutValues.Reserve(ArrayHelper.Num());
		for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
		{
			OutValues.Add(*reinterpret_cast<const ElementType*>(ArrayHelper.GetRawPtr(Index)));
		}
	}

	static void DrawOrders(const UViceTonicsProcurementComponent& ProcurementComponent)
	{
		TArray<FProcurementOrderClientData> Orders;
		ReadObjectArrayProperty(ProcurementComponent, TEXT("ClientOrders"), Orders);

		if (!ImGui::CollapsingHeader("Orders", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}
		ImGui::Text("Count: %d", Orders.Num());
		ImGui::Columns(10, "ProcurementOrderColumns", true);
		ImGui::TextUnformatted("OrderID"); ImGui::NextColumn();
		ImGui::TextUnformatted("QuestInstanceId"); ImGui::NextColumn();
		ImGui::TextUnformatted("ShopID"); ImGui::NextColumn();
		ImGui::TextUnformatted("FactoryId"); ImGui::NextColumn();
		ImGui::TextUnformatted("DeliveryMode"); ImGui::NextColumn();
		ImGui::TextUnformatted("State"); ImGui::NextColumn();
		ImGui::TextUnformatted("DeliveryAddress"); ImGui::NextColumn();
		ImGui::TextUnformatted("DeliveryAddrID"); ImGui::NextColumn();
		ImGui::TextUnformatted("PickupLocation"); ImGui::NextColumn();
		ImGui::TextUnformatted("Time"); ImGui::NextColumn();
		ImGui::Separator();

		for (const FProcurementOrderClientData& Order : Orders)
		{
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Order.OrderID)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatGuid(Order.QuestInstanceId)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Order.ShopID)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Order.FactoryId)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(Order.DeliveryMode)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(Order.State)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Order.DeliveryAddress)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Order.DeliveryAddrID)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatVector(Order.PickupLocation)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("Create=%s +%lldh/%lldh"),
				*GangstarLocalDebugHelpers::FormatUnixTime(Order.CreateTime),
				Order.ActivateDelayHours,
				Order.ExpireDelayHours)); ImGui::NextColumn();
		}
		ImGui::Columns(1);
	}

	static void DrawPackages(const char* Header, const TArray<FProcurementPackageClientData>& Packages)
	{
		if (!ImGui::CollapsingHeader(Header, ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		ImGui::Text("Count: %d", Packages.Num());
		ImGui::Columns(10, Header, true);
		ImGui::TextUnformatted("PackageID"); ImGui::NextColumn();
		ImGui::TextUnformatted("OrderID"); ImGui::NextColumn();
		ImGui::TextUnformatted("ShopID"); ImGui::NextColumn();
		ImGui::TextUnformatted("PickupMode"); ImGui::NextColumn();
		ImGui::TextUnformatted("State"); ImGui::NextColumn();
		ImGui::TextUnformatted("DisplayType"); ImGui::NextColumn();
		ImGui::TextUnformatted("ContainerId"); ImGui::NextColumn();
		ImGui::TextUnformatted("QuestInstanceId"); ImGui::NextColumn();
		ImGui::TextUnformatted("SpawnTime"); ImGui::NextColumn();
		ImGui::TextUnformatted("ExpireTime"); ImGui::NextColumn();
		ImGui::Separator();

		for (const FProcurementPackageClientData& Package : Packages)
		{
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Package.PackageID)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Package.OrderID)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Package.ShopID)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(Package.PickupMode)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(Package.State)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(Package.DisplayType)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatGuid(Package.ContainerId)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatGuid(Package.QuestInstanceId)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatUnixTime(Package.SpawnRealTime)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatUnixTime(Package.ExpireRealTime)); ImGui::NextColumn();
		}
		ImGui::Columns(1);
	}

	static void DrawDroppedPackage(const UViceTonicsProcurementComponent& ProcurementComponent, const char* Header, FName PropertyName, const char* EmptyText)
	{
		if (!ImGui::CollapsingHeader(Header, ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		FHappyLabDroppedPackageClientData DroppedPackage;
		if (!ReadObjectStructProperty(ProcurementComponent, PropertyName, DroppedPackage) || !DroppedPackage.bValid)
		{
			ImGui::TextUnformatted(EmptyText);
			return;
		}

		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("PackageID: %s"), *GangstarLocalDebugHelpers::FormatName(DroppedPackage.PackageID)));
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("State: %s"), *GangstarLocalDebugHelpers::FormatEnum(DroppedPackage.State)));
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("ContainerId: %s"), *GangstarLocalDebugHelpers::FormatGuid(DroppedPackage.ContainerId)));
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("QuestInstanceId: %s"), *GangstarLocalDebugHelpers::FormatGuid(DroppedPackage.QuestInstanceId)));
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("WorldLocation: %s"), *GangstarLocalDebugHelpers::FormatVector(DroppedPackage.WorldLocation)));
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("SpawnTime: %s"), *GangstarLocalDebugHelpers::FormatUnixTime(DroppedPackage.SpawnRealTime)));
		GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("ExpireTime: %s"), *GangstarLocalDebugHelpers::FormatUnixTime(DroppedPackage.ExpireRealTime)));
	}

	static void DrawCustomerTrades(const UViceTonicsProcurementComponent& ProcurementComponent)
	{
		TArray<FProcurementCustomerTradeClientData> Trades;
		ReadObjectArrayProperty(ProcurementComponent, TEXT("ClientCustomerTrades"), Trades);

		if (!ImGui::CollapsingHeader("Customer Trades", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		ImGui::Text("Count: %d", Trades.Num());
		ImGui::Columns(9, "ProcurementCustomerTradeColumns", true);
		ImGui::TextUnformatted("OrderID"); ImGui::NextColumn();
		ImGui::TextUnformatted("CustomerID"); ImGui::NextColumn();
		ImGui::TextUnformatted("State"); ImGui::NextColumn();
		ImGui::TextUnformatted("QuestInstanceId"); ImGui::NextColumn();
		ImGui::TextUnformatted("DeliveryAddress"); ImGui::NextColumn();
		ImGui::TextUnformatted("DeliveryAddrID"); ImGui::NextColumn();
		ImGui::TextUnformatted("TradeLocation"); ImGui::NextColumn();
		ImGui::TextUnformatted("Time"); ImGui::NextColumn();
		ImGui::TextUnformatted("Demands"); ImGui::NextColumn();
		ImGui::Separator();

		for (const FProcurementCustomerTradeClientData& Trade : Trades)
		{
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Trade.OrderID)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Trade.CustomerID)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(Trade.State)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatGuid(Trade.QuestInstanceId)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatName(Trade.DeliveryAddress)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Trade.DeliveryAddrID)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatVector(Trade.TradeLocation)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%.1f - %.1f"), Trade.TradeStartTime, Trade.TradeEndTime)); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(FString::Printf(TEXT("%d"), Trade.Demands.Num())); ImGui::NextColumn();
		}
		ImGui::Columns(1);
	}
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugProcurementPanel::Draw(const FGangstarLocalDebugContext& Context)
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	if (!Context.ProcurementComponent)
	{
		ImGui::TextUnformatted("ViceTonicsProcurementComponent not found.");
		return;
	}

	ImGui::Text("ProcurementComponent: %s", TCHAR_TO_UTF8(*Context.ProcurementComponent->GetName()));
	ImGui::Text("CurrentDeliveryPackageActor: %s", Context.CurrentHappyLabFactory ? TCHAR_TO_UTF8(*Context.CurrentHappyLabFactory->GetName()) : "None");
	ImGui::Separator();

	GangstarLocalDebugProcurementPanel::DrawOrders(*Context.ProcurementComponent);

	TArray<FProcurementPackageClientData> SelfPickupPackages;
	GangstarLocalDebugProcurementPanel::ReadObjectArrayProperty(*Context.ProcurementComponent, TEXT("ClientSelfPickupPackages"), SelfPickupPackages);
	GangstarLocalDebugProcurementPanel::DrawPackages("Self Pickup Packages", SelfPickupPackages);

	TArray<FProcurementPackageClientData> FactoryDeliveryPackages;
	if (Context.CurrentHappyLabFactory)
	{
		GangstarLocalDebugProcurementPanel::ReadObjectArrayProperty(*Context.CurrentHappyLabFactory, TEXT("ClientDeliveryPackages"), FactoryDeliveryPackages);
	}
	GangstarLocalDebugProcurementPanel::DrawPackages("Factory Delivery Packages", FactoryDeliveryPackages);

	GangstarLocalDebugProcurementPanel::DrawDroppedPackage(*Context.ProcurementComponent, "Dropped Package", TEXT("ClientDroppedPackage"), "No dropped package.");
	GangstarLocalDebugProcurementPanel::DrawDroppedPackage(*Context.ProcurementComponent, "Motorhome Dropped Package", TEXT("ClientMotorhomeDroppedPackage"), "No motorhome dropped package.");
	GangstarLocalDebugProcurementPanel::DrawCustomerTrades(*Context.ProcurementComponent);
#else
	(void)Context;
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}
