[CmdletBinding()]
param(
    [ValidateSet('x64', 'x86')]
    [string]$Architecture = 'x64',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',
    [string]$BuildDirectory,
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer introuvable.' }
$installations = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json
if ($LASTEXITCODE -ne 0 -or !$installations) { throw 'Outils Visual C++ x86/x64 introuvables.' }
$installation = @($installations)[0]
$major = ([version]$installation.installationVersion).Major
if ($major -eq 18) { $generator = 'Visual Studio 18 2026' }
elseif ($major -eq 17) { $generator = 'Visual Studio 17 2022' }
else { throw "Version Visual Studio non prise en charge : $major" }
$cmake = Join-Path $installation.installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
if (!(Test-Path -LiteralPath $cmake) -or !(Test-Path -LiteralPath $ctest)) { throw 'Composant CMake de Visual Studio introuvable.' }
if (!$BuildDirectory) { $BuildDirectory = "out/build/windows-$Architecture-vs$major" }
if (![IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory = Join-Path $projectRoot $BuildDirectory }
$platform = if ($Architecture -eq 'x64') { 'x64' } else { 'Win32' }
& $cmake -S $projectRoot -B $BuildDirectory -G $generator -A $platform '-DARCANUM_BUILD_TESTS=ON'
if ($LASTEXITCODE -ne 0) { throw "Configuration $Architecture en echec." }
& $cmake --build $BuildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "Compilation $Architecture en echec." }
if (!$SkipTests) {
    & $ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure --no-tests=error
    if ($LASTEXITCODE -ne 0) { throw "Tests $Architecture en echec." }
}
Write-Output (Join-Path $BuildDirectory "$Configuration/arcanum-ce.exe")
