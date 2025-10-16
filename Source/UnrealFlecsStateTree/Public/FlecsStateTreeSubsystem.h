#pragma once

#include "Worlds/FlecsAbstractWorldSubsystem.h"
#include "Containers/Ticker.h"

#include "FlecsStateTreeSubsystem.generated.h"

class UFlecsWorld;
class UStateTree;
class UZZFlecsGameLoop;

struct FFlecsStateTreeConfigComponent;
struct FFlecsStateTreeRuntimeComponent;

namespace flecs
{
    struct entity;
    struct world;
}

/** World subsystem responsible for ticking StateTree instances inside the Flecs fixed-step loop. */
UCLASS()
class UNREALFLECSSTATETREE_API UFlecsStateTreeSubsystem : public UFlecsAbstractWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
    bool HandleRegistrationTicker(float DeltaTime);
    void EnsureSystemsRegistered();
    void UnregisterSystems();

    void ExecuteStateTree(UFlecsWorld* FlecsWorld, flecs::entity Entity, FFlecsStateTreeConfigComponent& Config, FFlecsStateTreeRuntimeComponent& Runtime) const;
    float ResolveTickInterval(const FFlecsStateTreeConfigComponent& Config) const;

private:
    FTSTicker::FDelegateHandle RegistrationTickerHandle;
    bool bSystemsRegistered = false;
    double FixedStepInterval = 1.0 / 60.0;

    mutable bool bLoggedMissingGameLoop = false;
    mutable bool bLoggedInvalidAsset = false;
    mutable bool bLoggedMissingPhase = false;
};
