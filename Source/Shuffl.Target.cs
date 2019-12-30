// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ShufflTarget : TargetRules
{
	public ShufflTarget( TargetInfo Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.V2;
		Type = TargetType.Game;

		ExtraModuleNames.AddRange( new string[] { "Shuffl" } );
	}
}
