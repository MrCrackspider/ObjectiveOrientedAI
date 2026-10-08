
#pragma once

#include "CoreMinimal.h"
#include "MontagePlaySettings.generated.h"

/**
 * How a montage objective plays its montage. Held by the objective itself, so these are the values the
 * details panel and the exposed on spawn pin edit.
 *
 * From C++ it is built by chaining onto a default instance, leaving anything untouched at its default:
 *
 *   UAIO_PlayMontage::MakeInstance(Montage, FMontagePlaySettings().SetSectionName(TEXT("Loop")).SetPlayRate(1.5f));
 */
USTRUCT(BlueprintType)
struct FMontagePlaySettings
{
	GENERATED_BODY()

	// Section to jump to once the montage starts, None to play from the top
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage", meta = (GetOptions = "GetMontageSections"))
	FName SectionName = NAME_None;

	// Playback speed. Must stay above zero, a frozen montage would never reach its end
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage", meta = (ClampMin = "0.01"))
	float PlayRate = 1.f;

	// Position in the montage to start from
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	float StartingPosition = 0.f;

	// Finish as soon as the montage starts blending out, instead of waiting for it to fully end
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	bool bInvalidateOnBlendOut = true;

	// Pick the montage up where the objective last stopped it, rather than starting it over
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	bool bResumeFromStopPosition = false;

	FMontagePlaySettings& SetSectionName(FName InSectionName)
	{
		SectionName = InSectionName;
		return *this;
	}

	FMontagePlaySettings& SetPlayRate(float InPlayRate)
	{
		PlayRate = InPlayRate;
		return *this;
	}

	FMontagePlaySettings& SetStartingPosition(float InStartingPosition)
	{
		StartingPosition = InStartingPosition;
		return *this;
	}

	FMontagePlaySettings& SetInvalidateOnBlendOut(bool bInInvalidateOnBlendOut)
	{
		bInvalidateOnBlendOut = bInInvalidateOnBlendOut;
		return *this;
	}

	FMontagePlaySettings& SetResumeFromStopPosition(bool bInResumeFromStopPosition)
	{
		bResumeFromStopPosition = bInResumeFromStopPosition;
		return *this;
	}
};

/**
 * Redeclares the FMontagePlaySettings setters on a struct deriving from it, each handing back the derived
 * type. Without them a chain which touches an inherited setter is left holding an FMontagePlaySettings&
 * and cannot continue into the derived setters, so the order a chain is written in would start to matter
 */
#define MONTAGE_PLAY_SETTINGS_BUILDER(StructType) \
	StructType& SetSectionName(FName InSectionName) \
	{ \
		FMontagePlaySettings::SetSectionName(InSectionName); \
		return *this; \
	} \
	StructType& SetPlayRate(float InPlayRate) \
	{ \
		FMontagePlaySettings::SetPlayRate(InPlayRate); \
		return *this; \
	} \
	StructType& SetStartingPosition(float InStartingPosition) \
	{ \
		FMontagePlaySettings::SetStartingPosition(InStartingPosition); \
		return *this; \
	} \
	StructType& SetInvalidateOnBlendOut(bool bInInvalidateOnBlendOut) \
	{ \
		FMontagePlaySettings::SetInvalidateOnBlendOut(bInInvalidateOnBlendOut); \
		return *this; \
	} \
	StructType& SetResumeFromStopPosition(bool bInResumeFromStopPosition) \
	{ \
		FMontagePlaySettings::SetResumeFromStopPosition(bInResumeFromStopPosition); \
		return *this; \
	}
