// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// The Run Action Blueprint node: pick an action class on the node, fill in its Expose on Spawn variables
// as pins (like Spawn Actor from Class), and it's made and run on Target in one step.
// Extends Epic's Construct Object from Class node, which does the class picker and the pins. At compile
// time it becomes: Create Action For Run -> set the exposed variables -> Run Action (the library function, hidden from the menu).
//
// Hidden from the node menu for now (GetMenuActions registers nothing): keeping every action's exposed pins
// correct as the class changes needs more work. Nodes already placed still compile and run.

#pragma once

#include "CoreMinimal.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "K2Node_RunReactiveAction.generated.h"

UCLASS()
class UK2Node_RunReactiveAction : public UK2Node_ConstructObjectFromClass
{
	GENERATED_BODY()

public:
	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetKeywords() const override;

	// UK2Node
	virtual FText GetMenuCategory() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	// UK2Node_ConstructObjectFromClass
	virtual bool IsSpawnVarPin(UEdGraphPin* Pin) const override;

protected:
	virtual FText GetBaseNodeTitle() const override;
	virtual FText GetNodeTitleFormat() const override;
	virtual UClass* GetClassPinBaseClass() const override;

private:
	UEdGraphPin* GetTargetPin() const;
	UEdGraphPin* GetInstigatorPin() const;
	UEdGraphPin* GetStartedPin() const;
};
