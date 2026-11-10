using UnrealBuildTool;
public class OrbitBreakoutEditorTarget : TargetRules { public OrbitBreakoutEditorTarget(TargetInfo Target) : base(Target) { Type=TargetType.Editor; DefaultBuildSettings=BuildSettingsVersion.Latest; IncludeOrderVersion=EngineIncludeOrderVersion.Latest; ExtraModuleNames.Add("OrbitBreakout"); } }
