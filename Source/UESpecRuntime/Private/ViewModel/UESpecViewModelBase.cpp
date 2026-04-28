#include "ViewModel/UESpecViewModelBase.h"

#include "AngelscriptManager.h"
#include "AngelscriptType.h"
#include "UObject/FieldIterator.h"
#include "ViewModel/UESpecAngelscriptBridge.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UESpecViewModelBase)

namespace UE::UESpec::Private
{
	static TSharedPtr<FAngelscriptClassDesc> TryGetAngelscriptClassDescriptor(const UClass* ViewModelClass)
	{
		if (!ViewModelClass || !UE::UESpec::Angelscript::IsAngelscriptGeneratedClass(ViewModelClass) || !FAngelscriptManager::IsInitialized())
		{
			return nullptr;
		}

		return FAngelscriptManager::Get().GetClass(ViewModelClass->GetName());
	}

	static bool ShouldExposeViewModelFunction(const UFunction* Function)
	{
		if (!Function)
		{
			return false;
		}

		const UClass* OwnerClass = Function->GetOwnerClass();
		if (!OwnerClass || !OwnerClass->IsChildOf(UUESpecViewModelBase::StaticClass()))
		{
			return false;
		}

		if (OwnerClass == UUESpecViewModelBase::StaticClass() || OwnerClass == UMVVMViewModelBase::StaticClass())
		{
			return false;
		}

		const bool bIsAngelscriptOwner = UE::UESpec::Angelscript::IsAngelscriptGeneratedClass(OwnerClass);
		if (!Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
		{
			if (!bIsAngelscriptOwner)
			{
				return false;
			}

			const TSharedPtr<FAngelscriptClassDesc> ClassDesc = TryGetAngelscriptClassDescriptor(OwnerClass);
			const TSharedPtr<FAngelscriptFunctionDesc> MethodDesc = ClassDesc.IsValid() ? ClassDesc->GetMethod(Function->GetName()) : nullptr;
			if (!MethodDesc.IsValid() || (!MethodDesc->bBlueprintCallable && !MethodDesc->bBlueprintPure))
			{
				return false;
			}
		}

		if (Function->HasAnyFunctionFlags(FUNC_Delegate | FUNC_Static))
		{
			return false;
		}

		if (Function->HasMetaData(TEXT("DeprecatedFunction")) || Function->GetName().StartsWith(TEXT("K2_")))
		{
			return false;
		}

		return true;
	}
}

void UUESpecViewModelBase::DescribeViewModelClass(TSubclassOf<UUESpecViewModelBase> ViewModelClass, TArray<FUESpecViewModelMethodDescriptor>& OutMethods)
{
	DescribeViewModelClass(ViewModelClass.Get(), OutMethods);
}

void UUESpecViewModelBase::DescribeViewModelClass(const UClass* ViewModelClass, TArray<FUESpecViewModelMethodDescriptor>& OutMethods)
{
	OutMethods.Reset();
	if (!ViewModelClass || !ViewModelClass->IsChildOf(UUESpecViewModelBase::StaticClass()))
	{
		return;
	}

	TMap<FString, int32> DescriptorIndicesByName;
	for (TFieldIterator<UFunction> FunctionIt(ViewModelClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
	{
		const UFunction* Function = *FunctionIt;
		if (!UE::UESpec::Private::ShouldExposeViewModelFunction(Function))
		{
			continue;
		}

		FUESpecViewModelMethodDescriptor Descriptor;
		Descriptor.Name = Function->GetName();
		Descriptor.bPure = IsUESpecPureFunction(Function);

		for (TFieldIterator<FProperty> PropertyIt(Function); PropertyIt; ++PropertyIt)
		{
			const FProperty* Property = *PropertyIt;
			if (!Property->HasAnyPropertyFlags(CPF_Parm))
			{
				continue;
			}

			if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				Descriptor.ReturnType = Property->GetCPPType(nullptr, CPPF_None);
				continue;
			}

			if (Property->HasAnyPropertyFlags(CPF_OutParm) && !Property->HasAnyPropertyFlags(CPF_ConstParm))
			{
				continue;
			}

			FUESpecViewModelMethodParameter Parameter;
			Parameter.Name = Property->GetName();
			Parameter.Type = Property->GetCPPType(nullptr, CPPF_None);
			Descriptor.Parameters.Add(MoveTemp(Parameter));
		}

		DescriptorIndicesByName.Add(Descriptor.Name, OutMethods.Num());
		OutMethods.Add(MoveTemp(Descriptor));
	}

	if (const TSharedPtr<FAngelscriptClassDesc> ClassDesc = UE::UESpec::Private::TryGetAngelscriptClassDescriptor(ViewModelClass))
	{
		for (const TSharedRef<FAngelscriptFunctionDesc>& Method : ClassDesc->Methods)
		{
			if (!Method->bBlueprintCallable && !Method->bBlueprintPure)
			{
				continue;
			}

			if (Method->bIsStatic || Method->bIsPrivate || Method->bIsProtected)
			{
				continue;
			}

			if (int32* ExistingIndex = DescriptorIndicesByName.Find(Method->FunctionName))
			{
				OutMethods[*ExistingIndex].bPure = OutMethods[*ExistingIndex].bPure || Method->bBlueprintPure;
				continue;
			}

			FUESpecViewModelMethodDescriptor Descriptor;
			Descriptor.Name = Method->FunctionName;
			Descriptor.bPure = Method->bBlueprintPure;
			Descriptor.ReturnType = Method->ReturnType.IsValid() ? Method->ReturnType.GetAngelscriptDeclaration() : FString();

			for (const FAngelscriptArgumentDesc& Argument : Method->Arguments)
			{
				FUESpecViewModelMethodParameter Parameter;
				Parameter.Name = Argument.ArgumentName;
				Parameter.Type = Argument.Type.IsValid() ? Argument.Type.GetAngelscriptDeclaration() : FString();
				Descriptor.Parameters.Add(MoveTemp(Parameter));
			}

			DescriptorIndicesByName.Add(Descriptor.Name, OutMethods.Num());
			OutMethods.Add(MoveTemp(Descriptor));
		}
	}

	OutMethods.Sort([](const FUESpecViewModelMethodDescriptor& A, const FUESpecViewModelMethodDescriptor& B)
	{
		return A.Name < B.Name;
	});
}

bool UUESpecViewModelBase::IsUESpecPureFunction(const UFunction* Function)
{
	if (!Function)
	{
		return false;
	}

	if (Function->HasAnyFunctionFlags(FUNC_BlueprintPure) || Function->HasMetaData(TEXT("UESpecPure")))
	{
		return true;
	}

	const UClass* const OwnerClass = Function->GetOwnerClass();
	if (const TSharedPtr<FAngelscriptClassDesc> ClassDesc = UE::UESpec::Private::TryGetAngelscriptClassDescriptor(OwnerClass))
	{
		if (const TSharedPtr<FAngelscriptFunctionDesc> MethodDesc = ClassDesc->GetMethod(Function->GetName()))
		{
			return MethodDesc->bBlueprintPure;
		}
	}

	return false;
}
