#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerSanityComponent.generated.h"

class UMaterialInstanceDynamic;
class UAudioComponent;
class UPostProcessComponent;
class UMaterialInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSanityChanged, float, NormalizedSanity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSanityAudioParameter, FName, ParameterName, float, NormalizedSanity);

UCLASS(ClassGroup = (Horror), meta = (BlueprintSpawnableComponent))
class JACKNKILL_API UPlayerSanityComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPlayerSanityComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sanity")
    TObjectPtr<AActor> Monster;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sanity", meta = (ClampMin = "0"))
    float DrainPerSecond = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sanity", meta = (ClampMin = "0"))
    float RecoveryPerSecond = 0.025f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sanity", meta = (ClampMin = "1"))
    float MaxLookDistance = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sanity", meta = (ClampMin = "1", ClampMax = "89"))
    float LookHalfAngleDegrees = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sanity|Audio")
    FName AudioParameterName = TEXT("Sanity");

    UPROPERTY(BlueprintAssignable, Category = "Sanity")
    FSanityChanged OnSanityChanged;

    // Bind to Wwise/FMOD in the presentation layer; values remain normalized.
    UPROPERTY(BlueprintAssignable, Category = "Sanity|Audio")
    FSanityAudioParameter OnAudioParameter;

    UFUNCTION(BlueprintPure, Category = "Sanity")
    float GetNormalizedSanity() const { return NormalizedSanity; }

    // Pass an owned post-process component with no conflicting blendables.
    UFUNCTION(BlueprintCallable, Category = "Sanity|Presentation")
    void InitializePresentation(UPostProcessComponent* PostProcess,
        UMaterialInterface* Material, UAudioComponent* Ambience);

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    float NormalizedSanity = 1.0f;
    float OriginalPitchMultiplier = 1.0f;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> SanityMaterial;

    TWeakObjectPtr<UPostProcessComponent> PresentationPostProcess;
    TWeakObjectPtr<UAudioComponent> AmbienceAudio;

    bool IsLookingAtMonster() const;
    void PublishPresentation();
    void ReleasePresentation();
};
