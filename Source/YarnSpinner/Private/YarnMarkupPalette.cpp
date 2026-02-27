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

#include "YarnMarkupPalette.h"

bool UYarnMarkupPalette::ContainsMarker(const FString& MarkerName) const
{
	// check basic markers
	for (const FYarnBasicMarker& Marker : BasicMarkers)
	{
		if (Marker.MarkerName.Equals(MarkerName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	// check custom markers
	for (const FYarnCustomMarker& Marker : CustomMarkers)
	{
		if (Marker.MarkerName.Equals(MarkerName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool UYarnMarkupPalette::TryGetBasicMarker(const FString& MarkerName, FYarnBasicMarker& OutMarker) const
{
	for (const FYarnBasicMarker& Marker : BasicMarkers)
	{
		if (Marker.MarkerName.Equals(MarkerName, ESearchCase::IgnoreCase))
		{
			OutMarker = Marker;
			return true;
		}
	}
	return false;
}

bool UYarnMarkupPalette::TryGetCustomMarker(const FString& MarkerName, FYarnCustomMarker& OutMarker) const
{
	for (const FYarnCustomMarker& Marker : CustomMarkers)
	{
		if (Marker.MarkerName.Equals(MarkerName, ESearchCase::IgnoreCase))
		{
			OutMarker = Marker;
			return true;
		}
	}
	return false;
}

TArray<FString> UYarnMarkupPalette::GetAllMarkerNames() const
{
	TArray<FString> Names;

	for (const FYarnBasicMarker& Marker : BasicMarkers)
	{
		Names.Add(Marker.MarkerName);
	}

	for (const FYarnCustomMarker& Marker : CustomMarkers)
	{
		Names.Add(Marker.MarkerName);
	}

	return Names;
}

bool UYarnMarkupPalette::TryGetPropertyDefault(const FString& MarkerName, const FString& PropertyName, FString& OutDefaultValue) const
{
	for (const FYarnCustomMarker& Marker : CustomMarkers)
	{
		if (Marker.MarkerName.Equals(MarkerName, ESearchCase::IgnoreCase))
		{
			for (const FYarnMarkerProperty& Prop : Marker.Properties)
			{
				if (Prop.PropertyName.Equals(PropertyName, ESearchCase::IgnoreCase))
				{
					OutDefaultValue = Prop.DefaultValue;
					return true;
				}
			}
		}
	}
	return false;
}
