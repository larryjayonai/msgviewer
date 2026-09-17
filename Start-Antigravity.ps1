[CmdletBinding()]
param([switch]$CheckOnly)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$promptPath = Join-Path $projectRoot 'ANTIGRAVITY_PROMPT.txt'
$handoffPath = Join-Path $projectRoot 'ANTIGRAVITY_HANDOFF.md'
$agyCommand = Get-Command agy -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$agyPath = if ($agyCommand) { $agyCommand.Source } else { Join-Path $env:LOCALAPPDATA 'agy\bin\agy.exe' }

foreach ($requiredPath in @($promptPath, $handoffPath, (Join-Path $projectRoot 'seed.yaml'), (Join-Path $projectRoot 'SPEC.md'), $agyPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required handoff file or CLI not found: $requiredPath"
    }
}

$initialPrompt = [System.IO.File]::ReadAllText($promptPath, [System.Text.Encoding]::UTF8)
if ([string]::IsNullOrWhiteSpace($initialPrompt)) { throw 'The handoff prompt is empty.' }

if ($CheckOnly) {
    [pscustomobject]@{
        ProjectRoot = $projectRoot
        AntigravityCli = $agyPath
        PromptFile = $promptPath
        PromptCharacters = $initialPrompt.Length
        Status = 'ready'
    } | ConvertTo-Json
    return
}

Push-Location -LiteralPath $projectRoot
try {
    & $agyPath --prompt-interactive $initialPrompt
    if ($LASTEXITCODE -ne 0) { throw "Antigravity exited with code $LASTEXITCODE" }
}
finally {
    Pop-Location
}
