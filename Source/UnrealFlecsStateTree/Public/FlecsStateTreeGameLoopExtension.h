#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Entities/FlecsEntityHandle.h"

#include "FlecsStateTreeGameLoopExtension.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UFlecsStateTreeGameLoopExtension : public UInterface
{
    GENERATED_BODY()
};

/** Interface implemented by game-loop objects that expose a dedicated StateTree tick phase. */
class UNREALFLECSSTATETREE_API IFlecsStateTreeGameLoopExtension
{
    GENERATED_BODY()

public:
    /** Return the Flecs phase entity the subsystem should bind its StateTree system to. */
    virtual FFlecsEntityHandle GetStateTreeTickPhase() const = 0;

    /** Provide the fixed-step interval (seconds) that should drive each StateTree tick. */
    virtual double GetStateTreeFixedStepInterval() const = 0;
};