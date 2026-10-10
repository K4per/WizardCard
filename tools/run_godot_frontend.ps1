param(
    [string]$EditorPath = '',
    [string]$AssetsPath = '',
    [string]$UserDirectory = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $EditorPath) {
    $EditorPath = Join-Path $projectRoot '.cache/godot-portable/Godot_v4.7.2-stable_win64_console.exe'
}
if (-not (Test-Path -LiteralPath $EditorPath -PathType Leaf)) {
    throw 'Provide -EditorPath for the pinned Godot 4.7.2 editor.'
}
$editorVersion = (& $EditorPath --version | Out-String).Trim()
if ($editorVersion -ne '4.7.2.stable.official.ed1daf0bf') {
    throw "Unexpected editor version: $editorVersion"
}
$bridgePath = Join-Path $projectRoot 'godot/bin/wizard_godot_bridge.debug.dll'
if (-not (Test-Path -LiteralPath $bridgePath -PathType Leaf)) {
    throw 'Build wizard_godot_bridge with GODOTCPP_TARGET=template_debug first; see godot/README.md.'
}
& $EditorPath --headless --path (Join-Path $projectRoot 'godot') --import --frame-delay 1000
if ($LASTEXITCODE -ne 0) { throw 'Frontend import failed; inspect the Godot diagnostics above.' }
$frontendArguments = @('--path', (Join-Path $projectRoot 'godot'))
if ($AssetsPath -or $UserDirectory) {
    $frontendArguments += '--'
    if ($AssetsPath) { $frontendArguments += @('--assets', $AssetsPath) }
    if ($UserDirectory) { $frontendArguments += @('--user-data', $UserDirectory) }
}
& $EditorPath @frontendArguments
exit $LASTEXITCODE
