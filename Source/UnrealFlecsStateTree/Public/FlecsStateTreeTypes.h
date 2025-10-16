#pragma once

#include "CoreMinimal.h"
#include "StateTreeTypes.h"
#include "StateTreeInstanceData.h"
#include "Entities/FlecsEntityHandle.h"
#include "Templates/Function.h"

#include "FlecsStateTreeTypes.generated.h"

class UFlecsWorld;
class UStateTree;

/** Configuration component attaching a StateTree asset to an ECS entity. */
USTRUCT(BlueprintType)
struct UNREALFLECSSTATETREE_API FFlecsStateTreeConfigComponent
{
    GENERATED_BODY()

public:
    /** StateTree asset that should drive this entity. */
    UPROPERTY(EditAnywhere, Category = "StateTree")
    TObjectPtr<const UStateTree> StateTree = nullptr;

    /** Automatically start the StateTree when the subsystem first observes the entity. */
    UPROPERTY(EditAnywhere, Category = "StateTree")
    bool bAutoStart = true;

    /** Restart automatically when the tree finishes (Succeeded/Failed). */
    UPROPERTY(EditAnywhere, Category = "StateTree")
    bool bAutoRestart = false;

    /** Optional tick interval override in seconds. 0 uses the fixed-step interval from the Flecs game loop. */
    UPROPERTY(EditAnywhere, Category = "StateTree", meta = (ClampMin = "0.0"))
    float TickIntervalOverride = 0.0f;
};

/** Runtime bookkeeping the subsystem maintains per entity. */
USTRUCT(BlueprintType)
struct UNREALFLECSSTATETREE_API FFlecsStateTreeRuntimeComponent
{
    GENERATED_BODY()

public:
    FFlecsStateTreeRuntimeComponent();

    void ResetForAsset(const UStateTree* InAsset, bool bStartOnInitialize);

    /** Asset currently initialised in InstanceData (may be null if unassigned). */
    UPROPERTY(VisibleAnywhere, Category = "StateTree")
    TObjectPtr<const UStateTree> ActiveAsset = nullptr;

    /** True while the last tick returned Running. */
    UPROPERTY(VisibleAnywhere, Category = "StateTree")
    bool bIsRunning = false;

    /** True when the subsystem should call Start() on the next tick. */
    UPROPERTY(VisibleAnywhere, Category = "StateTree")
    bool bPendingStart = false;

    /** Most recent run status returned from Start/ Tick. */
    UPROPERTY(VisibleAnywhere, Category = "StateTree")
    EStateTreeRunStatus LastRunStatus = EStateTreeRunStatus::Stopped;

    /** Seconds since engine start when the last tick completed. */
    UPROPERTY(VisibleAnywhere, Category = "StateTree")
    double LastTickTimeSeconds = 0.0;

    /** Mutable instance data buffer shared with the execution context. */
    FStateTreeInstanceData InstanceData;
};

/** External data exposed to StateTree tasks so they can interact with the Flecs world safely. */
USTRUCT(BlueprintType)
struct UNREALFLECSSTATETREE_API FFlecsStateTreeBindingContext
{
    GENERATED_BODY()

public:
    FFlecsStateTreeBindingContext() = default;
    FFlecsStateTreeBindingContext(UFlecsWorld* InFlecsWorld, const FFlecsEntityHandle& InEntity, float InDeltaSeconds);

    /** Entity being controlled by the StateTree. */
    UPROPERTY(VisibleAnywhere, Category = "StateTree")
    FFlecsEntityHandle Entity;

    /** Owning Flecs world. */
    UPROPERTY()
    TWeakObjectPtr<UFlecsWorld> FlecsWorld;

    /** Fixed-step delta time used for this tick. */
    UPROPERTY(VisibleAnywhere, Category = "StateTree")
    float FixedDeltaSeconds = 0.0f;

    /** @return true if the entity and world are valid. */
    bool IsValid() const;

    template <typename TComponent>
    bool TryReadComponent(TComponent& OutComponent) const
    {
        if (!IsValid())
        {
            return false;
        }

        if (const TComponent* Data = Entity.TryGet<TComponent>())
        {
            OutComponent = *Data;
            return true;
        }

        return false;
    }

    template <typename TComponent>
    bool WriteComponent(const TComponent& InComponent) const
    {
        if (!IsValid())
        {
            return false;
        }

        Entity.GetEntity().set<TComponent>(InComponent);
        return true;
    }

    template <typename TComponent, typename TCallable>
    bool ModifyComponent(TCallable&& Callable) const
    {
        if (!IsValid())
        {
            return false;
        }

        TComponent Current;
        if (!TryReadComponent(Current))
        {
            return false;
        }

        Callable(Current);
        return WriteComponent(Current);
    }

    bool Enqueue(const TFunctionRef<void(const FFlecsEntityHandle&)>& Writer) const;
};
