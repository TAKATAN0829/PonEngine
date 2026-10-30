# 本番のWinMainを含めて再ビルドし、別の起動コードでは見逃すABIの不整合も検証する。
param(
    [ValidateSet('Debug', 'Development', 'Release')]
    [string]$Configuration = 'Debug'
)
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path $PSScriptRoot -Parent
$buildTool = 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe'
if (!(Test-Path -LiteralPath $buildTool)) { throw 'Visual StudioのMSBuildが見つかりません。' }
& $buildTool (Join-Path $repositoryRoot 'project/PonEngine.slnx') /t:Rebuild "/p:Configuration=$Configuration" /p:Platform=x64 /m:1 /nr:false /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { throw '本番実行ファイルの再ビルドに失敗しました。' }

# 配置・プリセット・レイアウトの保存先を隔離し、ユーザーの編集データへ書き込まない。
$runtimeDirectory = Join-Path $repositoryRoot "generated/editor-startup-tests/$Configuration/runtime"
New-Item -ItemType Directory -Force $runtimeDirectory | Out-Null
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'project/CG2/resources') -Destination $runtimeDirectory -Recurse -Force
$executable = Join-Path $repositoryRoot "generated/outputs/$Configuration/CG2.exe"
$testProcess = Start-Process -FilePath $executable -ArgumentList '--smoke-test' -WorkingDirectory $runtimeDirectory -WindowStyle Hidden -PassThru
if (!$testProcess.WaitForExit(60000)) {
    # このスクリプト自身が起動した試験プロセスだけを終了する。
    $testProcess.Kill()
    throw '本番起動試験が60秒以内に終了しませんでした。'
}
if ($testProcess.ExitCode -ne 0) { throw "本番起動試験が失敗しました（終了コード: $($testProcess.ExitCode)）。" }
Write-Output "Production startup test passed ($Configuration, 90 frames)."
