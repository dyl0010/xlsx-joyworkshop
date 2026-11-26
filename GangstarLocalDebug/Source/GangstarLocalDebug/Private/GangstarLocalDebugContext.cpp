#include "GangstarLocalDebugContext.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
#include "Characters/BasePlayerController.h"
#include "Characters/BasePlayerState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GM/GmHelperActor.h"
#include "ItemContainer/ItemContainerManager.h"
#include "ItemContainer/ItemContainerReplicatorComponent.h"
#include "LightQuest/LightQuestComponent.h"

namespace
{
	bool IsDeliveryPackageActor(const AActor& Actor)
	{
		for (const UClass* Class = Actor.GetClass(); Class; Class = Class->GetSuperClass())
		{
			const FName ClassName = Class->GetFName();
			if (ClassName == TEXT("RealPropertyHappyLabFactory")
				|| ClassName == TEXT("RealPropertyTearDownDynamicsCompany"))
			{
				return true;
			}
		}

		return false;
	}
}
#endif // WITH_GANGSTAR_LOCAL_IMGUI

FGangstarLocalDebugContext FGangstarLocalDebugContext::Build()
{
	FGangstarLocalDebugContext Context;

#if WITH_GANGSTAR_LOCAL_IMGUI
	if (!GEngine)
	{
		return Context;
	}

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		UWorld* CandidateWorld = WorldContext.World();
		if (!CandidateWorld || CandidateWorld->IsNetMode(NM_DedicatedServer))
		{
			continue;
		}

		if (CandidateWorld->WorldType != EWorldType::Game && CandidateWorld->WorldType != EWorldType::PIE)
		{
			continue;
		}

		Context.World = CandidateWorld;
		break;
	}

	if (!Context.World)
	{
		return Context;
	}

	Context.GameInstance = Context.World->GetGameInstance();
	Context.GameMode = Context.World->GetAuthGameMode<AGameModeBase>();
	Context.GameState = Context.World->GetGameState<AGameStateBase>();
	Context.PlayerController = Context.World->GetFirstPlayerController();
	Context.Pawn = Context.PlayerController ? Context.PlayerController->GetPawn() : nullptr;
	Context.Character = Cast<ACharacter>(Context.Pawn);
	if (ABasePlayerController* BasePlayerController = Cast<ABasePlayerController>(Context.PlayerController))
	{
		Context.GmHelperActor = BasePlayerController->GmHelperActor.Get();
	}

	Context.PlayerState = Context.PlayerController ? Context.PlayerController->GetPlayerState<ABasePlayerState>() : nullptr;
	if (Context.PlayerState)
	{
		Context.AbilitySystemComponent = Context.PlayerState->GetAbilitySystemComponent();
		Context.LightQuestComponent = Context.PlayerState->LightQuestComponent;
		Context.ItemContainerManager = Context.PlayerState->GetPlayerItemContainerManager();
		Context.OwnerOnlyItemContainerReplicator = Context.PlayerState->OwnerOnlyItemContainerReplicatorComponent;
		Context.ProcurementComponent = Context.PlayerState->ViceTonicsProcurementComponent;
	}

	for (TActorIterator<AActor> ActorIt(Context.World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (Actor && IsDeliveryPackageActor(*Actor))
		{
			Context.CurrentHappyLabFactory = Actor;
			Context.FactoryItemContainerReplicator = Actor->FindComponentByClass<UItemContainerReplicatorComponent>();
			break;
		}
	}
#endif // WITH_GANGSTAR_LOCAL_IMGUI

	return Context;
}
