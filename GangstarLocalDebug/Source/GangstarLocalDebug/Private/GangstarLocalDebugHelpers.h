#pragma once

#include "CoreMinimal.h"

namespace GangstarLocalDebugHelpers
{
	FString FormatGuid(const FGuid& Value);
	FString FormatName(const FName& Value);
	FString FormatText(const FText& Value);
	FString FormatVector(const FVector& Value);
	FString FormatUnixTime(int64 UnixTime);
	FString FormatBool(bool bValue);
	void DrawCopyableText(const FString& Text);

	template <typename EnumType>
	FString FormatEnum(EnumType Value)
	{
		const UEnum* Enum = StaticEnum<EnumType>();
		return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Value)) : FString::Printf(TEXT("%d"), static_cast<int32>(Value));
	}
}
