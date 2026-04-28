class UVM_AsSample : UUESpecViewModelBase
{
    UPROPERTY(BlueprintReadWrite)
    FText SkillLabel = FText::FromString("AS Slash");

    UPROPERTY(BlueprintReadWrite)
    bool bSkillReady = true;

    UPROPERTY(BlueprintReadWrite)
    bool bShowCooldown = false;

    UPROPERTY(BlueprintReadWrite)
    float CooldownRatio = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    int LastUsedSkillIndex = -1;

    UFUNCTION(BlueprintCallable)
    void UseSkill(int Index)
    {
        LastUsedSkillIndex = Index;
        bSkillReady = false;
        bShowCooldown = true;
        CooldownRatio = 0.75f;
        SkillLabel = FText::FromString("AS Skill Used");
    }

    UFUNCTION(BlueprintCallable)
    void ResetSkill()
    {
        LastUsedSkillIndex = -1;
        bSkillReady = true;
        bShowCooldown = false;
        CooldownRatio = 0.0f;
        SkillLabel = FText::FromString("AS Slash");
    }

    UFUNCTION(BlueprintPure)
    bool IsSkillReady(int Index) const
    {
        return bSkillReady;
    }

    UFUNCTION(BlueprintPure)
    bool IsSkillLocked(int Index) const
    {
        return false;
    }
}
