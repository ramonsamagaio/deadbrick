$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Stage = Join-Path $ProjectRoot "ReferenceExtracted\LayOfTheLand"
$Docs = Join-Path $ProjectRoot "Docs"
$Out = Join-Path $Docs "LOTL_ASSET_MANIFEST.txt"

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " DEADBRICK - generate LayOfTheLand asset manifest" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path $Stage)) {
    Write-Host "Could not find extracted reference folder:" -ForegroundColor Red
    Write-Host $Stage -ForegroundColor Yellow
    Write-Host "Run IMPORT_LOTL_REFERENCE_UE58.bat first." -ForegroundColor Yellow
    exit 12
}

New-Item -ItemType Directory -Path $Docs -Force | Out-Null
$allFiles = @(Get-ChildItem $Stage -Recurse -File)

function Get-Category([string]$Path) {
    $n = $Path.ToLowerInvariant()
    if ($n -match 'anim|idle|walk|run|sprint|attack|hit|hurt|death|die|jump|fall|land|locomotion|montage') { return 'ANIMATION' }
    if ($n -match 'player|character|human|male|female|enemy|creature|skeleton|skin|body|head') { return 'CHARACTER' }
    if ($n -match 'door|gate|hatch') { return 'DOOR' }
    if ($n -match 'window|glass') { return 'WINDOW' }
    if ($n -match 'container|crate|chest|box|locker|cabinet|shelf') { return 'CONTAINER' }
    if ($n -match 'weapon|sword|axe|bow|gun|rifle|pistol|shotgun|ammo') { return 'WEAPON' }
    if ($n -match 'vehicle|cart|wagon|car|truck') { return 'VEHICLE' }
    if ($n -match 'tree|rock|stone|grass|bush|plant|furniture|chair|table|barrel|prop') { return 'WORLD_PROP' }
    if ($n -match 'material|[/\\]m_|[/\\]mi_') { return 'MATERIAL' }
    if ($n -match 'texture|[/\\]t_') { return 'TEXTURE' }
    if ($n -match 'voxel|building|structure|road|cave|biome|generator|manager|fluid|gas|fire|water|weather|craft|item|save|physics') { return 'SYSTEM_OR_WORLD' }
    return 'OTHER'
}

$records = foreach ($f in $allFiles) {
    $relative = $f.FullName.Substring($Stage.Length).TrimStart('\','/').Replace('\','/')
    [PSCustomObject]@{
        Category = Get-Category $relative
        Extension = if ([string]::IsNullOrWhiteSpace($f.Extension)) { '<none>' } else { $f.Extension.ToLowerInvariant() }
        SizeKB = [math]::Round($f.Length / 1KB, 1)
        Path = $relative
    }
}

$uassets = @($records | Where-Object { $_.Extension -eq '.uasset' })
$priorityCategories = @('CHARACTER','ANIMATION','DOOR','WINDOW','CONTAINER','WEAPON','VEHICLE','WORLD_PROP','SYSTEM_OR_WORLD')
$summary = $uassets | Group-Object Category | Sort-Object Name
$extensionSummary = $records | Group-Object Extension | Sort-Object Count -Descending

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("DEADBRICK - LayOfTheLand Asset Manifest")
$lines.Add("Generated: $(Get-Date -Format o)")
$lines.Add("Stage: $Stage")
$lines.Add("Total extracted files: $($records.Count)")
$lines.Add("Total .uasset files: $($uassets.Count)")
$lines.Add("")
$lines.Add("=== EXTENSION SUMMARY ===")
foreach ($g in $extensionSummary) { $lines.Add(("{0,-16} {1,7}" -f $g.Name, $g.Count)) }
$lines.Add("")
$lines.Add("=== CATEGORY SUMMARY (.uasset) ===")
foreach ($g in $summary) { $lines.Add(("{0,-20} {1,7}" -f $g.Name, $g.Count)) }
$lines.Add("")
$lines.Add("=== PRIORITY UASSETS ===")
foreach ($r in ($uassets | Where-Object { $_.Category -in $priorityCategories } | Sort-Object Category,Path)) {
    $lines.Add(("[{0}] {1}" -f $r.Category, $r.Path))
}
$lines.Add("")
$lines.Add("=== ALL UASSETS ===")
foreach ($r in ($uassets | Sort-Object Path)) { $lines.Add($r.Path) }

if ($uassets.Count -eq 0) {
    $lines.Add("")
    $lines.Add("=== FALLBACK: ALL EXTRACTED FILES (no uassets found) ===")
    foreach ($r in ($records | Sort-Object Path)) {
        $lines.Add(("[{0}] {1} ({2} KB)" -f $r.Extension, $r.Path, $r.SizeKB))
    }
}

$lines | Set-Content -Path $Out -Encoding UTF8

Write-Host "Manifest generated: $Out" -ForegroundColor Green
Write-Host "Recovered .uassets: $($uassets.Count)" -ForegroundColor Green
Write-Host ""
Write-Host "Commit/push only Docs\LOTL_ASSET_MANIFEST.txt. Keep ReferenceExtracted and imported Content local." -ForegroundColor Yellow
