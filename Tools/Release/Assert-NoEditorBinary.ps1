param(
    [Parameter(Mandatory = $true)]
    [string]$MainPath
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $MainPath -PathType Leaf)) {
    throw "Main.exe nao encontrado para a verificacao de editor: $MainPath"
}

[byte[]]$mainBytes = [System.IO.File]::ReadAllBytes($MainPath)
$binaryViews = @(
    [System.Text.Encoding]::ASCII.GetString($mainBytes),
    [System.Text.Encoding]::Unicode.GetString($mainBytes)
)
$editorMarkers = @(
    "--character-map-editor",
    "[Editor] Starting in editor mode",
    "Editor de cenario - World75"
)

foreach ($marker in $editorMarkers) {
    foreach ($view in $binaryViews) {
        if ($view.IndexOf($marker, [System.StringComparison]::Ordinal) -ge 0) {
            throw "Release bloqueada: Main.exe contem codigo do MU Editor (marcador '$marker'). Use o preset windows-x86-release com ENABLE_EDITOR=OFF."
        }
    }
}

$hash = (Get-FileHash -LiteralPath $MainPath -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "editor_guard: ok"
Write-Host "main_sha256: $hash"