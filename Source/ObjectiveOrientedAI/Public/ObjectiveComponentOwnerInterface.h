// © Volga Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ObjectiveComponentOwnerInterface.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UObjectiveComponentOwnerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class IObjectiveComponentOwnerInterface
{
	GENERATED_BODY()

public:
	
	virtual class UObjectiveComponent* GetObjectiveComponent() const = 0;
};
