<#
.SYNOPSIS
  Drive clang-tidy over the translation units the solution builds, for the naming table.

.DESCRIPTION
  AGENTS.md section 1: ".clang-tidy is in the tree and nothing runs it ... CI therefore does not
  check naming." This is that driver. It reads each .vcxproj for its sources and include
  directories, follows the .vcxitems it imports, and runs clang-tidy in clang-cl driver mode so
  the MSVC-shaped flags and the Windows SDK headers resolve.

  WHAT IT CANNOT DO. clang is not MSVC. /std:c++latest is ahead of what clang implements, and the
  C++/WinRT headers on the client side lean on MSVC extensions -- so a parse error here is a
  clang limitation, not a defect in the tree, and that is exactly why this runs as a REPORT and
  not as a gate. Read its output; do not let it fail a build until it has been seen green once.

  Naming is enforced by review until then (AGENTS.md section 1), so treat a finding as a finding
  and a crash as a note to whoever next touches this script.

  WHERE IT STANDS, measured on the runner rather than guessed (AGENTS.md section 6):
    2026-09-21, first run   19 translation units, 8 with diagnostics. All of them traced to
                            $(VCInstallDir) never expanding, so CppUnitTest.h was not found and
                            TEST_CLASS(SuiteSmoke) parsed as a global variable declaration.
    2026-09-21, after that  19 translation units, 2 with diagnostics.
  What is left is noise rather than findings: clang-tidy reports tens of thousands of warnings
  generated from the Windows SDK headers it walks. .clang-tidy's HeaderFilterRegex limits what is
  REPORTED, not what is analyzed. Quieting that is the work between here and -Gate.

.PARAMETER Gate
  Return a non-zero exit code when clang-tidy reports a diagnostic. Off by default: turn it on
  only once this has run clean on the runner, and say so in the pull request that does it.

.EXAMPLE
  pwsh Scripts/RunClangTidy.ps1
  pwsh Scripts/RunClangTidy.ps1 -Gate
#>
[CmdletBinding()]
param(
  [switch]$Gate,
  [string]$Platform = 'x64',
  [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# clang-tidy ships with Visual Studio under the "C++ Clang tools for Windows" component, which is
# optional -- so its absence is a skip with a message, never a failure.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$clangTidy = $null
if (Test-Path $vswhere) {
  $vsRoot = & $vswhere -latest -products * -property installationPath | Select-Object -First 1
  if ($vsRoot) {
    $candidate = Join-Path $vsRoot 'VC\Tools\Llvm\bin\clang-tidy.exe'
    if (Test-Path $candidate) { $clangTidy = $candidate }
  }
}
if (-not $clangTidy) {
  $onPath = Get-Command clang-tidy -ErrorAction SilentlyContinue
  if ($onPath) { $clangTidy = $onPath.Source }
}
if (-not $clangTidy) {
  Write-Output "clang-tidy not found. Install the 'C++ Clang tools for Windows' component."
  Write-Output "Naming stays review's problem until then (AGENTS.md section 1)."
  exit 0
}
Write-Output "clang-tidy: $clangTidy"

# $(VCInstallDir) is MSBuild's, not the shell's, and the suites reach CppUnitTest.h through it.
# Without this every test translation unit fails to parse, and TEST_CLASS(SuiteSmoke) is then read
# as a global variable declaration -- which is where the first run's naming "finding" came from.
$vcInstallDir = $null
if ($vsRoot) {
  foreach ($candidate in @("$vsRoot\VC\", "$vsRoot\VC\Auxiliary\VS\")) {
    if (Test-Path (Join-Path $candidate 'UnitTest\include\CppUnitTest.h')) { $vcInstallDir = $candidate; break }
  }
}
if ($vcInstallDir) { Write-Output "VCInstallDir: $vcInstallDir" }
else { Write-Output "CppUnitTest.h not located; the suites will not parse." }

function Get-Metadata([xml]$xml, [string]$name) {
  # The value of an unconditioned ClCompile setting, with MSBuild's macros expanded.
  $nodes = $xml.Project.ItemDefinitionGroup | Where-Object { -not $_.Condition }
  ($nodes.ClCompile.$name | Where-Object { $_ }) -join ';'
}

function Expand-Macros([string]$text, [string]$projectDir) {
  $expanded = $text -replace '\$\(SolutionDir\)', "$root\" `
                    -replace '\$\(MSBuildThisFileDirectory\)', "$projectDir\" `
                    -replace '%\(AdditionalIncludeDirectories\)', ''
  if ($script:vcInstallDir) { $expanded = $expanded -replace '\$\(VCInstallDir\)', $script:vcInstallDir }
  $expanded
}

$units = 0
$findings = 0
foreach ($project in Get-ChildItem -Path $root -Recurse -Filter '*.vcxproj' -File) {
  $xml = [xml](Get-Content -LiteralPath $project.FullName)
  $projectDir = $project.DirectoryName

  $includes = New-Object System.Collections.Generic.List[string]
  $defines = New-Object System.Collections.Generic.List[string]
  $includes.Add($projectDir)                       # cl searches the including file's directory
  foreach ($piece in (Expand-Macros (Get-Metadata $xml 'AdditionalIncludeDirectories') $projectDir) -split ';') {
    $piece = $piece.Trim()
    if ($piece -and $piece -notmatch '\$\(') { $includes.Add($piece.TrimEnd('\')) }
  }
  foreach ($piece in (Get-Metadata $xml 'PreprocessorDefinitions') -split ';') {
    if ($piece.Trim() -and $piece -notlike '%(*') { $defines.Add($piece.Trim()) }
  }
  $defines.Add($(if ($Configuration -eq 'Debug') { '_DEBUG' } else { 'NDEBUG' }))

  # Sources of the project, plus those of every .vcxitems it imports -- the shared-items files
  # are compiled INTO this project, with this project's settings, so they are checked here.
  $sources = New-Object System.Collections.Generic.List[string]
  foreach ($item in $xml.Project.ItemGroup.ClCompile) {
    if ($item.Include) { $sources.Add((Join-Path $projectDir $item.Include)) }
  }
  foreach ($import in $xml.Project.ImportGroup.Import) {
    if ($import.Project -and $import.Project.EndsWith('.vcxitems')) {
      $sharedPath = Join-Path $projectDir $import.Project
      if (-not (Test-Path $sharedPath)) { continue }
      $shared = [xml](Get-Content -LiteralPath $sharedPath)
      $sharedDir = Split-Path -Parent $sharedPath
      $includes.Add($sharedDir)
      foreach ($piece in (Expand-Macros (Get-Metadata $shared 'AdditionalIncludeDirectories') $sharedDir) -split ';') {
        if ($piece.Trim()) { $includes.Add($piece.Trim().TrimEnd('\')) }
      }
      foreach ($item in $shared.Project.ItemGroup.ClCompile) {
        if ($item.Include) { $sources.Add((Expand-Macros $item.Include $sharedDir)) }
      }
    }
  }

  $flags = @('--driver-mode=cl', '/std:c++latest', '/permissive-', '/EHsc', '/DWIN32', '/D_WINDOWS')
  $flags += ($defines | Select-Object -Unique | ForEach-Object { "/D$_" })
  $flags += ($includes | Select-Object -Unique | ForEach-Object { "/I$_" })

  foreach ($source in $sources | Select-Object -Unique) {
    # pch.cpp exists to create the precompiled header and holds no code of its own.
    if (-not (Test-Path $source) -or (Split-Path -Leaf $source) -eq 'pch.cpp') { continue }
    $units++
    $output = & $clangTidy --quiet $source -- @flags 2>&1
    $text = ($output | Out-String).Trim()
    if ($text) {
      Write-Output "--- $((Resolve-Path $source -Relative)) ---"
      Write-Output $text
      if ($text -match 'warning:|error:') { $findings++ }
    }
  }
}

Write-Output ""
Write-Output "$units translation unit(s), $findings with diagnostics."
if ($Gate -and $findings -gt 0) {
  throw "clang-tidy reported diagnostics in $findings translation unit(s)."
}
exit 0
