# 本番と同じコードで実GPU描画を試験し、設定ファイルはgenerated配下のコピーだけを利用する。
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path $PSScriptRoot -Parent
$buildTool = 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe'
if (!(Test-Path -LiteralPath $buildTool)) { throw 'Visual StudioのMSBuildが見つかりません。' }
& $buildTool (Join-Path $repositoryRoot 'project/PonEngine.slnx') /p:Configuration=Debug /p:Platform=x64 "/p:ForceImportBeforeCppTargets=$PSScriptRoot/EditorGraphicsTests.targets" /m:1 /nr:false /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { throw '描画試験のビルドに失敗しました。' }
$runtimeDirectory = Join-Path $repositoryRoot 'generated/editor-graphics-tests/runtime'
New-Item -ItemType Directory -Force $runtimeDirectory | Out-Null
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'project/CG2/resources') -Destination $runtimeDirectory -Recurse -Force
Push-Location -LiteralPath $runtimeDirectory
try {
    & (Join-Path $repositoryRoot 'generated/editor-graphics-tests/bin/EditorGraphicsTests.exe')
    if ($LASTEXITCODE -ne 0) { throw '描画またはParticleの動作試験に失敗しました。' }
} finally {
    Pop-Location
}
