﻿// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if !UE_ENABLE_ICU
#include "Text.h"
#include "TextData.h"

bool FText::IsWhitespace( const TCHAR Char )
{
	return FChar::IsWhitespace(Char);
}

FText FText::AsDate( const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle, const FString& TimeZone, const FCulturePtr& TargetCulture )
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	return FText::FromString( DateTime.ToString( TEXT("%Y.%m.%d") ) );
}

FText FText::AsTime( const FDateTime& DateTime, const EDateTimeStyle::Type TimeStyle, const FString& TimeZone, const FCulturePtr& TargetCulture )
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	return FText::FromString( DateTime.ToString( TEXT("%H.%M.%S") ) );
}

FText FText::AsTimespan( const FTimespan& Timespan, const FCulturePtr& TargetCulture)
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	FDateTime DateTime(Timespan.GetTicks());
	return FText::FromString( DateTime.ToString( TEXT("%H.%M.%S") ) );
}

FText FText::AsDateTime( const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle, const EDateTimeStyle::Type TimeStyle, const FString& TimeZone, const FCulturePtr& TargetCulture )
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	return FText::FromString(DateTime.ToString(TEXT("%Y.%m.%d-%H.%M.%S")));
}

FText FText::AsMemory( SIZE_T NumBytes, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture )
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	FFormatNamedArguments Args;

	if (NumBytes < 1024)
	{
		Args.Add( TEXT("Number"), FText::AsNumber( (uint64)NumBytes, Options, TargetCulture) );
		Args.Add( TEXT("Unit"), FText::FromString( FString( TEXT("B") ) ) );
		return FText::Format( NSLOCTEXT("Internationalization", "ComputerMemoryFormatting", "{Number} {Unit}"), Args );
	}

	static const TCHAR* Prefixes = TEXT("kMGTPEZY");
	int32 Prefix = 0;

	for (; NumBytes > 1024 * 1024; NumBytes >>= 10)
	{
		++Prefix;
	}

	const double MemorySizeAsDouble = (double)NumBytes / 1024.0;
	Args.Add( TEXT("Number"), FText::AsNumber( MemorySizeAsDouble, Options, TargetCulture) );
	Args.Add( TEXT("Unit"), FText::FromString( FString( 1, &Prefixes[Prefix] ) + TEXT("B") ) );
	return FText::Format( NSLOCTEXT("Internationalization", "ComputerMemoryFormatting", "{Number} {Unit}"), Args);
}

int32 FText::CompareTo( const FText& Other, const ETextComparisonLevel::Type ComparisonLevel ) const
{
	return FCString::Strcmp( *TextData->GetDisplayString(), *Other.TextData->GetDisplayString() );
}

int32 FText::CompareToCaseIgnored( const FText& Other ) const
{
	return FCString::Stricmp( *TextData->GetDisplayString(), *Other.TextData->GetDisplayString() );
}

bool FText::EqualTo( const FText& Other, const ETextComparisonLevel::Type ComparisonLevel ) const
{
	return FCString::Strcmp( *TextData->GetDisplayString(), *Other.TextData->GetDisplayString() ) == 0;
}

bool FText::EqualToCaseIgnored( const FText& Other ) const
{
	return CompareToCaseIgnored( Other ) == 0;
}

FText::FSortPredicate::FSortPredicate(const ETextComparisonLevel::Type ComparisonLevel)
{

}

bool FText::FSortPredicate::operator()(const FText& A, const FText& B) const
{
	return A.ToString() < B.ToString();
}

bool FText::IsLetter( const TCHAR Char )
{
	return (Char>='A' && Char<='Z') || (Char>='a' && Char<='z');
}

namespace TextBiDi
{

namespace Internal
{

class FLegacyTextBiDi : public ITextBiDi
{
public:
	virtual ETextDirection ComputeTextDirection(const FText& InText) override
	{
		return FLegacyTextBiDi::ComputeTextDirection(InText.ToString());
	}

	virtual ETextDirection ComputeTextDirection(const FString& InString) override
	{
		return FLegacyTextBiDi::ComputeTextDirection(*InString, 0, InString.Len());
	}

	virtual ETextDirection ComputeTextDirection(const TCHAR*, const int32 InStringStartIndex, const int32 InStringLen) override
	{
		return ETextDirection::LeftToRight;
	}

	virtual ETextDirection ComputeTextDirection(const FText& InText, TArray<FTextDirectionInfo>& OutTextDirectionInfo) override
	{
		return FLegacyTextBiDi::ComputeTextDirection(InText.ToString(), OutTextDirectionInfo);
	}

	virtual ETextDirection ComputeTextDirection(const FString& InString, TArray<FTextDirectionInfo>& OutTextDirectionInfo) override
	{
		return FLegacyTextBiDi::ComputeTextDirection(*InString, 0, InString.Len(), OutTextDirectionInfo);
	}

	virtual ETextDirection ComputeTextDirection(const TCHAR*, const int32 InStringStartIndex, const int32 InStringLen, TArray<FTextDirectionInfo>& OutTextDirectionInfo) override
	{
		OutTextDirectionInfo.Reset();

		if (InStringLen > 0)
		{
			FTextDirectionInfo TextDirectionInfo;
			TextDirectionInfo.StartIndex = InStringStartIndex;
			TextDirectionInfo.Length = InStringLen;
			TextDirectionInfo.TextDirection = ETextDirection::LeftToRight;
			OutTextDirectionInfo.Add(MoveTemp(TextDirectionInfo));
		}

		return ETextDirection::LeftToRight;
	}

	virtual ETextDirection ComputeBaseDirection(const FText& InText) override
	{
		return FLegacyTextBiDi::ComputeBaseDirection(InText.ToString());
	}

	virtual ETextDirection ComputeBaseDirection(const FString& InString) override
	{
		return FLegacyTextBiDi::ComputeBaseDirection(*InString, 0, InString.Len());
	}

	virtual ETextDirection ComputeBaseDirection(const TCHAR*, const int32 InStringStartIndex, const int32 InStringLen) override
	{
		return ETextDirection::LeftToRight;
	}
};

} // namespace Internal

TUniquePtr<ITextBiDi> CreateTextBiDi()
{
	return MakeUnique<Internal::FLegacyTextBiDi>();
}

ETextDirection ComputeTextDirection(const FText& InText)
{
	return ComputeTextDirection(InText.ToString());
}

ETextDirection ComputeTextDirection(const FString& InString)
{
	return ComputeTextDirection(*InString, 0, InString.Len());
}

ETextDirection ComputeTextDirection(const TCHAR* InString, const int32 InStringStartIndex, const int32 InStringLen)
{
	return ETextDirection::LeftToRight;
}

ETextDirection ComputeTextDirection(const FText& InText, TArray<FTextDirectionInfo>& OutTextDirectionInfo)
{
	return ComputeTextDirection(InText.ToString(), OutTextDirectionInfo);
}

ETextDirection ComputeTextDirection(const FString& InString, TArray<FTextDirectionInfo>& OutTextDirectionInfo)
{
	return ComputeTextDirection(*InString, 0, InString.Len(), OutTextDirectionInfo);
}

ETextDirection ComputeTextDirection(const TCHAR* InString, const int32 InStringStartIndex, const int32 InStringLen, TArray<FTextDirectionInfo>& OutTextDirectionInfo)
{
	OutTextDirectionInfo.Reset();

	if (InStringLen > 0)
	{
		FTextDirectionInfo TextDirectionInfo;
		TextDirectionInfo.StartIndex = InStringStartIndex;
		TextDirectionInfo.Length = InStringLen;
		TextDirectionInfo.TextDirection = ETextDirection::LeftToRight;
		OutTextDirectionInfo.Add(MoveTemp(TextDirectionInfo));
	}

	return ETextDirection::LeftToRight;
}

ETextDirection ComputeBaseDirection(const FText& InText)
{
	return ComputeBaseDirection(InText.ToString());
}

ETextDirection ComputeBaseDirection(const FString& InString)
{
	return ComputeBaseDirection(*InString, 0, InString.Len());
}

ETextDirection ComputeBaseDirection(const TCHAR* InString, const int32 InStringStartIndex, const int32 InStringLen)
{
	return ETextDirection::LeftToRight;
}

} // namespace TextBiDi

#endif
