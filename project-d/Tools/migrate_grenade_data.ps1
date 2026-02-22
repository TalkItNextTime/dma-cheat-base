param(
    [string]$DataDir = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($DataDir)) {
    $DataDir = Join-Path $PSScriptRoot "..\GrenadeData"
}

$dataRoot = (Resolve-Path -LiteralPath $DataDir).Path

function New-CjkText([int[]]$Codes) {
    return -join ($Codes | ForEach-Object { [char]$_ })
}

$KW_DOUBLE_KEY = New-CjkText @(0x53CC, 0x952E)
$KW_DOUBLE_PRESS = New-CjkText @(0x53CC, 0x6309)
$KW_DOUBLE = New-CjkText @(0x53CC)
$KW_LEFT_RIGHT = New-CjkText @(0x5DE6, 0x53F3, 0x952E)
$KW_RIGHT_KEY = New-CjkText @(0x53F3, 0x952E)

$KW_CROUCH_DOWN = New-CjkText @(0x8E72, 0x4E0B)
$KW_DOWN_CROUCH = New-CjkText @(0x4E0B, 0x8E72)
$KW_CROUCH = New-CjkText @(0x8E72)
$KW_JUMP_THROW = New-CjkText @(0x8DF3, 0x6295)
$KW_RUN_THROW = New-CjkText @(0x8DD1, 0x6295)
$KW_RUN_JUMP = New-CjkText @(0x8DD1, 0x8DF3)
$KW_RUN_JUMP_THROW = New-CjkText @(0x8DD1, 0x8DF3, 0x6295)
$KW_RUN_TO = New-CjkText @(0x8DD1, 0x5230)
$KW_WALK_TO = New-CjkText @(0x8D70, 0x5230)
$KW_RUN_UNTIL = New-CjkText @(0x8DD1, 0x81F3)
$KW_WALK_UNTIL = New-CjkText @(0x8D70, 0x81F3)
$KW_ARRIVE = New-CjkText @(0x5230, 0x8FBE)
$KW_TO = New-CjkText @(0x5230)
$KW_UNTIL = New-CjkText @(0x81F3)
$KW_SILENT_WALK = New-CjkText @(0x9759, 0x6B65)
$KW_SILENT_MOVE = New-CjkText @(0x9759, 0x8D70)
$KW_SILENT = New-CjkText @(0x9759)
$KW_WALK = New-CjkText @(0x8D70)

function Trim-Ascii([string]$Value) {
    if ($null -eq $Value) { return "" }
    return $Value.Trim()
}

function To-LowerInvariant([string]$Value) {
    if ($null -eq $Value) { return "" }
    return $Value.ToLowerInvariant()
}

function New-ThrowKeyState {
    return [ordered]@{
        Mouse = "LB"
        W = $false
        A = $false
        S = $false
        D = $false
        Shift = $false
        Ctrl = $false
        Space = $false
    }
}

function Parse-ThrowType([string]$ThrowTypeRaw) {
    $state = New-ThrowKeyState
    $throwType = Trim-Ascii $ThrowTypeRaw
    if ([string]::IsNullOrWhiteSpace($throwType)) {
        return $state
    }

    $lower = To-LowerInvariant $throwType
    switch ($lower) {
        "standthrow" { return $state }
        "jumpthrow" { $state.Space = $true; return $state }
        "runthrow" { $state.W = $true; return $state }
        "runjumpthrow" { $state.W = $true; $state.Space = $true; return $state }
        "runjump" { $state.W = $true; $state.Space = $true; return $state }
        "walkthrow" { $state.Shift = $true; return $state }
        "shiftthrow" { $state.Shift = $true; return $state }
        "silentwalkthrow" { $state.Shift = $true; return $state }
        "walkjumpthrow" { $state.Shift = $true; $state.Space = $true; return $state }
        "walkjump" { $state.Shift = $true; $state.Space = $true; return $state }
        "shiftjumpthrow" { $state.Shift = $true; $state.Space = $true; return $state }
        "silentwalkjumpthrow" { $state.Shift = $true; $state.Space = $true; return $state }
    }

    $parts = $throwType -split '\+'
    foreach ($part in $parts) {
        $token = (Trim-Ascii $part).ToUpperInvariant()
        if ([string]::IsNullOrWhiteSpace($token)) { continue }

        switch ($token) {
            "LB" { $state.Mouse = "LB"; continue }
            "LMB" { $state.Mouse = "LB"; continue }
            "LEFT" { $state.Mouse = "LB"; continue }
            "LEFTCLICK" { $state.Mouse = "LB"; continue }

            "RB" { $state.Mouse = "RB"; continue }
            "RMB" { $state.Mouse = "RB"; continue }
            "RIGHT" { $state.Mouse = "RB"; continue }
            "RIGHTCLICK" { $state.Mouse = "RB"; continue }

            "LRB" { $state.Mouse = "LRB"; continue }
            "DUAL" { $state.Mouse = "LRB"; continue }
            "BOTH" { $state.Mouse = "LRB"; continue }

            "W" { $state.W = $true; continue }
            "A" { $state.A = $true; continue }
            "S" { $state.S = $true; continue }
            "D" { $state.D = $true; continue }
            "SHIFT" { $state.Shift = $true; continue }
            "SHIFT_EN" { $state.Shift = $true; continue }
            "SHIFTEN" { $state.Shift = $true; continue }
            "SHIFT-EN" { $state.Shift = $true; continue }
            "WALK" { $state.Shift = $true; continue }
            "SLOWWALK" { $state.Shift = $true; continue }
            "SILENTWALK" { $state.Shift = $true; continue }
            "CTRL" { $state.Ctrl = $true; continue }
            "CONTROL" { $state.Ctrl = $true; continue }
            "CROUCH" { $state.Ctrl = $true; continue }
            "DUCK" { $state.Ctrl = $true; continue }
            "SPACE" { $state.Space = $true; continue }
            "JUMP" { $state.Space = $true; continue }
        }
    }

    return $state
}

function Build-CanonicalThrowType($State) {
    $mouse = if ([string]::IsNullOrWhiteSpace($State.Mouse)) { "LB" } else { $State.Mouse }
    $result = $mouse

    if ($State.W) { $result += "+W" }
    if ($State.A) { $result += "+A" }
    if ($State.S) { $result += "+S" }
    if ($State.D) { $result += "+D" }
    if ($State.Shift) { $result += "+Shift" }
    if ($State.Ctrl) { $result += "+Ctrl" }
    if ($State.Space) { $result += "+Space" }

    return $result
}

function Contains-AnyKeyword([string]$Text, [string[]]$Keywords) {
    if ([string]::IsNullOrWhiteSpace($Text)) { return $false }

    $lower = To-LowerInvariant $Text
    foreach ($keyword in $Keywords) {
        if ([string]::IsNullOrWhiteSpace($keyword)) { continue }
        if ($Text.Contains($keyword) -or $lower.Contains((To-LowerInvariant $keyword))) {
            return $true
        }
    }
    return $false
}

function Add-RemarkSegment([string]$Current, [string]$Segment) {
    $currentTrimmed = Trim-Ascii $Current
    $segmentTrimmed = Trim-Ascii $Segment

    if ([string]::IsNullOrWhiteSpace($segmentTrimmed)) { return $currentTrimmed }
    if ([string]::IsNullOrWhiteSpace($currentTrimmed)) { return $segmentTrimmed }
    if ($currentTrimmed.Contains($segmentTrimmed) -or $segmentTrimmed.Contains($currentTrimmed)) { return $currentTrimmed }

    return "$currentTrimmed | $segmentTrimmed"
}

function Split-TrailingBracketRemark([string]$Name) {
    $trimmed = Trim-Ascii $Name
    if ([string]::IsNullOrWhiteSpace($trimmed)) {
        return [ordered]@{
            name = ""
            bracket_remark = ""
        }
    }

    $pattern = '^(?<base>.*?)[\s]*[\(\[\uFF08\u3010](?<remark>[^()\[\]\uFF08\uFF09\u3010\u3011]*)[\)\]\uFF09\u3011][\s]*$'
    $match = [regex]::Match($trimmed, $pattern)
    if ($match.Success) {
        $base = Trim-Ascii $match.Groups["base"].Value
        $remark = Trim-Ascii $match.Groups["remark"].Value
        if (-not [string]::IsNullOrWhiteSpace($base)) {
            return [ordered]@{
                name = $base
                bracket_remark = $remark
            }
        }
    }

    return [ordered]@{
        name = $trimmed
        bracket_remark = ""
    }
}

function Apply-ThrowHintsFromText([string]$Text, $State) {
    if ([string]::IsNullOrWhiteSpace($Text)) { return }

    if (Contains-AnyKeyword $Text @($KW_DOUBLE_KEY, $KW_DOUBLE_PRESS, $KW_DOUBLE, $KW_LEFT_RIGHT, "dual", "both", "lrb")) {
        $State.Mouse = "LRB"
    }
    elseif (Contains-AnyKeyword $Text @($KW_RIGHT_KEY, "right click", "rmb", "rb")) {
        $State.Mouse = "RB"
    }

    if (Contains-AnyKeyword $Text @($KW_CROUCH_DOWN, $KW_DOWN_CROUCH, $KW_CROUCH, "ctrl", "crouch", "duck")) {
        $State.Ctrl = $true
    }

    $hasRunJump = Contains-AnyKeyword $Text @(
        $KW_RUN_JUMP_THROW,
        $KW_RUN_JUMP,
        "runjumpthrow",
        "runjump",
        "run jump throw",
        "run jump",
        "run-jump"
    )
    $hasRun = Contains-AnyKeyword $Text @(
        $KW_RUN_THROW,
        "runthrow",
        "run throw",
        "run-throw"
    )
    $hasJump = Contains-AnyKeyword $Text @(
        $KW_JUMP_THROW,
        "jumpthrow",
        "jump throw",
        "jump-throw"
    )

    if ($hasRunJump) {
        $State.W = $true
        $State.Space = $true
        return
    }

    $hasSilentWalk = Contains-AnyKeyword $Text @(
        $KW_SILENT_WALK,
        $KW_SILENT_MOVE,
        $KW_SILENT,
        $KW_WALK,
        "walk",
        "shift",
        "slowwalk",
        "silentwalk"
    )

    if ($hasRun) { $State.W = $true }
    if ($hasJump) { $State.Space = $true }
    if ($hasSilentWalk) {
        $State.Shift = $true
        $State.W = $false
    }
}

function Extract-ArrivalRemark([string]$Name) {
    if ([string]::IsNullOrWhiteSpace($Name)) { return "" }

    $rules = @(
        @{ keyword = $KW_RUN_TO; lower = $false },
        @{ keyword = $KW_WALK_TO; lower = $false },
        @{ keyword = $KW_RUN_UNTIL; lower = $false },
        @{ keyword = $KW_WALK_UNTIL; lower = $false },
        @{ keyword = $KW_ARRIVE; lower = $false },
        @{ keyword = "run to"; lower = $true },
        @{ keyword = "walk to"; lower = $true },
        @{ keyword = "go to"; lower = $true },
        @{ keyword = $KW_TO; lower = $false },
        @{ keyword = $KW_UNTIL; lower = $false },
        @{ keyword = "to "; lower = $true }
    )

    $lowerName = To-LowerInvariant $Name
    $bestPos = [int]::MaxValue
    foreach ($rule in $rules) {
        $needle = [string]$rule.keyword
        if ([string]::IsNullOrWhiteSpace($needle)) { continue }
        $pos = if ($rule.lower) { $lowerName.IndexOf($needle) } else { $Name.IndexOf($needle) }
        if ($pos -ge 0 -and $pos -lt $bestPos) {
            $bestPos = $pos
        }
    }

    if ($bestPos -eq [int]::MaxValue) { return "" }
    return Trim-Ascii ($Name.Substring($bestPos))
}

function Normalize-ThrowNameAndRemark([string]$ThrowType, [string]$Name, [string]$Remark) {
    $state = Parse-ThrowType $ThrowType
    $split = Split-TrailingBracketRemark $Name
    $normalizedName = Trim-Ascii ([string]$split.name)
    $bracketRemark = Trim-Ascii ([string]$split.bracket_remark)

    if ([string]::IsNullOrWhiteSpace($normalizedName)) {
        $normalizedName = "Unnamed"
    }

    Apply-ThrowHintsFromText $normalizedName $state
    Apply-ThrowHintsFromText $bracketRemark $state

    $finalRemark = Trim-Ascii $Remark
    if (-not [string]::IsNullOrWhiteSpace($finalRemark)) {
        Apply-ThrowHintsFromText $finalRemark $state
    }

    $finalRemark = Add-RemarkSegment $finalRemark $bracketRemark
    $arrivalRemark = Extract-ArrivalRemark $normalizedName
    $finalRemark = Add-RemarkSegment $finalRemark $arrivalRemark

    return [ordered]@{
        name = $normalizedName
        throw_type = (Build-CanonicalThrowType $state)
        remark = $finalRemark
    }
}

function Repair-MalformedNameLines([string]$RawText) {
    if ([string]::IsNullOrWhiteSpace($RawText)) { return $RawText }

    $lines = @($RawText -split "`n")
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        if ($line -notmatch '"name"\s*:') { continue }

        $quoteCount = 0
        $escaped = $false
        foreach ($ch in $line.ToCharArray()) {
            if ($ch -eq '\' -and -not $escaped) {
                $escaped = $true
                continue
            }
            if ($ch -eq '"' -and -not $escaped) {
                $quoteCount++
            }
            $escaped = $false
        }

        if (($quoteCount % 2) -ne 0) {
            $commaIndex = $line.LastIndexOf(',')
            if ($commaIndex -ge 0) {
                $line = $line.Insert($commaIndex, '"')
            }
            else {
                $line += '"'
            }
            $lines[$i] = $line
        }
    }

    return ($lines -join "`n")
}

$files = Get-ChildItem -Path $dataRoot -Filter "*.json" -File | Sort-Object Name
if ($files.Count -eq 0) {
    throw "No grenade json files found in: $dataRoot"
}

$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$migrated = 0

foreach ($file in $files) {
    $raw = [System.IO.File]::ReadAllText($file.FullName, [System.Text.Encoding]::UTF8)
    $repaired = Repair-MalformedNameLines $raw

    $root = $null
    try {
        $root = $repaired | ConvertFrom-Json
    }
    catch {
        throw "Failed to parse '$($file.Name)': $($_.Exception.Message)"
    }

    $mapName = ""
    if ($null -ne $root.map_name) {
        $mapName = [string]$root.map_name
    }
    if ([string]::IsNullOrWhiteSpace($mapName)) {
        $mapName = [System.IO.Path]::GetFileNameWithoutExtension($file.Name)
    }

    $outRoot = [ordered]@{
        map_name = $mapName
        grenades = @()
    }

    $items = @()
    if ($null -ne $root.grenades) {
        $items = @($root.grenades)
    }

    $nextId = 1
    foreach ($item in $items) {
        if ($null -eq $item) { continue }

        $id = $nextId
        if ($null -ne $item.id) {
            try {
                $id = [int]$item.id
            }
            catch {
                $id = $nextId
            }
        }
        if ($id -le 0) { $id = $nextId }
        $nextId = [Math]::Max($nextId + 1, $id + 1)

        $name = if ($null -ne $item.name) { [string]$item.name } else { "" }
        if ([string]::IsNullOrWhiteSpace($name)) { $name = "Unnamed" }

        $type = if ($null -ne $item.type) { [string]$item.type } else { "Unknown" }
        if ([string]::IsNullOrWhiteSpace($type)) { $type = "Unknown" }

        $throwType = if ($null -ne $item.throw_type) { [string]$item.throw_type } else { "LB" }
        $remark = if ($null -ne $item.remark) { [string]$item.remark } else { "" }
        $normalized = Normalize-ThrowNameAndRemark $throwType $name $remark

        $position = [ordered]@{
            x = [double]$item.position.x
            y = [double]$item.position.y
            z = [double]$item.position.z
        }
        $aimTarget = [ordered]@{
            x = [double]$item.aim_target.x
            y = [double]$item.aim_target.y
            z = [double]$item.aim_target.z
        }

        $outItem = [ordered]@{
            id = $id
            type = $type
            name = [string]$normalized.name
            remark = [string]$normalized.remark
            position = $position
            aim_target = $aimTarget
            throw_type = [string]$normalized.throw_type
        }

        $outRoot.grenades += $outItem
    }

    $jsonText = $outRoot | ConvertTo-Json -Depth 100
    [System.IO.File]::WriteAllText($file.FullName, $jsonText, $utf8NoBom)
    $migrated++
}

Write-Output "Migrated files: $migrated"
