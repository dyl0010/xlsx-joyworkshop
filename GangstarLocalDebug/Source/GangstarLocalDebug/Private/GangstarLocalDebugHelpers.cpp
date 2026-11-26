#include "GangstarLocalDebugHelpers.h"

#include "GSTMCommonDefines/GSTMDateTimeTools.h"

#if WITH_GANGSTAR_LOCAL_IMGUI
#include <imgui.h>
#endif // WITH_GANGSTAR_LOCAL_IMGUI

namespace GangstarLocalDebugHelpers
{
	FString FormatGuid(const FGuid& Value)
	{
		return Value.IsValid() ? Value.ToString(EGuidFormats::DigitsWithHyphens) : TEXT("None");
	}

	FString FormatName(const FName& Value)
	{
		return Value.IsNone() ? TEXT("None") : Value.ToString();
	}

	FString FormatText(const FText& Value)
	{
		return Value.IsEmpty() ? TEXT("") : Value.ToString();
	}

	FString FormatVector(const FVector& Value)
	{
		return FString::Printf(TEXT("%.1f, %.1f, %.1f"), Value.X, Value.Y, Value.Z);
	}

	FString FormatUnixTime(int64 UnixTime)
	{
		if (UnixTime <= 0)
		{
			return TEXT("N/A");
		}

		return FDateTime::FromUnixTimestamp(UnixTime + GSTMDateTimeTools::ServerTimeZoneSeconds).ToString(TEXT("%Y-%m-%d %H:%M:%S"));
	}

	FString FormatBool(bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	void DrawCopyableText(const FString& Text)
	{
#if WITH_GANGSTAR_LOCAL_IMGUI
		const FTCHARToUTF8 Utf8Text(*Text);
		ImGui::TextUnformatted(Utf8Text.Get());
		if (ImGui::IsItemClicked())
		{
			ImGui::SetClipboardText(Utf8Text.Get());
		}
#else
		(void)Text;
#endif // WITH_GANGSTAR_LOCAL_IMGUI
	}
}
