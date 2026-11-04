$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'Visual Studio Installer and the C++ toolchain are required.' }
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) { throw 'No Visual Studio C++ toolchain was found.' }
$vcvars = Join-Path $vsRoot 'VC/Auxiliary/Build/vcvars64.bat'
$tests = @(
    @{ Name='FunctionExpression'; Sources=@('Source/FuncGame/Math/FunctionExpression.cpp','Tests/FunctionExpressionTests.cpp') },
    @{ Name='FunctionTrajectory'; Sources=@('Source/FuncGame/Math/FunctionExpression.cpp','Source/FuncGame/Math/FunctionTrajectory.cpp','Tests/FunctionTrajectoryTests.cpp') },
    @{ Name='CombatRules'; Sources=@('Source/FuncGame/Gameplay/CombatRules.cpp','Tests/CombatRulesTests.cpp') }
)
foreach ($test in $tests) {
    $buildDir = Join-Path $projectRoot ("Saved/Tests/" + $test.Name)
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
    $exePath = Join-Path $buildDir ($test.Name + '.exe')
    $sourceArgs = ($test.Sources | ForEach-Object { '"' + (Join-Path $projectRoot $_) + '"' }) -join ' '
    $command = 'call "{0}" >nul && cl /nologo /std:c++17 /EHsc /utf-8 /W4 /Fe:"{1}" {2}' -f $vcvars,$exePath,$sourceArgs
    Push-Location $buildDir
    try {
        & $env:ComSpec /d /s /c $command
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $($test.Name)" }
        & $exePath
        if ($LASTEXITCODE -ne 0) { throw "Test failed: $($test.Name)" }
    } finally { Pop-Location }
}
