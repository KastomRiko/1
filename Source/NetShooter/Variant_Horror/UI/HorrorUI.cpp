// Copyright Epic Games, Inc. All Rights Reserved.


#include "HorrorUI.h"
#include "HorrorCharacter.h"

void UHorrorUI::SetupCharacter(AHorrorCharacter* HorrorCharacter)
{
	if (BoundCharacter.Get() == HorrorCharacter)
	{
		return;
	}

	if (AHorrorCharacter* PreviousCharacter = BoundCharacter.Get())
	{
		PreviousCharacter->OnSprintMeterUpdated.RemoveDynamic(this, &UHorrorUI::OnSprintMeterUpdated);
		PreviousCharacter->OnSprintStateChanged.RemoveDynamic(this, &UHorrorUI::OnSprintStateChanged);
	}

	BoundCharacter = HorrorCharacter;

	if (IsValid(HorrorCharacter))
	{
		HorrorCharacter->OnSprintMeterUpdated.AddUniqueDynamic(this, &UHorrorUI::OnSprintMeterUpdated);
		HorrorCharacter->OnSprintStateChanged.AddUniqueDynamic(this, &UHorrorUI::OnSprintStateChanged);
	}
}

void UHorrorUI::OnSprintMeterUpdated(float Percent)
{
	// call the BP handler
	BP_SprintMeterUpdated(Percent);
}

void UHorrorUI::OnSprintStateChanged(bool bSprinting)
{
	// call the BP handler
	BP_SprintStateChanged(bSprinting);
}
