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

$extensions = @(".uasset", ".uexp", ".ubulk", ".uptnl")
$files = Get-ChildItem $Stage -Recurse -File | Where-Object { $extensions -contains $_.Extension.ToLowerInvariant() }

function Get-Category([string]$Name) {
    $n = $Name.ToLowerInvariant()
    if ($n -match 'anim|idle|walk|run|attack|hit|death|jump|locomotion') { return 'ANIMATION' }
    if ($n -match 'player|character|human|male|female|enemy|creature|skeleton|skin') { return 'CHARACTER' }
    if ($n -match 'door|gate') { return 'DOOR' }
    if ($n -match 'window|glass') { return 'WINDOW' }
    if ($n -match 'container|crate|chest|box|locker') { return 'CONTAINER' }
    if ($n -match 'weapon|sword|axe|bow|gun|rifle|pistol') { return 'WEAPON' }
    if ($n -match 'vehicle|cart|wagon') { return 'VEHICLE' }
    if ($n -match 'tree|rock|stone|grass|bush|plant') { return 'WORLD_PROP' }
    if ($n -match 'material|^m_|^mi_') { return 'MATERIAL' }
    if ($n -match 'texture|^t_') { return 'TEXTURE' }
    if ($n -match 'voxel|building|structure|road|cave|biome') { return 'SYSTEM_OR_WORLD' }
    return 'OTHER'
}

$records = foreach ($f in $files) {
    $relative = $f.FullName.Substring($Stage.Length).TrimStart('\','/')
    [PSCustomObject]@{
        Category = Get-Category $f.BaseName
        Extension = $f.Extension.ToLowerInvariant()
        SizeKB = [math]::Round($f.Length / 1KB, 1)
        Path = $relative.Replace('\','/')
    }
}

$uassets = $records | Where-Object { $_.Extension -eq '.uasset' }
$summary = $uassets | Group-Object Category | Sort-Object Name

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("DEADBRICK - LayOfTheLand Asset Manifest")
$lines.Add("Generated: $(Get-Date -Format o)")
$lines.Add("Stage: $Stage")
$lines.Add("Total cooked files: $($records.Count)")
$lines.Add("Total .uasset files: $($uassets.Count)")
$lines.Add("")
$lines.Add("=== CATEGORY SUMMARY (.uasset only) ===")
foreach ($g in $summary) { $lines.Add(("{0,-20} {1,6}" -f $g.Name, $g.Count)) }
$lines.Add("")
$lines.Add("=== PRIORITY ASSETS ===")
foreach ($r in ($uassets | Where-Object { $_.Category -in @('CHARACTER','ANIMATION','DOOR','WINDOW','CONTAINER','WEAPON','VEHICLE','SYSTEM_OR_WORLD') } | Sort-Object Category,Path)) {
    $lines.Add(("[{0}] {1}" -f $r.Category, $r.Path))
}
$lines.Add("")
$lines.Add("=== ALL UASSETS ===")
foreach ($r in ($uassets | Sort-Object Path)) { $lines.Add($r.Path) }

$lines | Set-Content -Path $Out -Encoding UTF8

Write-Host "Manifest generated:" -ForegroundColor Green
Write-Host $Out -ForegroundColor Green
Write-Host ""
Write-Host "Now commit and push ONLY Docs\LOTL_ASSET_MANIFEST.txt." -ForegroundColor Yellow
Write-Host "Do not add the extracted 2GB reference folders to Git." -ForegroundColor Yellow
