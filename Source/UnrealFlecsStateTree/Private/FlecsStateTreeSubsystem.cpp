#include "FlecsStateTreeSubsystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(FlecsStateTreeSubsystem)

#include "FlecsStateTreeTypes.h"
#include "FlecsStateTreeGameLoopExtension.h"
#include "Worlds/FlecsWorld.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Logging/LogMacros.h"
#include "HAL/PlatformTime.h"

#include "flecs.h"

DEFINE_LOG_CATEGORY_STATIC(LogFlecsStateTreeSubsystem, Log, All);

namespace
{
    constexpr const TCHAR* StateTreeSystemName = TEXT("ZZ.Movement.StateTree.Step");
}

void UFlecsStateTreeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FixedStepInterval = 1.0 / 60.0;
    RegistrationTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UFlecsStateTreeSubsystem::HandleRegistrationTicker));
}

void UFlecsStateTreeSubsystem::Deinitialize()
{
    UnregisterSystems();

    if (RegistrationTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(RegistrationTickerHandle);
        RegistrationTickerHandle.Reset();
    }

    Super::Deinitialize();
}

void UFlecsStateTreeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    EnsureSystemsRegistered();
}

bool UFlecsStateTreeSubsystem::HandleRegistrationTicker(const float DeltaTime)
{
    if (!bSystemsRegistered)
    {
        EnsureSystemsRegistered();
    }

    return !bSystemsRegistered;
}

void UFlecsStateTreeSubsystem::EnsureSystemsRegistered()
{
    if (bSystemsRegistered)
    {
        return;
    }

    UFlecsWorld* const FlecsWorldPtr = GetFlecsWorld();
    if (!IsValid(FlecsWorldPtr))
    {
        return;
    }

    UObject* GameLoopObject = FlecsWorldPtr->GameLoopInterface.GetObject();
    IFlecsStateTreeGameLoopExtension* StateTreeLoop = Cast<IFlecsStateTreeGameLoopExtension>(GameLoopObject);
    if (!StateTreeLoop)
    {
        if (!bLoggedMissingGameLoop)
        {
            UE_LOG(LogFlecsStateTreeSubsystem, Verbose, TEXT("Awaiting game loop implementation of IFlecsStateTreeGameLoopExtension before registering StateTree systems."));
            bLoggedMissingGameLoop = true;
        }
        return;
    }

    bLoggedMissingGameLoop = false;

    const FFlecsEntityHandle TickPhaseHandle = StateTreeLoop->GetStateTreeTickPhase();
    const flecs::id_t TickPhaseId = TickPhaseHandle.GetRawId();
    if (TickPhaseId == 0)
    {
        if (!bLoggedMissingPhase)
        {
            UE_LOG(LogFlecsStateTreeSubsystem, Verbose, TEXT("StateTree game loop extension has not produced a tick phase yet; deferring registration."));
            bLoggedMissingPhase = true;
        }
        return;
    }

    bLoggedMissingPhase = false;

    const double ExtensionInterval = StateTreeLoop->GetStateTreeFixedStepInterval();
    if (ExtensionInterval > 0.0)
    {
        FixedStepInterval = ExtensionInterval;
    }

    flecs::world& World = FlecsWorldPtr->World;

    FlecsWorldPtr->RegisterComponentType<FFlecsStateTreeConfigComponent>();
    FlecsWorldPtr->RegisterComponentType<FFlecsStateTreeRuntimeComponent>();

    const flecs::entity ExistingSystem = World.lookup(TCHAR_TO_UTF8(StateTreeSystemName));
    if (ExistingSystem.id() != 0)
    {
        bSystemsRegistered = true;
        return;
    }

    const flecs::entity TickPhase(World, TickPhaseId);

    UFlecsWorld* CapturedWorld = FlecsWorldPtr;

    World.system<FFlecsStateTreeConfigComponent, FFlecsStateTreeRuntimeComponent>(TCHAR_TO_UTF8(StateTreeSystemName))
        .kind(TickPhase)
        .each([this, CapturedWorld](flecs::entity Entity, FFlecsStateTreeConfigComponent& Config, FFlecsStateTreeRuntimeComponent& Runtime)
        {
            ExecuteStateTree(CapturedWorld, Entity, Config, Runtime);
        });

    bSystemsRegistered = World.lookup(TCHAR_TO_UTF8(StateTreeSystemName)).id() != 0;

    if (bSystemsRegistered)
    {
        UE_LOG(LogFlecsStateTreeSubsystem, Log, TEXT("Registered %s system."), StateTreeSystemName);
    }
}

void UFlecsStateTreeSubsystem::UnregisterSystems()
{
    if (UFlecsWorld* FlecsWorldPtr = GetFlecsWorld())
    {
        const flecs::entity SystemHandle = FlecsWorldPtr->World.lookup(TCHAR_TO_UTF8(StateTreeSystemName));
        if (SystemHandle.id() != 0)
        {
            SystemHandle.destruct();
        }
    }

    bSystemsRegistered = false;
}

void UFlecsStateTreeSubsystem::ExecuteStateTree(UFlecsWorld* InFlecsWorld, flecs::entity Entity, FFlecsStateTreeConfigComponent& Config, FFlecsStateTreeRuntimeComponent& Runtime) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Flecs.StateTree.Step"));

    if (!IsValid(InFlecsWorld))
    {
        return;
    }

    const UStateTree* Asset = Config.StateTree;
    if (!Asset || !Asset->IsReadyToRun())
    {
        if (Runtime.ActiveAsset != nullptr)
        {
            Runtime.ResetForAsset(nullptr, false);
        }

        if (Asset && !bLoggedInvalidAsset)
        {
            UE_LOG(LogFlecsStateTreeSubsystem, Warning, TEXT("StateTree asset %s is not linked; skipping execution."), *GetNameSafe(Asset));
            bLoggedInvalidAsset = true;
        }
        return;
    }

    bLoggedInvalidAsset = false;

    if (Runtime.ActiveAsset != Asset)
    {
        Runtime.ResetForAsset(Asset, Config.bAutoStart);
    }
    else if (!Runtime.bIsRunning && !Runtime.bPendingStart && Config.bAutoStart && Runtime.LastRunStatus == EStateTreeRunStatus::Stopped)
    {
        Runtime.bPendingStart = true;
    }

    const float DeltaSeconds = ResolveTickInterval(Config);
    Runtime.LastTickTimeSeconds = FPlatformTime::Seconds();

    FFlecsStateTreeBindingContext BindingContext(InFlecsWorld, FFlecsEntityHandle(Entity), DeltaSeconds);

    FOnCollectStateTreeExternalData CollectDelegate = FOnCollectStateTreeExternalData::CreateLambda(
        [&BindingContext](const FStateTreeExecutionContext& Context, const UStateTree* StateTree, TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs, TArrayView<FStateTreeDataView> OutDataViews)
        {
            for (int32 Index = 0; Index < ExternalDataDescs.Num(); ++Index)
            {
                const FStateTreeExternalDataDesc& Desc = ExternalDataDescs[Index];
                if (Desc.Struct == FFlecsStateTreeBindingContext::StaticStruct())
                {
                    OutDataViews[Index] = FStateTreeDataView(FFlecsStateTreeBindingContext::StaticStruct(), &BindingContext);
                }
            }
            return true;
        });

    FStateTreeExecutionContext ExecutionContext(*const_cast<UFlecsStateTreeSubsystem*>(this), *Asset, Runtime.InstanceData, CollectDelegate);
    if (!ExecutionContext.IsValid())
    {
        UE_LOG(LogFlecsStateTreeSubsystem, Warning, TEXT("Failed to build execution context for %s."), *GetNameSafe(Asset));
        return;
    }

    if (!Runtime.bIsRunning && Runtime.bPendingStart)
    {
        Runtime.bPendingStart = false;
        Runtime.LastRunStatus = ExecutionContext.Start();
        Runtime.bIsRunning = Runtime.LastRunStatus == EStateTreeRunStatus::Running;

        if (!Runtime.bIsRunning)
        {
            ExecutionContext.Stop();
            Runtime.bPendingStart = Config.bAutoRestart;
            return;
        }
    }

    if (Runtime.bIsRunning)
    {
        Runtime.LastRunStatus = ExecutionContext.Tick(DeltaSeconds);
        Runtime.bIsRunning = Runtime.LastRunStatus == EStateTreeRunStatus::Running;

        if (!Runtime.bIsRunning)
        {
            ExecutionContext.Stop();
            Runtime.bPendingStart = Config.bAutoRestart;
        }
    }
}

float UFlecsStateTreeSubsystem::ResolveTickInterval(const FFlecsStateTreeConfigComponent& Config) const
{
    return (Config.TickIntervalOverride > 0.0f)
        ? Config.TickIntervalOverride
        : static_cast<float>(FixedStepInterval);
}