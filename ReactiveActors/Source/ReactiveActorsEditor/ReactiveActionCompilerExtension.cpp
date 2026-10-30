// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveActionCompilerExtension.h"

#include "KismetCompiler.h"
#include "ReactiveActionChecks.h"

void UReactiveActionCompilerExtension::ProcessBlueprintCompiled(const FKismetCompilerContext& CompilationContext, const FBlueprintCompiledData& Data)
{
	if (UEdGraphNode* ExecuteNode = ReactiveActionChecks::FindExecuteWithoutFinish(CompilationContext.Blueprint))
	{
		const FString Message = TEXT("@@ ") + ReactiveActionChecks::MissingFinishMessage().ToString();
		CompilationContext.MessageLog.Warning(*Message, ExecuteNode);
	}
}
