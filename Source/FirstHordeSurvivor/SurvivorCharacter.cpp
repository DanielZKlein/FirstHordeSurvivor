#include "SurvivorCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivorEnemy.h"
#include "SurvivorWeapon.h"
#include "WeaponDataBase.h"
#include "UpgradeSubsystem.h"

#include "Components/StaticMeshComponent.h"

ASurvivorCharacter::ASurvivorCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	AttributeComp = CreateDefaultSubobject<UAttributeComponent>(TEXT("AttributeComp"));

	// Create a Static Mesh for the "Ball" visual since ACharacter's default Mesh is Skeletal
	PlayerVisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlayerVisualMesh"));
	PlayerVisualMesh->SetupAttachment(RootComponent);
	
	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComp"));
	SpringArmComp->SetupAttachment(RootComponent);
	SpringArmComp->SetUsingAbsoluteRotation(true); // Don't rotate arm with character
	SpringArmComp->TargetArmLength = 2000.0f;
	SpringArmComp->bUsePawnControlRotation = false;
	SpringArmComp->SetRelativeRotation(FRotator(-80.0f, 0.0f, 0.0f));
	SpringArmComp->bInheritPitch = false;
	SpringArmComp->bInheritYaw = false;
	SpringArmComp->bInheritRoll = false;
	SpringArmComp->bDoCollisionTest = false;

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(SpringArmComp);

	RollingAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("RollingAudioComp"));
	RollingAudioComp->SetupAttachment(RootComponent);
	RollingAudioComp->bAutoActivate = true;

	SpeedParameterName = "Speed";

	// Configure Character Movement for Top-Down
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 640.0f, 0.0f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	// Don't rotate camera with controller
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Load Wall Bounce Sound
	static ConstructorHelpers::FObjectFinder<USoundBase> SoundAsset(TEXT("/Game/Sound/Pool-cue-hitting-ball.Pool-cue-hitting-ball"));
	if (SoundAsset.Succeeded())
	{
		WallBounceSound = SoundAsset.Object;
	}
}

void ASurvivorCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	// Apply initial attribute values to movement component
	ApplyMovementAttributes();

	// Listen for future attribute changes
	if (AttributeComp)
	{
		AttributeComp->OnAttributeChanged.AddDynamic(this, &ASurvivorCharacter::OnAttributeChanged);
	}

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	// Bind OnHit for Wall Bounce
	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ASurvivorCharacter::OnHit);

	// Register with upgrade subsystem
	if (UUpgradeSubsystem* UpgradeSubsystem = GetWorld()->GetSubsystem<UUpgradeSubsystem>())
	{
		UpgradeSubsystem->RegisterPlayer(this);
	}

	// Spawn starting weapon using new multi-weapon system
	if (StartingWeaponData)
	{
		AddWeapon(StartingWeaponData);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASurvivorCharacter: StartingWeaponData is NULL! No weapon spawned. Please assign a WeaponData asset in the Character Blueprint."));
	}
}

void ASurvivorCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Cache velocity for OnHit calculations (since GetVelocity() might be zeroed by the time OnHit fires)
	LastFrameVelocity = GetVelocity();

	// Calculate Rolling Visuals
	if (PlayerVisualMesh)
	{
		float VelocitySize = GetVelocity().Size();
		if (VelocitySize > 10.0f)
		{
			// Calculate rotation amount based on distance traveled
			// ArcLength = Radius * Angle (radians) -> Angle = ArcLength / Radius
			// ArcLength (this frame) = Velocity * DeltaTime
			
			float Radius = 50.0f; // Default fallback
			if (PlayerVisualMesh->GetStaticMesh())
			{
				Radius = PlayerVisualMesh->GetStaticMesh()->GetBounds().SphereRadius;
			}
			
			// Avoid divide by zero
			Radius = FMath::Max(Radius, 1.0f);

			float DistanceTraveled = VelocitySize * DeltaTime;
			float RotationAngleRadians = DistanceTraveled / Radius;
			float RotationAngleDegrees = FMath::RadiansToDegrees(RotationAngleRadians);

			// Rotate around the Right Vector (Pitch) to roll forward
			// Since Actor rotates to face movement (Yaw), "Forward" is always X, so we rotate around Y (Pitch).
			// Negative Pitch is "Nose Down" which corresponds to rolling forward.
			PlayerVisualMesh->AddLocalRotation(FRotator(-RotationAngleDegrees, 0.0f, 0.0f));
		}
	}

	// Update Rolling Sound
	if (RollingAudioComp && AttributeComp)
	{
		float CurrentSpeed = GetVelocity().Size();
		// Only play if moving significantly and on the ground
		bool bShouldPlay = CurrentSpeed > 10.0f && GetCharacterMovement()->IsMovingOnGround();

		if (bShouldPlay)
		{
			if (!RollingAudioComp->IsPlaying())
			{
				RollingAudioComp->Play();
			}

			// Volume Control
			if (SpeedToVolumeCurve)
			{
				float Volume = SpeedToVolumeCurve->GetFloatValue(CurrentSpeed);
				RollingAudioComp->SetFloatParameter(SpeedParameterName, Volume);
			}

			// Pitch Control (Scaled by MaxSpeed)
			float MaxSpeed = AttributeComp->MaxSpeed.GetCurrentValue();
			// Ensure MaxSpeed is non-zero to avoid division errors or weird mapping
			MaxSpeed = FMath::Max(MaxSpeed, 1.0f);

			float Pitch = FMath::GetMappedRangeValueClamped(FVector2D(0, MaxSpeed), FVector2D(0.8f, 1.2f), CurrentSpeed);
			RollingAudioComp->SetPitchMultiplier(Pitch);
		}
		else
		{
			// Stop playing if idle or in air
			if (RollingAudioComp->IsPlaying())
			{
				RollingAudioComp->Stop();
			}
		}
	}
}

void ASurvivorCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASurvivorCharacter::Move);
	}
}

void ASurvivorCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// Move relative to the Camera (SpringArm)
		const FRotator CameraRotation = SpringArmComp->GetComponentRotation();
		const FRotator YawRotation(0, CameraRotation.Yaw, 0);

		// Get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// Get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// Debug Log
		// UE_LOG(LogTemp, Warning, TEXT("Input: %s | CamYaw: %f | Fwd: %s | Right: %s"), 
		// 	*MovementVector.ToString(), CameraRotation.Yaw, *ForwardDirection.ToString(), *RightDirection.ToString());

		// Add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ASurvivorCharacter::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Bounce off any blocking object that isn't a Pawn (so we don't bounce off enemies/players)
	if (OtherActor && OtherActor != this && !OtherActor->IsA(APawn::StaticClass()))
	{
		// Use LastFrameVelocity because GetVelocity() might be zeroed out by the collision resolution before OnHit is called
		FVector CurrentVelocity = LastFrameVelocity;
		
		// If velocity is too low, we might be stuck or just starting to move, try to use LastInputVector or just ignore
		// But for a bounce, we usually need some speed.
		
		// Reflect velocity off the wall
		FVector MirroredVelocity = FMath::GetReflectionVector(CurrentVelocity, Hit.ImpactNormal);

		// Add a vertical "hop" to the bounce (Manual Jump)
		// We use the CharacterMovement's JumpZVelocity so it matches a normal jump height
		MirroredVelocity.Z = GetCharacterMovement()->JumpZVelocity;
		
		// Launch Character handles the velocity change and can override current velocity
		// We override XY and Z to force the bounce
		LaunchCharacter(MirroredVelocity, true, true);

		// Play Bounce Sound
		if (WallBounceSound)
		{
			UGameplayStatics::PlaySound2D(this, WallBounceSound);
		}
	}
}

void ASurvivorCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (AttributeComp)
	{
		// Listen for attribute changes (MaxHealth, Speed, etc.)
		AttributeComp->OnAttributeChanged.AddDynamic(this, &ASurvivorCharacter::OnHealthChanged);
		
		// Listen for Health changes (Damage, Healing)
		AttributeComp->OnHealthChanged.AddDynamic(this, &ASurvivorCharacter::OnHealthChanged);

		// Initialize legacy values
		CurHealth = AttributeComp->GetCurrentHealth();
		MaxHealth = AttributeComp->MaxHealth.GetCurrentValue();
	}
}

void ASurvivorCharacter::OnHealthChanged(UAttributeComponent* Component, bool bIsResultOfEditorChange)
{
	// Sync legacy floats for UI
	if (AttributeComp)
	{
		CurHealth = AttributeComp->GetCurrentHealth();
		MaxHealth = AttributeComp->MaxHealth.GetCurrentValue();

		// Notify Blueprint to update UI
		OnHealthUpdated();
	}
}

void ASurvivorCharacter::ApplyMovementAttributes()
{
	if (GetCharacterMovement() && AttributeComp)
	{
		GetCharacterMovement()->MaxWalkSpeed = AttributeComp->MaxSpeed.GetCurrentValue();
		GetCharacterMovement()->MaxAcceleration = AttributeComp->MaxAcceleration.GetCurrentValue();
	}
}

void ASurvivorCharacter::OnAttributeChanged(UAttributeComponent* Component, bool bIsResultOfEditorChange)
{
	ApplyMovementAttributes();
}

void ASurvivorCharacter::AddXP(int32 Amount)
{
    CurrentXP += Amount;

    // Check for level ups (can level multiple times from one XP gain)
    int32 XPNeeded = GetXPForCurrentLevel();
    while (CurrentXP >= XPNeeded)
    {
        CurrentXP -= XPNeeded;
        CurrentLevel++;

        // Trigger upgrade selection via subsystem
        if (UUpgradeSubsystem* UpgradeSubsystem = GetWorld()->GetSubsystem<UUpgradeSubsystem>())
        {
            UpgradeSubsystem->TriggerUpgradeSelection();
        }

        OnLevelUp(CurrentLevel);
        XPNeeded = GetXPForCurrentLevel();
    }

    OnXPAdded(CurrentXP, CurrentLevel, GetLevelProgress());
}

int32 ASurvivorCharacter::GetXPForLevel(int32 Level) const
{
    // Formula: XP = Base * Level^Exponent + Linear * Level
    return FMath::FloorToInt(
        XPCurveBase * FMath::Pow(static_cast<float>(Level), XPCurveExponent)
        + XPCurveLinear * static_cast<float>(Level)
    );
}

int32 ASurvivorCharacter::GetXPForCurrentLevel() const
{
    return GetXPForLevel(CurrentLevel);
}

float ASurvivorCharacter::GetLevelProgress() const
{
    const int32 XPNeeded = GetXPForCurrentLevel();
    if (XPNeeded <= 0) return 0.0f;
    return static_cast<float>(CurrentXP) / static_cast<float>(XPNeeded);
}

void ASurvivorCharacter::DebugKillNearby()
{
    // Sphere trace or just iterate all actors
    TArray<AActor*> FoundEnemies;
    UGameplayStatics::GetAllActorsOfClass(this, ASurvivorEnemy::StaticClass(), FoundEnemies);

    for (AActor* Actor : FoundEnemies)
    {
        if (ASurvivorEnemy* Enemy = Cast<ASurvivorEnemy>(Actor))
        {
            // Kill within range (e.g. 2000 units) or all?
            // User said "kill enemies so I can test gems", so killing all is probably fine/better.
            // Let's kill all.
            if (UAttributeComponent* Attr = Enemy->AttributeComp)
            {
                Attr->ApplyHealthChange(-10000.0f);
            }
        }
    }
}

ASurvivorWeapon* ASurvivorCharacter::AddWeapon(UWeaponDataBase* WeaponData)
{
    if (!WeaponData)
    {
        UE_LOG(LogTemp, Warning, TEXT("ASurvivorCharacter::AddWeapon: WeaponData is null"));
        return nullptr;
    }

    if (!CanAddWeapon())
    {
        UE_LOG(LogTemp, Warning, TEXT("ASurvivorCharacter::AddWeapon: At max weapon capacity (%d/%d)"),
            OwnedWeapons.Num(), MaxWeaponSlots);
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;

    // Get the weapon actor class from the data asset (or use default)
    TSubclassOf<ASurvivorWeapon> WeaponActorClass = WeaponData->GetWeaponActorClass();
    if (!WeaponActorClass)
    {
        WeaponActorClass = ASurvivorWeapon::StaticClass();
    }

    ASurvivorWeapon* NewWeapon = GetWorld()->SpawnActor<ASurvivorWeapon>(WeaponActorClass, GetActorTransform(), SpawnParams);
    if (NewWeapon)
    {
        NewWeapon->WeaponData = WeaponData;
        NewWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        NewWeapon->StartShooting();

        OwnedWeapons.Add(NewWeapon);

        // Register with upgrade subsystem for tracking
        if (UUpgradeSubsystem* UpgradeSubsystem = GetWorld()->GetSubsystem<UUpgradeSubsystem>())
        {
            UpgradeSubsystem->RegisterWeapon(WeaponData, NewWeapon);
        }

        UE_LOG(LogTemp, Log, TEXT("ASurvivorCharacter: Added weapon '%s' (%d/%d slots)"),
            *WeaponData->WeaponID.ToString(), OwnedWeapons.Num(), MaxWeaponSlots);
    }

    return NewWeapon;
}

void ASurvivorCharacter::RemoveWeapon(ASurvivorWeapon* Weapon)
{
    if (!Weapon)
    {
        return;
    }

    Weapon->StopShooting();
    OwnedWeapons.Remove(Weapon);
    Weapon->Destroy();

    UE_LOG(LogTemp, Log, TEXT("ASurvivorCharacter: Removed weapon (%d/%d slots remaining)"),
        OwnedWeapons.Num(), MaxWeaponSlots);
}
