// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ShufflEditorTarget : TargetRules
{
	public ShufflEditorTarget( TargetInfo Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.V2;
		Type = TargetType.Editor;

		ExtraModuleNames.AddRange( new string[] { "Shuffl" } );
	}
}
