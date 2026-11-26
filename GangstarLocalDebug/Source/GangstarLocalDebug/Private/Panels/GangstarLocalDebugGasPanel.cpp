#include "Panels/GangstarLocalDebugGasPanel.h"

#include "GangstarLocalDebugContext.h"
#include "GangstarLocalDebugHelpers.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
#include "AbilitySystem/Buff/BuffComponent.h"
#include "AbilitySystem/Buff/BuffInterface.h"
#include "AbilitySystem/Buff/BuffStructs.h"
#include "AbilitySystemComponent.h"
#include "Characters/BasePlayerState.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include <imgui.h>
#endif // WITH_GANGSTAR_LOCAL_IMGUI

namespace GangstarLocalDebugGasPanel
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	struct FTrackedGameplayTagDebugItem
	{
		const char* Label = nullptr;
		FGameplayTag Tag;
	};

	static UAbilitySystemComponent* GetPlayerAbilitySystemComponent(const FGangstarLocalDebugContext& Context)
	{
		return Context.PlayerState ? Context.PlayerState->GetAbilitySystemComponent() : nullptr;
	}

	static void DrawAbilitySystemSummary(const FGangstarLocalDebugContext& Context, const UAbilitySystemComponent* ASC)
	{
		ImGui::TextUnformatted("AbilitySystem Summary");
		ImGui::Separator();
		ImGui::Text("PlayerState: %s", Context.PlayerState ? TCHAR_TO_UTF8(*Context.PlayerState->GetName()) : "None");
		ImGui::Text("ASC: %s", ASC ? TCHAR_TO_UTF8(*ASC->GetName()) : "None");
		ImGui::Text("OwnerActor: %s", ASC && ASC->GetOwnerActor() ? TCHAR_TO_UTF8(*ASC->GetOwnerActor()->GetName()) : "None");
		ImGui::Text("AvatarActor: %s", ASC && ASC->GetAvatarActor() ? TCHAR_TO_UTF8(*ASC->GetAvatarActor()->GetName()) : "None");
		ImGui::TextUnformatted("Note: This panel shows local client ASC tags. Server RPC validates server-side PlayerState ASC.");
	}

	static void DrawTrackedTags(const UAbilitySystemComponent* ASC)
	{
		ImGui::Separator();
		ImGui::TextUnformatted("Important Tags");

		if (!ASC)
		{
			ImGui::TextUnformatted("AbilitySystemComponent not found.");
			return;
		}

		const FTrackedGameplayTagDebugItem TrackedTags[] =
		{
			{ "Container Transfer Required Tag", FGameplayTag::RequestGameplayTag(TEXT("Ability.Collect"), false) },
		};

		ImGui::Columns(3, "GasTrackedTagColumns", true);
		ImGui::TextUnformatted("Label"); ImGui::NextColumn();
		ImGui::TextUnformatted("Tag"); ImGui::NextColumn();
		ImGui::TextUnformatted("Matched"); ImGui::NextColumn();
		ImGui::Separator();

		for (const FTrackedGameplayTagDebugItem& Item : TrackedTags)
		{
			const FString TagText = Item.Tag.IsValid() ? Item.Tag.ToString() : TEXT("Invalid");
			const bool bMatched = Item.Tag.IsValid() && ASC->HasMatchingGameplayTag(Item.Tag);

			ImGui::TextUnformatted(Item.Label ? Item.Label : ""); ImGui::NextColumn();
			GangstarLocalDebugHelpers::DrawCopyableText(TagText); ImGui::NextColumn();
			ImGui::TextUnformatted(bMatched ? "true" : "false"); ImGui::NextColumn();
		}
		ImGui::Columns(1);
	}

	static bool PassesTagFilter(const FString& TagText, const char* Filter)
	{
		if (!Filter || Filter[0] == '\0')
		{
			return true;
		}

		const FString FilterText = UTF8_TO_TCHAR(Filter);
		return TagText.Contains(FilterText, ESearchCase::IgnoreCase);
	}

	static void DrawOwnedTags(const UAbilitySystemComponent* ASC)
	{
		ImGui::Separator();
		ImGui::TextUnformatted("Owned GameplayTags");

		if (!ASC)
		{
			ImGui::TextUnformatted("AbilitySystemComponent not found.");
			return;
		}

		static char TagFilter[128] = "";
		ImGui::PushItemWidth(260.0f);
		ImGui::InputText("Filter##GasOwnedTagFilter", TagFilter, UE_ARRAY_COUNT(TagFilter));
		ImGui::PopItemWidth();

		FGameplayTagContainer OwnedTags;
		ASC->GetOwnedGameplayTags(OwnedTags);

		TArray<FGameplayTag> TagArray;
		OwnedTags.GetGameplayTagArray(TagArray);
		TagArray.Sort([](const FGameplayTag& Left, const FGameplayTag& Right)
		{
			return Left.ToString() < Right.ToString();
		});

		int32 VisibleCount = 0;
		for (const FGameplayTag& Tag : TagArray)
		{
			if (PassesTagFilter(Tag.ToString(), TagFilter))
			{
				++VisibleCount;
			}
		}

		ImGui::Text("Owned Tag Count: %d | Visible: %d", TagArray.Num(), VisibleCount);
		if (VisibleCount <= 0)
		{
			ImGui::TextUnformatted("No matching tags.");
			return;
		}

		if (ImGui::BeginChild("GasOwnedTagsChild", ImVec2(0.0f, 360.0f), true))
		{
			for (const FGameplayTag& Tag : TagArray)
			{
				const FString TagText = Tag.ToString();
				if (!PassesTagFilter(TagText, TagFilter))
				{
					continue;
				}

				GangstarLocalDebugHelpers::DrawCopyableText(TagText);
			}
		}
		ImGui::EndChild();
	}

	static bool PassesBuffFilter(const FBuffSpec& BuffSpec, const FString& OwnerName, const char* Filter)
	{
		if (!Filter || Filter[0] == '\0')
		{
			return true;
		}

		const FString FilterText = UTF8_TO_TCHAR(Filter);
		return BuffSpec.BuffID.ToString().Contains(FilterText, ESearchCase::IgnoreCase)
			|| BuffSpec.BuffName.ToString().Contains(FilterText, ESearchCase::IgnoreCase)
			|| OwnerName.Contains(FilterText, ESearchCase::IgnoreCase);
	}

	static float GetBuffRemainingTime(const UWorld* World, const FBuffSpec& BuffSpec)
	{
		if (!World || BuffSpec.DurationPolicy != EBuffDurationPolicy::HasDuration || BuffSpec.Duration <= 0.0f)
		{
			return BuffSpec.DurationPolicy == EBuffDurationPolicy::Infinite ? -1.0f : 0.0f;
		}

		return FMath::Max(0.0f, BuffSpec.StartWorldTime + BuffSpec.Duration - World->GetTimeSeconds());
	}

	static void DrawProjectBuffs(const UAbilitySystemComponent* ASC)
	{
		ImGui::Separator();
		ImGui::TextUnformatted("Project Buffs");

		const IBuffInterface* BuffInterface = Cast<IBuffInterface>(ASC);
		UBuffComponent* BuffComponent = BuffInterface ? BuffInterface->GetBuffComponent() : nullptr;
		if (!BuffComponent)
		{
			ImGui::TextUnformatted("BuffComponent not found.");
			return;
		}

		static char BuffFilter[128] = "";
		ImGui::PushItemWidth(260.0f);
		ImGui::InputText("Filter##GasProjectBuffFilter", BuffFilter, UE_ARRAY_COUNT(BuffFilter));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::TextDisabled("BuffID / BuffName / Owner");

		struct FVisibleBuffDebugItem
		{
			AActor* Owner = nullptr;
			FBuffSpec Spec;
		};

		TArray<FVisibleBuffDebugItem> VisibleBuffs;
		int32 TotalCount = 0;
		BuffComponent->ForEachActiveBuff([&VisibleBuffs, &TotalCount](AActor* Owner, const FBuffSpec& Spec)
		{
			++TotalCount;
			const FString OwnerName = Owner ? Owner->GetName() : TEXT("None");
			if (PassesBuffFilter(Spec, OwnerName, BuffFilter))
			{
				FVisibleBuffDebugItem& Item = VisibleBuffs.AddDefaulted_GetRef();
				Item.Owner = Owner;
				Item.Spec = Spec;
			}
			return true;
		});

		VisibleBuffs.Sort([](const FVisibleBuffDebugItem& Left, const FVisibleBuffDebugItem& Right)
		{
			return Left.Spec.BuffID.ToString() < Right.Spec.BuffID.ToString();
		});

		ImGui::Text("Active Buff Count: %d | Visible: %d", TotalCount, VisibleBuffs.Num());
		if (VisibleBuffs.Num() <= 0)
		{
			ImGui::TextUnformatted("No matching buffs.");
			return;
		}

		if (ImGui::BeginChild("GasProjectBuffsChild", ImVec2(0.0f, 420.0f), true))
		{
			ImGui::Columns(9, "GasProjectBuffColumns", true);
			ImGui::TextUnformatted("BuffID"); ImGui::NextColumn();
			ImGui::TextUnformatted("Name"); ImGui::NextColumn();
			ImGui::TextUnformatted("Handle"); ImGui::NextColumn();
			ImGui::TextUnformatted("Stack"); ImGui::NextColumn();
			ImGui::TextUnformatted("Max"); ImGui::NextColumn();
			ImGui::TextUnformatted("Duration"); ImGui::NextColumn();
			ImGui::TextUnformatted("Remain"); ImGui::NextColumn();
			ImGui::TextUnformatted("Policy"); ImGui::NextColumn();
			ImGui::TextUnformatted("Owner"); ImGui::NextColumn();
			ImGui::Separator();

			const UWorld* World = ASC ? ASC->GetWorld() : nullptr;
			for (const FVisibleBuffDebugItem& Item : VisibleBuffs)
			{
				const FBuffSpec& Spec = Item.Spec;
				const float RemainingTime = GetBuffRemainingTime(World, Spec);
				GangstarLocalDebugHelpers::DrawCopyableText(Spec.BuffID.ToString()); ImGui::NextColumn();
				GangstarLocalDebugHelpers::DrawCopyableText(Spec.BuffName.ToString()); ImGui::NextColumn();
				GangstarLocalDebugHelpers::DrawCopyableText(Spec.BuffHandle.ToString()); ImGui::NextColumn();
				ImGui::Text("%d", Spec.StackCount); ImGui::NextColumn();
				ImGui::Text("%d", Spec.MaxStack); ImGui::NextColumn();
				ImGui::Text("%.2f", Spec.Duration); ImGui::NextColumn();
				if (Spec.DurationPolicy == EBuffDurationPolicy::Infinite)
				{
					ImGui::TextUnformatted("Infinite");
				}
				else
				{
					ImGui::Text("%.2f", RemainingTime);
				}
				ImGui::NextColumn();
				GangstarLocalDebugHelpers::DrawCopyableText(GangstarLocalDebugHelpers::FormatEnum(Spec.DurationPolicy)); ImGui::NextColumn();
				GangstarLocalDebugHelpers::DrawCopyableText(Item.Owner ? Item.Owner->GetName() : TEXT("None")); ImGui::NextColumn();
			}
			ImGui::Columns(1);
		}
		ImGui::EndChild();
	}
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}

void FGangstarLocalDebugGasPanel::Draw(const FGangstarLocalDebugContext& Context)
{
#if WITH_GANGSTAR_LOCAL_IMGUI
	UAbilitySystemComponent* ASC = GangstarLocalDebugGasPanel::GetPlayerAbilitySystemComponent(Context);
	GangstarLocalDebugGasPanel::DrawAbilitySystemSummary(Context, ASC);
	GangstarLocalDebugGasPanel::DrawTrackedTags(ASC);
	GangstarLocalDebugGasPanel::DrawOwnedTags(ASC);
	GangstarLocalDebugGasPanel::DrawProjectBuffs(ASC);
#else
	(void)Context;
#endif // WITH_GANGSTAR_LOCAL_IMGUI
}
