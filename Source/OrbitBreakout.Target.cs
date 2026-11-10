using UnrealBuildTool;
public class OrbitBreakoutTarget : TargetRules { public OrbitBreakoutTarget(TargetInfo Target) : base(Target) { Type=TargetType.Game; DefaultBuildSettings=BuildSettingsVersion.Latest; IncludeOrderVersion=EngineIncludeOrderVersion.Latest; ExtraModuleNames.Add("OrbitBreakout"); } }
