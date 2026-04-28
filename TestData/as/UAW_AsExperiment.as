class UUAW_AsExperiment : UUESpecActivatableWidgetBase
{
    UPROPERTY(BlueprintReadOnly)
    bool bActivatedMethodCalled = false;

    UFUNCTION(BlueprintCallable)
    void CustomMethod()
    {
        bActivatedMethodCalled = true;
    }
}
