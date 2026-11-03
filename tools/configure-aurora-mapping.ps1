#requires -Version 7.0
[CmdletBinding()]
param(
    [switch]$Apply,
    [string]$AuroraDirectory=(Join-Path $env:ProgramFiles 'Aurora'),
    [string]$BackupDirectory=(Join-Path $env:LOCALAPPDATA 'RedBladeAurora\Backups')
)
$ErrorActionPreference='Stop'
$lib=Join-Path $AuroraDirectory 'AuroraDeviceManager'
# Use the exact RGB.NET provider shipped with Aurora to resolve the actual
# LED IDs. This also handles releases with missing/changed QMK LED labels.
foreach($name in @('RGB.NET.Core.dll','OpenRGB.NET.dll','RGB.NET.Devices.OpenRGB.dll')) {
    [void][Reflection.Assembly]::LoadFrom((Join-Path $lib $name))
}
$definition=[RGB.NET.Devices.OpenRGB.OpenRGBServerDefinition]::new()
$definition.Ip='127.0.0.1'
$definition.Port=6742
$definition.ClientName='Red Blade Aurora mapping check'
$provider=[RGB.NET.Devices.OpenRGB.OpenRGBDeviceProvider]::Instance
$provider.AddDeviceDefinition($definition)
try {
    [void]$provider.Initialize([RGB.NET.Core.RGBDeviceType]::Keyboard,$true)
    $targets=@($provider.Devices | Where-Object {
        $_.DeviceInfo.Manufacturer -eq 'DesignedbyGG' -and $_.DeviceInfo.Model -eq 'Redblade FS'
    })
    if($targets.Count -eq 0) {
        Write-Output 'Red Blade RGB controller is not available yet. Flash and verify the prepared firmware, then restart OpenRGB.'
        exit 2
    }
    if($targets.Count -ne 1) { throw 'Expected exactly one Redblade FS controller.' }
    $target=$targets[0]
    $physical=Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot '..\aurora-key-map.json') | ConvertFrom-Json
    $mapping=[ordered]@{}
    $seen=[Collections.Generic.HashSet[int]]::new()
    foreach($led in $target) {
        if($led.CustomData -isnot [int]) { throw 'Unexpected provider LED index format; no mapping was changed.' }
        $index=[int]$led.CustomData
        if($index -lt 0 -or $index -ge 109 -or -not $seen.Add($index)) { throw 'Invalid or duplicated provider LED index.' }
        $mapping[[string][int]$led.Id]=[int]$physical[$index].device_key
    }
    if($seen.Count -ne 109) { throw "Expected 109 LEDs, found $($seen.Count)." }
    Write-Output "Verified explicit physical mapping of 109 LEDs for $($target.DeviceInfo.DeviceName)."
    if(-not $Apply) { Write-Output 'Use -Apply to save this mapping, then restart Aurora.'; return }
    $path=Join-Path $env:APPDATA 'Aurora\DeviceMappings.json'
    $backup=Join-Path $BackupDirectory (Get-Date -Format 'yyyyMMdd-HHmmss')
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    if(Test-Path -LiteralPath $path) {
        Copy-Item -LiteralPath $path -Destination (Join-Path $backup 'DeviceMappings.json')
        $config=Get-Content -Raw -LiteralPath $path | ConvertFrom-Json -AsHashtable
    } else { $config=@{d=@()} }
    $others=@($config['d'] | Where-Object { $_.n -ne $target.DeviceInfo.DeviceName })
    $config['d']=@($others)+@(@{n=$target.DeviceInfo.DeviceName;k=$mapping})
    $config | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $path -Encoding utf8NoBOM
    Write-Output "Mapping saved. Restart Aurora to load it. Backup: $backup"
} finally { $provider.Dispose() }
