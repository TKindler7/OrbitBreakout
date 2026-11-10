using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.Loader;

// Optional, version-sensitive action-export host, verified initially with UE 5.8.
// This affects only this process's cached Unreal settings directory. It does not
// modify engine binaries, global settings, filesystem permissions or the sandbox.
// Normal unrestricted builds should use Unreal's stock Build.sh route instead.
class ProjectBuildHost
{
    static int Main(string[] args)
    {
        try
        {
            if (args.Length < 3)
                throw new ArgumentException("Usage: ProjectBuildHost <UBT-directory> <project-cache-directory> <UBT-arguments...>");
            string ubtDir = Path.GetFullPath(args[0]);
            string cacheDir = Path.GetFullPath(args[1]);
            Directory.CreateDirectory(cacheDir);
            AssemblyLoadContext.Default.Resolving += (_, name) => {
                string path = Path.Combine(ubtDir, name.Name + ".dll");
                return File.Exists(path) ? AssemblyLoadContext.Default.LoadFromAssemblyPath(path) : null;
            };
            var core = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(ubtDir, "EpicGames.Core.dll"));
            var build = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(ubtDir, "EpicGames.Build.dll"));
            var unreal = build.GetType("UnrealBuildBase.Unreal", true);
            var directoryRef = core.GetType("EpicGames.Core.DirectoryReference", true);
            foreach (string fieldName in new[] {"s_applicationSettingDirectory", "s_userSettingDirectory"})
            {
                var field = unreal.GetField(fieldName, BindingFlags.Static | BindingFlags.NonPublic);
                if (field == null) throw new NotSupportedException("Unsupported Unreal cache API: " + fieldName);
                var lazy = field.GetValue(null);
                lazy.GetType().GetProperty("Value").GetValue(lazy);
                var valueField = lazy.GetType().GetField("_value", BindingFlags.Instance | BindingFlags.NonPublic);
                if (valueField == null) throw new NotSupportedException("Unsupported .NET Lazy cache layout.");
                valueField.SetValue(lazy, Activator.CreateInstance(directoryRef, new object[] {cacheDir}));
            }

            var ubt = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(ubtDir, "UnrealBuildTool.dll"));
            var flags = BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
            Type T(string name) => ubt.GetType("UnrealBuildTool." + name, true);
            object FileRef(string path) => Activator.CreateInstance(core.GetType("EpicGames.Core.FileReference", true), new object[] {path});
            string[] buildArgs = args.Skip(2).ToArray();
            string Option(string prefix) => buildArgs.Single(s => s.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)).Substring(prefix.Length);
            string root = Path.GetFullPath(Option("-rootdirectory="));
            string project = Path.GetFullPath(Option("-Project="));
            Directory.SetCurrentDirectory(Path.Combine(root, "Engine", "Source"));
            var cli = Activator.CreateInstance(core.GetType("EpicGames.Core.CommandLineArguments", true), new object[] {buildArgs});
            var logger = core.GetType("EpicGames.Core.Log", true).GetProperty("Logger", flags).GetValue(null);
            var projectDir = Activator.CreateInstance(directoryRef, new object[] {Path.GetDirectoryName(project)});
            T("UnrealBuildTool").GetField("InitialEnvironment", flags).SetValue(null, Environment.GetEnvironmentVariables());
            var xml = T("XmlConfig");
            xml.GetMethod("ReadConfigFiles", flags).Invoke(null, new object[] {FileRef(Option("-XmlConfigCache=")), projectDir, logger});
            var mode = T("BuildMode");
            T("UEBuildPlatform").GetMethods(flags).Single(m => m.Name == "RegisterPlatforms" && m.GetParameters().Length == 5 && m.GetParameters()[2].ParameterType == typeof(Type))
                .Invoke(null, new object[] {false, true, mode, buildArgs, logger});
            var config = Activator.CreateInstance(T("BuildConfiguration"));
            xml.GetMethods(flags).Single(m => m.Name == "ApplyTo" && m.GetParameters().Length == 1).Invoke(null, new object[] {config});
            cli.GetType().GetMethods().Single(m => m.Name == "ApplyTo" && !m.IsGenericMethod && m.GetParameters().Length == 1).Invoke(cli, new object[] {config});
            var descriptors = T("TargetDescriptor").GetMethods(flags)
                .Single(m => m.Name == "ParseCommandLine" && m.GetParameters().Length == 3 && m.GetParameters()[0].ParameterType == cli.GetType())
                .Invoke(null, new object[] {cli, config, logger});
            var working = Activator.CreateInstance(T("EmptySourceFileWorkingSet"));
            var dependencies = Activator.CreateInstance(T("CppDependencyCache"));
            string actions = Path.Combine(Path.GetDirectoryName(project), "Intermediate", "Build", "Actions.json");
            var method = mode.GetMethods(BindingFlags.Static | BindingFlags.Public).Single(m => m.Name == "BuildAsync");
            var task = (System.Threading.Tasks.Task)method.Invoke(null, new object[] {
                descriptors, config, working, Enum.ToObject(T("BuildOptions"), 0), FileRef(actions), dependencies, logger, null
            });
            task.GetAwaiter().GetResult();
            Console.WriteLine("EXPORTED_ACTIONS " + actions);
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error is TargetInvocationException invocation ? invocation.InnerException : error);
            Console.Error.WriteLine("Action export failed. Engine internals may have changed; use the standard Build.sh executor when available.");
            return 1;
        }
    }
}
