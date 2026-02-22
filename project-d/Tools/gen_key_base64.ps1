param(
    [string]$KeysDir = "",
    [string]$OutFile = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($KeysDir)) {
    $KeysDir = Join-Path $PSScriptRoot "..\Keys"
}

if ([string]::IsNullOrWhiteSpace($OutFile)) {
    $OutFile = Join-Path $KeysDir "key_icons.base64.json"
}

$keysPath = (Resolve-Path -LiteralPath $KeysDir).Path
$tokenOrder = @("LB", "RB", "LRB", "Plus", "W", "A", "S", "D", "Shift", "Shift_en", "Ctrl", "Space", "Space_en")
$result = [ordered]@{}

foreach ($token in $tokenOrder) {
    $pngPath = Join-Path $keysPath ($token + ".png")
    if (-not (Test-Path -LiteralPath $pngPath)) {
        throw "Missing key icon: $pngPath"
    }

    $bytes = [System.IO.File]::ReadAllBytes($pngPath)
    $result[$token] = [Convert]::ToBase64String($bytes)
}

$json = $result | ConvertTo-Json -Depth 4
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($OutFile, $json, $utf8NoBom)

Write-Output "Generated: $OutFile"
