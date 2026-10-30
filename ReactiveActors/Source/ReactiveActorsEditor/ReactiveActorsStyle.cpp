// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveActorsStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

DEFINE_LOG_CATEGORY_STATIC(LogReactiveActorsEditor, Log, All);

TSharedPtr<FSlateStyleSet> FReactiveActorsStyle::StyleSet;

void FReactiveActorsStyle::Register()
{
	if (StyleSet.IsValid())
	{
		return;
	}

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ReactiveActors"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogReactiveActorsEditor, Warning, TEXT("Reactive Actors icons: plugin 'ReactiveActors' not found, no icons registered."));
		return;
	}

	StyleSet = MakeShared<FSlateStyleSet>(TEXT("ReactiveActorsStyle"));
	StyleSet->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources") / TEXT("Icons"));

	const FVector2D IconSize(16.f, 16.f);
	const FVector2D ThumbnailSize(64.f, 64.f);

	// The class name without its U prefix, which is how Unreal looks them up. Files: <Name>_16.svg and <Name>_64.svg
	const TCHAR* ClassNames[] =
	{
		TEXT("ReactiveAction"),
		TEXT("ReactiveRequirement"),
		TEXT("ReactiveRule"),
		TEXT("ReactiveTrigger"),
	};

	int32 Missing = 0;
	for (const TCHAR* ClassName : ClassNames)
	{
		const FString Name(ClassName);
		for (const TCHAR* Suffix : { TEXT("_16"), TEXT("_64") })
		{
			const FString File = StyleSet->RootToContentDir(Name + Suffix, TEXT(".svg"));
			if (!IFileManager::Get().FileExists(*File))
			{
				UE_LOG(LogReactiveActorsEditor, Warning, TEXT("Reactive Actors icons: missing %s"), *FPaths::ConvertRelativePathToFull(File));
				++Missing;
			}
		}
		StyleSet->Set(*(TEXT("ClassIcon.") + Name),
			new FSlateVectorImageBrush(StyleSet->RootToContentDir(Name + TEXT("_16"), TEXT(".svg")), IconSize));
		StyleSet->Set(*(TEXT("ClassThumbnail.") + Name),
			new FSlateVectorImageBrush(StyleSet->RootToContentDir(Name + TEXT("_64"), TEXT(".svg")), ThumbnailSize));
	}

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
	UE_LOG(LogReactiveActorsEditor, Log, TEXT("Reactive Actors icons: registered from %s (%d file(s) missing)."),
		*FPaths::ConvertRelativePathToFull(StyleSet->GetContentRootDir()), Missing);
}

void FReactiveActorsStyle::Unregister()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}
}
