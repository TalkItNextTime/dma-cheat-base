param(
    [string]$IconsDir = "",
    [string]$OutHeader = "",
    [int]$ChunkSize = 120
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($IconsDir)) {
    $IconsDir = Join-Path $PSScriptRoot "..\cs2-weapon-esp-icons-main"
}

if ([string]::IsNullOrWhiteSpace($OutHeader)) {
    $OutHeader = Join-Path $PSScriptRoot "..\Source\Features\ESP\WeaponIconsEmbedded.hpp"
}

if ($ChunkSize -lt 16) {
    throw "ChunkSize must be >= 16"
}

$iconsPath = (Resolve-Path -LiteralPath $IconsDir).Path
$pngFiles = Get-ChildItem -LiteralPath $iconsPath -File -Filter *.png | Sort-Object Name
if (-not $pngFiles -or $pngFiles.Count -eq 0) {
    throw "No PNG files found in: $iconsPath"
}

$entries = New-Object System.Collections.Generic.List[object]
$seenTokens = New-Object System.Collections.Generic.HashSet[string]

foreach ($file in $pngFiles) {
    $token = $file.BaseName.ToLowerInvariant()
    $token = [System.Text.RegularExpressions.Regex]::Replace($token, "[^a-z0-9_]+", "_")
    $token = $token.Trim("_")
    if ([string]::IsNullOrWhiteSpace($token)) {
        throw "Invalid token from file name: $($file.Name)"
    }
    if (-not $seenTokens.Add($token)) {
        throw "Duplicate normalized token '$token' from file: $($file.Name)"
    }

    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
    $base64 = [Convert]::ToBase64String($bytes)

    $entries.Add([PSCustomObject]@{
        Token = $token
        Base64 = $base64
    }) | Out-Null
}

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("#pragma once")
[void]$sb.AppendLine("#include <string>")
[void]$sb.AppendLine("#include <unordered_map>")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("namespace EmbeddedWeaponIcons")
[void]$sb.AppendLine("{")
[void]$sb.AppendLine("inline std::unordered_map<std::string, std::string> BuildWeaponIconsBase64Map()")
[void]$sb.AppendLine("{")
[void]$sb.AppendLine("    std::unordered_map<std::string, std::string> out{};")
[void]$sb.AppendLine(("    out.reserve({0});" -f $entries.Count))

foreach ($entry in $entries) {
    $token = [string]$entry.Token
    $value = [string]$entry.Base64
    if ([string]::IsNullOrWhiteSpace($value)) {
        throw "Empty base64 payload: $token"
    }

    [void]$sb.AppendLine("    {")
    [void]$sb.AppendLine("        std::string value{};")
    [void]$sb.AppendLine(("        value.reserve({0});" -f $value.Length))

    for ($i = 0; $i -lt $value.Length; $i += $ChunkSize) {
        $take = [Math]::Min($ChunkSize, $value.Length - $i)
        $chunk = $value.Substring($i, $take)
        [void]$sb.AppendLine(("        value += ""{0}"";" -f $chunk))
    }

    [void]$sb.AppendLine(("        out.emplace(""{0}"", std::move(value));" -f $token))
    [void]$sb.AppendLine("    }")
}

[void]$sb.AppendLine("    return out;")
[void]$sb.AppendLine("}")
[void]$sb.AppendLine("}")

$outDir = Split-Path -Path $OutHeader -Parent
if (-not [string]::IsNullOrWhiteSpace($outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($OutHeader, $sb.ToString(), $utf8NoBom)

Write-Output "Generated $($entries.Count) icon entries -> $OutHeader"
