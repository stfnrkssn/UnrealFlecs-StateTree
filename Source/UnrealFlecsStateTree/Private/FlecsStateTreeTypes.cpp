#include "FlecsStateTreeTypes.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(FlecsStateTreeTypes)

#include "StateTree.h"
#include "Worlds/FlecsWorld.h"

FFlecsStateTreeRuntimeComponent::FFlecsStateTreeRuntimeComponent()
{
    ResetForAsset(nullptr, false);
}

void FFlecsStateTreeRuntimeComponent::ResetForAsset(const UStateTree* InAsset, const bool bStartOnInitialize)
{
    ActiveAsset = InAsset;
    bIsRunning = false;
    bPendingStart = InAsset != nullptr && bStartOnInitialize;
    LastRunStatus = EStateTreeRunStatus::Stopped;
    LastTickTimeSeconds = 0.0;

    if (InAsset && InAsset->IsReadyToRun())
    {
        InstanceData = InAsset->GetDefaultInstanceData();
    }
    else
    {
        InstanceData = FStateTreeInstanceData();
    }
}

FFlecsStateTreeBindingContext::FFlecsStateTreeBindingContext(UFlecsWorld* InFlecsWorld, const FFlecsEntityHandle& InEntity, const float InDeltaSeconds)
    : Entity(InEntity)
    , FlecsWorld(InFlecsWorld)
    , FixedDeltaSeconds(InDeltaSeconds)
{
}

bool FFlecsStateTreeBindingContext::IsValid() const
{
    return Entity.IsValid() && FlecsWorld.IsValid();
}

bool FFlecsStateTreeBindingContext::Enqueue(const TFunctionRef<void(const FFlecsEntityHandle&)>& Writer) const
{
    if (!IsValid())
    {
        return false;
    }

    Writer(Entity);
    return true;
}
