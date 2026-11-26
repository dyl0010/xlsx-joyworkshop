#pragma once

#include "CoreMinimal.h"

class AActor;
class ABasePlayerState;
class ACharacter;
class AGameModeBase;
class AGameStateBase;
class AGmHelperActor;
class APawn;
class APlayerController;
class UAbilitySystemComponent;
class UGameInstance;
class UItemContainerManager;
class ULightQuestComponent;
class UOwnerOnlyItemContainerReplicatorComponent;
class UItemContainerReplicatorComponent;
class UViceTonicsProcurementComponent;
class UWorld;

struct FGangstarLocalDebugContext
{
	UWorld* World = nullptr;
	UGameInstance* GameInstance = nullptr;
	AGameModeBase* GameMode = nullptr;
	AGameStateBase* GameState = nullptr;
	APlayerController* PlayerController = nullptr;
	APawn* Pawn = nullptr;
	ACharacter* Character = nullptr;
	ABasePlayerState* PlayerState = nullptr;
	AGmHelperActor* GmHelperActor = nullptr;
	UAbilitySystemComponent* AbilitySystemComponent = nullptr;
	ULightQuestComponent* LightQuestComponent = nullptr;
	UItemContainerManager* ItemContainerManager = nullptr;
	UOwnerOnlyItemContainerReplicatorComponent* OwnerOnlyItemContainerReplicator = nullptr;
	UViceTonicsProcurementComponent* ProcurementComponent = nullptr;
	AActor* CurrentHappyLabFactory = nullptr;
	UItemContainerReplicatorComponent* FactoryItemContainerReplicator = nullptr;

	static FGangstarLocalDebugContext Build();
};
