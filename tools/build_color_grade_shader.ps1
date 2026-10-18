param([string]$Compiler)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $Compiler) {
    $sdkBin = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/bin'
    $Compiler = Get-ChildItem -LiteralPath $sdkBin -Directory |
        Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName 'x64/dxc.exe' } |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1
}
if (-not $Compiler) { throw 'DirectX Shader Compiler not found. Supply -Compiler with the path to dxc.exe.' }
$outputDirectory = Join-Path $projectRoot 'out'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$bytecodePath = Join-Path $outputDirectory 'color_grade.dxil'
$sourcePath = Join-Path $projectRoot 'first_party/tig/shaders/color_grade.hlsl'
& $Compiler -T ps_6_0 -E main -O3 -Fo $bytecodePath $sourcePath
if ($LASTEXITCODE -ne 0) { throw 'Shader compilation failed.' }
$bytecode = [IO.File]::ReadAllBytes($bytecodePath)
$lines = [Collections.Generic.List[string]]::new()
$lines.Add('#ifndef TIG_COLOR_GRADE_DXIL_H_')
$lines.Add('#define TIG_COLOR_GRADE_DXIL_H_')
$lines.Add('static const unsigned char color_grade_dxil[] = {')
for ($offset = 0; $offset -lt $bytecode.Length; $offset += 16) {
    $last = [Math]::Min($offset + 15, $bytecode.Length - 1)
    $values = $bytecode[$offset..$last] | ForEach-Object { '0x{0:X2}' -f $_ }
    $lines.Add('    ' + ($values -join ', ') + ',')
}
$lines.Add('};')
$lines.Add('#endif')
$headerPath = Join-Path $projectRoot 'first_party/tig/src/color_grade_dxil.h'
[IO.File]::WriteAllText($headerPath, ($lines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
Write-Output "Compiled shader embedded: $($bytecode.Length) bytes."
