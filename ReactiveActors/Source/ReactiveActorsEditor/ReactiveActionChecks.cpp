// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveActionChecks.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "ReactiveAction.h"

namespace ReactiveActionChecks
{
	UEdGraphNode* FindExecuteWithoutFinish(const UBlueprint* Blueprint)
	{
		if (!Blueprint || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(UReactiveAction::StaticClass()))
		{
			return nullptr; // not a Reactive Action
		}

		// The event is protected, so it's named directly (keep in sync with UReactiveAction::OnExecuteAction)
		static const FName ExecuteName(TEXT("OnExecuteAction"));
		static const FName FinishName = GET_FUNCTION_NAME_CHECKED(UReactiveAction, FinishAction);

		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);

		UEdGraphNode* ExecuteNode = nullptr;
		for (const UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}

			// Execute Action implemented as a function graph rather than an event
			if (!ExecuteNode && Graph->GetFName() == ExecuteName && Graph->Nodes.Num() > 0)
			{
				ExecuteNode = Graph->Nodes[0];
			}

			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (const UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
				{
					if (Event->EventReference.GetMemberName() == ExecuteName)
					{
						ExecuteNode = Node;
					}
				}
				else if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
				{
					const FName Called = Call->FunctionReference.GetMemberName();
					if (Called == FinishName || Called == ExecuteName)
					{
						return nullptr; // finishes, or calls the parent's Execute Action, which finishes
					}
				}
			}
		}
		return ExecuteNode;
	}

	FText MissingFinishMessage()
	{
		return INVTEXT("never calls Finish Action. Every Reactive Action must call Finish Action when it's done (Success on, or off to fail the reaction), or the reaction running it waits forever.");
	}
}
