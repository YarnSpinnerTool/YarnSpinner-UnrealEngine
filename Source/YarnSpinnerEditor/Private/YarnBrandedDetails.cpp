// ============================================================================
//
//  Yarn Spinner for Unreal Engine
//
//  Copyright (c) Yarn Spinner Pty. Ltd. All Rights Reserved.
//
//  Yarn Spinner is a trademark of Secret Lab Pty. Ltd., used under license.
//
//  This code is subject to the terms and conditions of the license found in
//  the LICENSE.md file in the root of this repository.
//
//  For help, support, and more information, visit:
//    https://yarnspinner.dev
//    https://docs.yarnspinner.dev
//
// ============================================================================

#include "YarnBrandedDetails.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "YarnBrandedDetails"

TSharedRef<IDetailCustomization> FYarnBrandedDetails::MakeInstance()
{
	return MakeShareable(new FYarnBrandedDetails());
}

void FYarnBrandedDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FString VersionName;
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("YarnSpinner"))
	{
		VersionName = Plugin->GetDescriptor().VersionName;
	}

	const ISlateStyle* Style = FSlateStyleRegistry::FindSlateStyle("YarnSpinnerStyle");
	const FSlateBrush* Logo = Style ? Style->GetBrush("YarnSpinner.Logo") : nullptr;

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(FName(TEXT("Yarn Spinner")));
	Category.HeaderContent(
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(Logo)
			.Visibility(Logo ? EVisibility::Visible : EVisibility::Collapsed)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SSpacer)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 12, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(VersionName))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 12, 0)
		[
			SNew(SHyperlink)
			.Text(LOCTEXT("DocsLink", "Docs"))
			.OnNavigate_Lambda([]()
			{
				FPlatformProcess::LaunchURL(TEXT("https://docs.yarnspinner.dev"), nullptr, nullptr);
			})
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 4, 0)
		[
			SNew(SHyperlink)
			.Text(LOCTEXT("SupportLink", "Support"))
			.OnNavigate_Lambda([]()
			{
				FPlatformProcess::LaunchURL(TEXT("https://discord.gg/yarnspinner"), nullptr, nullptr);
			})
		],
		/*bWholeRowContent=*/true
	);
}

#undef LOCTEXT_NAMESPACE
