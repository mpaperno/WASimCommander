# valid -Targets are "all", "version", "source", "module", "package", "docs"
# add "no-<target>" to exclude a target when building "all"
# -Projects <list> to build individual projects only (eg. "WASimModule" or "Testing/CS_BasicConsole")
# -Configuration <config>[,<config>]  (Debug/Debug-DLL/Release/Release-DLL/Release-NetFW)
# -BuildType <type>[,<type>]   (Clean/Build/Rebuild)

[CmdletBinding(PositionalBinding=$false)]
Param(
  [string[]]$Targets = "all",
  [string]$RootPath = "..",
  [string[]]$Configuration = @("Debug", "Debug-DLL", "Release-DLL", "Release-net5", "Release-net6", "Release-net7", "Release-netfw", "Release"),
  [string]$Platform = "x64",
  [string[]]$Projects = "all",
  [string]$BuildType = "Clean,Rebuild",
  [switch]$Clean = $false,
	[string]$ZipUtil = "${Env:ProgramFiles}\7-Zip\7z.exe",
	[string]$Doxygen = "D:\Programs\doxygen\bin\doxygen.exe"
)

$ErrorActionPreference = "Stop"
# $CurrentDir = [System.IO.Path]::GetDirectoryName($myInvocation.MyCommand.Path)

. .\version.ps1

$RootPath = Resolve-Path -Path "$RootPath"
$SrcPath = "$RootPath\src"
$BuildPath = "$SrcPath\build"
$DocsPath = "$RootPath\docs"
$DistPath = "$RootPath\dist"
$PackagePath = "$DistPath\package"
$LibPath = "$PackagePath\lib"
$ModulePath = "${SrcPath}\WASimModule\WASimModuleProject\WASimCommander-Module\WASimCommander-Module.xml"
$ModuleDest = "${DistPath}\module"
$ModulePackage = "${PackagePath}\module"
$DocsDistroPath = "$RootPath\..\gh-pages"

$testApps = @{
	"$BuildPath\CS_BasicConsole\Release-$Platform\$Platform\Release\net8.0-windows" = "$PackagePath\bin\CS_BasicConsole";
	"$BuildPath\CPP_BasicConsole\Release-$Platform" = "$PackagePath\bin\CPP_BasicConsole"
	"$SrcPath\Testing\Py_BasicConsole" = "$PackagePath\bin\Py_BasicConsole"
}

$buildAll = $Targets.Contains("all")
$build = @{}
foreach ($targetName in @("version", "source", "module", "package", "docs")) {
	$build[$targetName] = (($buildAll -and -not $Targets.Contains("no-${targetName}")) -or $Targets.Contains($targetName))
}


if (($buildAll -or $Clean) -and (Test-Path $PackagePath)) {
  Write-Output "Cleaning '$PROJECT_NAME' dist folder '$PackagePath'..."
  Remove-Item "$PackagePath" -Force -Recurse
}
New-Item -ItemType Directory -Force -Path "$PackagePath" | Out-Null

if ($build.version) {
	. .\Make-Version.ps1
	Make-Version -SrcPath $SrcPath -DocPath $DocsPath
}

if ($build.source)
{
	Import-Module -Name ".\Invoke-MsBuild.psm1"
	$projectTargets = ""
	if ($Projects -eq "all") {
		$projectTargets += "$BuildType"
	}
	else {
		foreach ($bldType in $BuildType) {
			foreach ($tgt in $Projects) {
				if ($projectTargets -ne "") {
					$projectTargets += ";"
				}
				$projectTargets += "${tgt}:${bldType}"
			}
		}
	}
	$MsBuildParams = "/target:$projectTargets /p:Platform=${Platform} /p:BuildInParallel=true /maxcpucount"

	foreach ($buildConfig in $Configuration) {
		Write-Output "Building '$PROJECT_NAME' for config '$buildConfig' with arguments: '$MsBuildParams'"
		$buildResult = Invoke-MsBuild -Path "$SrcPath\$PROJECT_NAME.sln" -MsBuildParameter "${MsBuildParams} /p:Configuration=${buildConfig}" `
			-ShowBuildOutputInNewWindow -BuildLogDirectoryPath "$DistPath" -KeepBuildLogOnSuccessfulBuilds -AutoLaunchBuildErrorsLogOnFailure
			#-PromptForInputBeforeClosing
		if ($buildResult.BuildSucceeded -eq $true) {
			Write-Output ("Build completed successfully in {0:N1} seconds." -f $buildResult.BuildDuration.TotalSeconds)
		}
		else {
			Write-Output ("Build failed after {0:N1} seconds. Check the build log file '$($buildResult.BuildLogFilePath)' for errors." -f $buildResult.BuildDuration.TotalSeconds)
			Exit 1
		}
	}
}

if ($build.module)
{
	$packageTool = "${Env:MSFS_SDK}\Tools\bin\fspackagetool.exe"
	& $packageTool "$ModulePath" -outputdir "$ModuleDest" -mirroring -rebuild
	# -nopause
}

$copyOptions = @("/S", "/A", "/NP", "/LOG+:""$DistPath\copy.log""")
Remove-Item "$DistPath\copy.log" -ErrorAction Ignore | Out-Null

if ($build.docs)
{
  Write-Output "Generating documentation with Doxygen..."
	Push-Location -Path "${DocsPath}"
  Remove-Item "${DocsPath}\html" -Force -Recurse
	& $Doxygen "Doxyfile"
	Pop-Location
	if (Test-Path $DocsDistroPath) {
		robocopy "${DocsPath}\html" "$DocsDistroPath" /XD .git /MIR $copyOptions
		Write-Output "Copied docs to distro folder $DocsDistroPath"
	}
}

if (-not $build.package) {
	Exit 0
}

Write-Output ("Copying files to $PackagePath ...")

# include folder
robocopy "$SrcPath\include" "$PackagePath\include" $copyOptions /XF .* *.in
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }

# C++ libs
$msvcVersion = "msvc142"
$libStatic = "${LibPath}\static\${msvcVersion}"
$libDynamic = "${LibPath}\dynamic\${msvcVersion}"

# static, release and debug
robocopy "$BuildPath\${CLIENT_NAME}\Release-$Platform" "$libStatic" $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
robocopy "$BuildPath\${CLIENT_NAME}\Debug-$Platform" "$libStatic" $copyOptions /XF *.ini
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
# dynamic, release and debug
robocopy "$BuildPath\${CLIENT_NAME}\Release-DLL-$Platform" "$libDynamic" $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
robocopy "$BuildPath\${CLIENT_NAME}\Debug-DLL-$Platform" "$libDynamic" $copyOptions /XF *.ini
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
# license and readme
robocopy "$SrcPath\${CLIENT_NAME}" "$libStatic" LICENSE.txt LICENSE.GPL.txt *.md $copyOptions
robocopy "$SrcPath\${CLIENT_NAME}" "$libDynamic" *.txt *.md $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }

# Managed DLL
$csLibPath = "${LibPath}\managed"

# .NET 5
robocopy "$BuildPath\${CLIENT_NAME}_CLI\Release-net5-$Platform" "${csLibPath}\net5" *.dll *.pdb *.xml *.ini $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
# .NET 6
robocopy "$BuildPath\${CLIENT_NAME}_CLI\Release-net6-$Platform" "${csLibPath}\net6" *.dll *.pdb *.xml *.ini $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
# .NET 7
robocopy "$BuildPath\${CLIENT_NAME}_CLI\Release-net7-$Platform" "${csLibPath}\net7" *.dll *.pdb *.xml *.ini $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
# .NET 8 (default Release build)
robocopy "$BuildPath\${CLIENT_NAME}_CLI\Release-$Platform" "${csLibPath}\net8" *.dll *.pdb *.xml *.ini $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
# .NET Framework
robocopy "$BuildPath\${CLIENT_NAME}_CLI\Release-netfw-$Platform" "${csLibPath}\net46" *.dll *.pdb *.xml *.ini $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
# license and readme
robocopy "$SrcPath\${CLIENT_NAME}_CLI" "$csLibPath" *.txt *.md $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }

# Module
robocopy "$ModuleDest\Packages" "$ModulePackage" $copyOptions /XF .git*
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
robocopy "$SrcPath\${SERVER_NAME}" "$ModulePackage" *.txt *.md $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }

# GUI
robocopy "$BuildPath\${APP_GUI_NAME}\Release-$Platform" "$PackagePath\bin\${APP_GUI_NAME}" $copyOptions /XF *.log* opengl32sw.dll vc_redist*.*
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
robocopy "$SrcPath\${APP_GUI_NAME}" "$PackagePath\bin\${APP_GUI_NAME}" LICENSE.* *.md $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }

# Test/demo app(s)
$testApps.GetEnumerator() | ForEach-Object {
	robocopy "$($_.Key)" "$($_.Value)" $copyOptions /XF *.log* *.*proj
	if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
}
# Required DLL files for Python example
robocopy "$BuildPath\${CLIENT_NAME}_CLI\Release-$Platform" $PackagePath\bin\Py_BasicConsole *.dll *.ini $copyOptions

# docs
robocopy "$DocsPath\html" "$PackagePath\docs\reference" $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }

copy "..\README.md" "$PackagePath"
copy "..\LICENSE*.*" "$PackagePath"
copy "..\CHANGELOG.md" "$PackagePath"

Write-Output ("Creating archives ...")

$fileVersion = "v{0}.{1}.{2}.{3}{4}" -f ($VER_MAJOR,$VER_MINOR,$VER_PATCH,$VER_BUILD,$VER_NAME)

$destZipFile = "$DistPath\${PROJECT_NAME}_SDK-$fileVersion.zip"
if (Test-Path $destZipFile) {
  Remove-Item $destZipFile -Force
}
& "$ZipUtil" a "$destZipFile" "$PackagePath\*" -tzip

$destZipFile = "$DistPath\${SERVER_NAME}-$fileVersion.zip"
if (Test-Path $destZipFile) {
  Remove-Item $destZipFile -Force
}
& "$ZipUtil" a "$destZipFile" "$ModulePackage\*" "$PackagePath\CHANGELOG.md" -tzip

$destZipFile = "$DistPath\${APP_GUI_NAME}-$fileVersion.zip"
if (Test-Path $destZipFile) {
  Remove-Item $destZipFile -Force
}
& "$ZipUtil" a "$destZipFile" "$PackagePath\bin\${APP_GUI_NAME}" "$PackagePath\CHANGELOG.md" -tzip


Write-Output ("`nC'est fini!")
