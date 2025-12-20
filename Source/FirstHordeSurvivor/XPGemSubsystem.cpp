#include "XPGemSubsystem.h"
#include "XPGemVisualConfig.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

void UXPGemSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    InitializeDefaultVisuals();
}

void UXPGemSubsystem::InitializeDefaultVisuals()
{
    // Load shared assets
    UStaticMesh* DebrisA = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/VFX_Destruction/Meshes/SM_Derbis_A.SM_Derbis_A"));
    UStaticMesh* DebrisB = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/VFX_Destruction/Meshes/SM_Derbis_B.SM_Derbis_B"));
    UStaticMesh* DebrisC = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/VFX_Destruction/Meshes/SM_Derbis_C.SM_Derbis_C"));
    UMaterialInterface* GemMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_XPGem.M_XPGem"));

    // 1 XP - Small gray gem
    FXPGemData Gem1;
    Gem1.Mesh = DebrisC;
    Gem1.Material = GemMaterial;
    Gem1.Scale = 5.0f;
    Gem1.Color = FLinearColor(0.16f, 0.16f, 0.16f);
    Gem1.EmissiveStrength = 15.0f;
    DefaultVisuals.Add(1, Gem1);

    // 5 XP - White gem
    FXPGemData Gem5;
    Gem5.Mesh = DebrisB;
    Gem5.Material = GemMaterial;
    Gem5.Scale = 5.0f;
    Gem5.Color = FLinearColor(2.0f, 2.0f, 2.0f);
    Gem5.EmissiveStrength = 20.0f;
    DefaultVisuals.Add(5, Gem5);

    // 20 XP - Yellow gem
    FXPGemData Gem20;
    Gem20.Mesh = DebrisB;
    Gem20.Material = GemMaterial;
    Gem20.Scale = 5.0f;
    Gem20.Color = FLinearColor(1.0f, 0.96f, 0.0f);
    Gem20.EmissiveStrength = 30.0f;
    DefaultVisuals.Add(20, Gem20);

    // 50 XP - Blue gem (bigger chunk)
    FXPGemData Gem50;
    Gem50.Mesh = DebrisA;
    Gem50.Material = GemMaterial;
    Gem50.Scale = 5.0f;
    Gem50.Color = FLinearColor(0.1f, 0.4f, 1.0f);
    Gem50.EmissiveStrength = 40.0f;
    DefaultVisuals.Add(50, Gem50);

    // 100 XP - Red gem (biggest chunk)
    FXPGemData Gem100;
    Gem100.Mesh = DebrisA;
    Gem100.Material = GemMaterial;
    Gem100.Scale = 5.0f;
    Gem100.Color = FLinearColor(1.0f, 0.03f, 0.0f);
    Gem100.EmissiveStrength = 50.0f;
    DefaultVisuals.Add(100, Gem100);

    // Default fallback (green)
    DefaultFallback.Mesh = DebrisC;
    DefaultFallback.Material = GemMaterial;
    DefaultFallback.Scale = 5.0f;
    DefaultFallback.Color = FLinearColor(0.2f, 1.0f, 0.3f);
    DefaultFallback.EmissiveStrength = 20.0f;
}

FXPGemData UXPGemSubsystem::GetVisualDataForValue(int32 Value) const
{
    // Try DataAsset first
    if (VisualConfig)
    {
        return VisualConfig->GetVisualForValue(Value);
    }

    // Fall back to hardcoded defaults
    if (const FXPGemData* Found = DefaultVisuals.Find(Value))
    {
        return *Found;
    }

    return DefaultFallback;
}

void UXPGemSubsystem::Deinitialize()
{
    // Clean up if necessary, though World destruction handles actors
    Super::Deinitialize();
}

void UXPGemSubsystem::SpawnGem(FVector Location, int32 Value)
{
    AXPGem* GemToSpawn = nullptr;

    // Try to find one in the pool
    if (GemPool.Num() > 0)
    {
        GemToSpawn = GemPool.Pop();
    }
    else
    {
        // Spawn new one
        // Note: We need the GemClass. Since this is a Subsystem, we can't easily set EditDefaultsOnly properties in the Editor 
        // unless we create a Blueprint subclass of the Subsystem (not possible for WorldSubsystems directly in the same way as Actors)
        // OR we look it up from a GameInstance or GameMode.
        // For now, let's assume we can hardcode or load a default class, OR better yet:
        // The user can set the GemClass in a DeveloperSettings or we can use a soft class path.
        // Let's try to find the class or use a default if not set.
        // Actually, a common pattern is to have a "Manager" actor that configures the Subsystem, OR just use a DataAsset.
        
        // For simplicity in this iteration, let's assume we load a BP class via ConstructorHelpers or SoftClassPath if we were in a GameMode.
        // But since we are in a Subsystem, we don't have a BP to configure.
        
        // ALTERNATIVE: The Enemy calls SpawnGem, maybe we pass the class? No, that's messy.
        // ALTERNATIVE: We use a GameInstanceSubsystem which can be configured via Project Settings?
        // ALTERNATIVE: We look for a specific BP in the content folder?
        
        // Let's use a simple approach: The GameMode can register the GemClass with the Subsystem on BeginPlay.
        // OR we just spawn a C++ AXPGem if no BP is found (but then no visuals).
        
        // Let's assume the GameMode will configure us.
        // But for now, let's just spawn the base class if nothing else, or rely on the GameMode to have set it up.
        // Wait, I can't easily rely on GameMode setting it up without modifying GameMode.
        
        // Let's modify the SpawnGem to take a class if it's the first time? No.
        
        // Let's use `TSoftClassPtr` and hardcode the path to the BP for now as a fallback, 
        // or better: Create a `UXPGemSettings` (DeveloperSettings) to configure this.
        // That's the "Correct" way but maybe overkill.
        
        // Let's just spawn the native class for now and assume the user will reparent or we will fix the class loading later.
        // Actually, I'll add a function `RegisterGemClass` that the GameMode calls.
        
        if (GemClass)
        {
            GemToSpawn = GetWorld()->SpawnActor<AXPGem>(GemClass, Location, FRotator::ZeroRotator);
        }
        else
        {
            // Fallback to native class
            GemToSpawn = GetWorld()->SpawnActor<AXPGem>(AXPGem::StaticClass(), Location, FRotator::ZeroRotator);
        }
    }

    if (GemToSpawn)
    {
        GemToSpawn->Initialize(Value, Location);

        // Apply visuals (DataAsset if available, otherwise code defaults)
        FXPGemData VisualData = GetVisualDataForValue(Value);
        GemToSpawn->SetVisuals(VisualData);
    }
}

void UXPGemSubsystem::ReturnGem(AXPGem* Gem)
{
    if (Gem)
    {
        Gem->Deactivate();
        GemPool.Add(Gem);
    }
}

void UXPGemSubsystem::RegisterGemClass(TSubclassOf<AXPGem> InGemClass)
{
    if (InGemClass)
    {
        GemClass = InGemClass;
    }
}

void UXPGemSubsystem::RegisterVisualConfig(UXPGemVisualConfig* InConfig)
{
    VisualConfig = InConfig;
}
