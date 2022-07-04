# valid -Targets are "all", "version", "source", "module", "package", "docs"
# add "no-<target>" to exclude a target when building "all"
# -Projects <list> to build individual projects only (eg. "WASimModule" or "Testing/CS_BasicConsole")
# -Configuration <config>[,<config>]  (Debug/Debug-DLL/Release/Release-DLL)
# -BuildType <type>[,<type>]   (Clean/Build/Rebuild)

[CmdletBinding(PositionalBinding=$false)]
Param(
  [string[]]$Targets = "all",
  [string]$RootPath = "..",
  [string[]]$Configuration = @("Debug", "Release-DLL", "Release"),
  [string]$Platform = "x64",
  [string[]]$Projects = "all",
  [string]$BuildType = "Clean,Rebuild",
  [switch]$Clean = $false,
  [string]$MsvcVer = "v142",
  [string]$MsvcName = "2019",
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
$ModulePath = "${SrcPath}\WASimModule\WASimModuleProject\WASimCommander-Module\WASimCommander-Module.xml"
$ModuleDest = "${DistPath}\module"
$ModulePackage = "${PackagePath}\module"

$testApps = @{
	"$BuildPath\CS_BasicConsole\Release-$Platform\$Platform\Release\net6.0-windows" = "$PackagePath\bin\CS_BasicConsole"
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

if ($build.docs)
{
  Write-Output "Generating documentation with Doxygen..."
	Push-Location -Path "${DocsPath}"
  Remove-Item "${DocsPath}\html" -Force -Recurse
	& $Doxygen "Doxyfile"
	Pop-Location
}

if (-not $build.package) {
	Exit 0
}

Write-Output ("Copying files to $PackagePath ...")

$libPath = "$PackagePath\lib\MSVS-${MsvcName}"
$libStatic = "${libPath}\static"
$libDynamic = "${libPath}\dynamic"
$libManaged = "${libPath}\managed"

$copyOptions = @("/S", "/A", "/NP", "/LOG+:""$DistPath\copy.log""")
Remove-Item "$DistPath\copy.log" -ErrorAction Ignore | Out-Null

# include folder
robocopy "$SrcPath\include" "$PackagePath\include" $copyOptions /XF .* *.in
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }

# C++ libs
robocopy "$BuildPath\${CLIENT_NAME}\Release-$Platform" "$libStatic" $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
robocopy "$BuildPath\${CLIENT_NAME}\Debug-$Platform" "$libStatic" $copyOptions /XF *.ini
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
robocopy "$BuildPath\${CLIENT_NAME}\Release-DLL-$Platform" "$libDynamic" $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
robocopy "$BuildPath\${CLIENT_NAME}\Debug-DLL-$Platform" "$libDynamic" $copyOptions /XF *.ini
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }

# Managed DLL
robocopy "$BuildPath\${CLIENT_NAME}_CLI\Release-$Platform" "$libManaged" *.dll *.xml *.ini $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
robocopy "$BuildPath\${CLIENT_NAME}_CLI\Debug-$Platform" "$libManaged" *.dll $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }

# libs licence and readme
robocopy "$SrcPath\${CLIENT_NAME}" "$PackagePath\lib" *.txt *.md $copyOptions
if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }

# Module
robocopy "$ModuleDest\Packages" "$ModulePackage" $copyOptions /XF .*
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
	robocopy "$($_.Key)" "$($_.Value)" $copyOptions /XF *.log*
	if ($LastExitCode -ge 8) { Write-Output($LastExitCode); Exit 1  }
}

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
& "$ZipUtil" a "$destZipFile" "$ModulePackage\*" -tzip

$destZipFile = "$DistPath\${APP_GUI_NAME}-$fileVersion.zip"
if (Test-Path $destZipFile) {
  Remove-Item $destZipFile -Force
}
& "$ZipUtil" a "$destZipFile" "$PackagePath\bin\${APP_GUI_NAME}" -tzip


Write-Output ("`nC'est fini!")
