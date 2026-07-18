// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class house_project : ModuleRules
{
	public house_project(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"DesktopPlatform",   // OpenFileDialog (사진 불러오기)
			"HTTP",              // Gemini API 요청
			"Json",              // Gemini 응답 파싱 + 레이아웃 저장
			"JsonUtilities"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		// House Tetris 소스만 포함 — 게임 템플릿 Variant 폴더 제거됨
		PublicIncludePaths.AddRange(new string[] {
			"house_project"
		});
	}
}
