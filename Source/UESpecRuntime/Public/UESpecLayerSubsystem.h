#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/SubclassOf.h"
#include "Widgets/UESpecWidgetBase.h"

#include "UESpecLayerSubsystem.generated.h"

USTRUCT()
struct FUESpecLayerStackEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FName LayerName = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<UUESpecActivatableWidgetBase> Widget = nullptr;
};

UCLASS()
class UESPECRUNTIME_API UUESpecLayerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UESpec|Layers")
	void RegisterLayerClass(FName LayerName, TSubclassOf<UUESpecActivatableWidgetBase> WidgetClass);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Layers")
	void UnregisterLayerClass(FName LayerName);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Layers")
	void PushLayer(FName LayerName);

	UFUNCTION(BlueprintCallable, Category = "UESpec|Layers")
	void CloseTopLayer();

	UFUNCTION(BlueprintCallable, Category = "UESpec|Layers")
	void CloseLayerByName(FName LayerName);

	UFUNCTION(BlueprintPure, Category = "UESpec|Layers")
	UUESpecActivatableWidgetBase* GetTopLayer() const;

private:
	UPROPERTY(Transient)
	TMap<FName, TSubclassOf<UUESpecActivatableWidgetBase>> RegisteredLayerClasses;

	UPROPERTY(Transient)
	TArray<FUESpecLayerStackEntry> LayerStack;

	void CompactLayerStack();
	UUESpecActivatableWidgetBase* CreateLayerWidget(FName LayerName) const;
	void CloseLayerEntryAt(int32 EntryIndex);
};
