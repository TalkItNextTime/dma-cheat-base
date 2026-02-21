param(
    [string]$JsonPath = "",
    [string]$OutHeader = "",
    [int]$ChunkSize = 120
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($JsonPath)) {
    $JsonPath = Join-Path $PSScriptRoot "..\Keys\key_icons.base64.json"
}

if ([string]::IsNullOrWhiteSpace($OutHeader)) {
    $OutHeader = Join-Path $PSScriptRoot "..\Source\Features\ESP\KeyIconsEmbedded.hpp"
}

if ($ChunkSize -lt 16) {
    throw "ChunkSize must be >= 16"
}

$tokenOrder = @("LB", "RB", "LRB", "Plus", "W", "A", "S", "D", "Ctrl", "Space", "Space_en")
$jsonText = Get-Content -LiteralPath $JsonPath -Raw -Encoding UTF8
$icons = $jsonText | ConvertFrom-Json

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("#pragma once")
[void]$sb.AppendLine("#include <string>")
[void]$sb.AppendLine("#include <unordered_map>")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("namespace EmbeddedKeyIcons")
[void]$sb.AppendLine("{")
[void]$sb.AppendLine("inline std::unordered_map<std::string, std::string> BuildKeyIconsBase64Map()")
[void]$sb.AppendLine("{")
[void]$sb.AppendLine("    std::unordered_map<std::string, std::string> out{};")
[void]$sb.AppendLine("    out.reserve(11);")

foreach ($token in $tokenOrder) {
    $value = [string]$icons.$token
    if ([string]::IsNullOrWhiteSpace($value)) {
        throw "Missing or empty token in json: $token"
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

$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($OutHeader, $sb.ToString(), $utf8NoBom)

Write-Output "Generated: $OutHeader"
