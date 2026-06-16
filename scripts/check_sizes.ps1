<#
.SYNOPSIS
    Flags source files that have grown past the per-file size budget.

.DESCRIPTION
    A soft guardrail against god objects. Walks engine/src and game/src and
    reports any .h/.cpp over the budget (default 800 lines). Known legacy
    offenders being carved down are tracked separately so the list of NEW
    violations stays actionable.

    Exit code is non-zero when a NON-exempt file exceeds the budget, so this
    can gate a commit hook or CI step. The legacy exemptions are expected to
    shrink over time — trim the list as files drop under budget.

.EXAMPLE
    .\scripts\check_sizes.ps1
    .\scripts\check_sizes.ps1 -Budget 600
#>
param(
    [int]$Budget = 800
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# Legacy god objects mid-carve. Don't let them grow; remove from this list once
# they're under budget. New files must never be added here.
$exempt = @(
    'game/src/GameApp.cpp',
    'game/src/world/World.cpp'
)

$roots = @('engine/src', 'game/src')
$violations = @()
$exemptOver = @()

foreach ($r in $roots) {
    $base = Join-Path $root $r
    if (-not (Test-Path $base)) { continue }
    Get-ChildItem -Path $base -Recurse -Include *.h, *.cpp -File | ForEach-Object {
        $lines = (Get-Content -LiteralPath $_.FullName | Measure-Object -Line).Lines
        if ($lines -le $Budget) { return }
        $rel = (Resolve-Path -LiteralPath $_.FullName -Relative -RelativeBasePath $root) `
            -replace '^[.\\/]+', '' -replace '\\', '/'
        $entry = [pscustomobject]@{ File = $rel; Lines = $lines }
        if ($exempt -contains $rel) { $exemptOver += $entry } else { $violations += $entry }
    }
}

if ($exemptOver) {
    Write-Host "Legacy files over budget ($Budget lines) — being carved down, don't grow:" -ForegroundColor DarkYellow
    $exemptOver | Sort-Object Lines -Descending | ForEach-Object {
        Write-Host ("  {0,6}  {1}" -f $_.Lines, $_.File) -ForegroundColor DarkYellow
    }
}

if ($violations) {
    Write-Host ""
    Write-Host "Files over the $Budget-line budget — split by responsibility:" -ForegroundColor Red
    $violations | Sort-Object Lines -Descending | ForEach-Object {
        Write-Host ("  {0,6}  {1}" -f $_.Lines, $_.File) -ForegroundColor Red
    }
    Write-Host ""
    Write-Host "See 'Keeping modules small' in CLAUDE.md." -ForegroundColor Red
    exit 1
}

Write-Host "All source files within the $Budget-line budget." -ForegroundColor Green
exit 0
