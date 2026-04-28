class UUW_AsExperiment : UUESpecWidgetBase
{
    UPROPERTY(BlueprintReadOnly)
    bool bCustomMethodCalled = false;

    UFUNCTION(BlueprintCallable)
    void CustomMethod()
    {
        bCustomMethodCalled = true;
    }
}
