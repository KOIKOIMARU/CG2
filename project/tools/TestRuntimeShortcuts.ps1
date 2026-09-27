[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$output = [IO.Path]::GetFullPath((Join-Path $project '..\generated\shortcut-tests'))
New-Item -ItemType Directory -Path $output -Force | Out-Null

# ゲーム本体の関数をそのまま取り込み、OS入力に依存せずビルド別の挙動を確認する。
$runtime = Get-Content -LiteralPath (Join-Path $project 'src\app\GameRuntime.cpp') -Raw
$start = $runtime.IndexOf('bool GameRuntime::HandleRuntimeShortcuts()')
$end = $runtime.IndexOf('void GameRuntime::UpdateStageDirector()', $start)
if ($start -lt 0 -or $end -le $start) { throw 'Runtime shortcut function not found.' }
$function = $runtime.Substring($start, $end - $start)
$stub = @'
enum { DIK_ESCAPE=1, DIK_H, DIK_RETURN, DIK_R, DIK_F1, DIK_F2, DIK_F3, DIK_F4, DIK_F5 };
struct Input { int pressed=0; bool TriggerKey(int key) { return key == pressed; } };
namespace MenuUi { bool Pressed(Input* input, int key) { return input && input->TriggerKey(key); } }
struct GameRuntime {
    Input input; Input* input_=&input;
    bool menuInputConsumed_=false, isGameOver_=false, isGameClear_=false;
    bool showControlsHelp_=false, isPaused_=false, isRetryRequested_=false;
    bool isEditorOverlayVisible_=false, isPerformanceOverlayVisible_=false;
    bool isPostEffectBypassEnabled_=false, showSkybox_=true, isExitRequested_=false;
    int pauseSelectedItem_=0, resultTransitionTimer_=0;
    bool HandleRuntimeShortcuts();
};
'@
$tests = @'
int main() {
    for (int key=DIK_F1; key<=DIK_F5; ++key) {
        GameRuntime game; game.input.pressed=key;
        const bool consumed=game.HandleRuntimeShortcuts();
#ifdef ENABLE_DEBUG_GUI
        if (!consumed) return 10+key;
        if (game.isEditorOverlayVisible_ != (key==DIK_F1) ||
            game.isExitRequested_ != (key==DIK_F2) ||
            game.isPerformanceOverlayVisible_ != (key==DIK_F3) ||
            game.isPostEffectBypassEnabled_ != (key==DIK_F4) ||
            game.showSkybox_ != (key!=DIK_F5)) return 30+key;
#else
        if (consumed || game.isEditorOverlayVisible_ || game.isExitRequested_ ||
            game.isPerformanceOverlayVisible_ || game.isPostEffectBypassEnabled_ ||
            !game.showSkybox_) return 50+key;
#endif
    }
    GameRuntime game;
    game.input.pressed=DIK_ESCAPE; game.HandleRuntimeShortcuts();
    if (!game.isPaused_) return 71;
    game.HandleRuntimeShortcuts(); if (game.isPaused_) return 72;
    game.input.pressed=DIK_H; game.HandleRuntimeShortcuts();
    if (!game.showControlsHelp_ || !game.isPaused_) return 73;
    game.input.pressed=DIK_RETURN; game.HandleRuntimeShortcuts();
    if (game.showControlsHelp_ || !game.menuInputConsumed_) return 74;
    game.isGameOver_=true; game.input.pressed=DIK_R;
    if (!game.HandleRuntimeShortcuts() || !game.isRetryRequested_) return 75;
    return 0;
}
'@
$source = Join-Path $output 'RuntimeShortcuts.cpp'
[IO.File]::WriteAllText($source, $stub + "`n" + $function + "`n" + $tests, [Text.UTF8Encoding]::new($false))
$vc = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$sdk = 'C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0'
[xml]$configuration = Get-Content -LiteralPath (Join-Path $project 'CG2.vcxproj') -Raw
foreach ($name in @('Debug', 'Release', 'Development')) {
    $group = $configuration.Project.ItemDefinitionGroup | Where-Object { $_.Condition -like "*'$name|x64'*" }
    $definitions = @($group.ClCompile.PreprocessorDefinitions.Split(';') | Where-Object { $_ -and $_ -notlike '%*' } | ForEach-Object { "/D$_" })
    $exe = Join-Path $output "$name.exe"
    & (Join-Path $vc 'bin\Hostx64\x64\cl.exe') /nologo /W4 /WX /EHsc /MT /utf-8 $definitions $source "/Fo$output\$name.obj" "/Fe$exe" /link "/LIBPATH:$vc\lib\x64" "/LIBPATH:$sdk\ucrt\x64" "/LIBPATH:$sdk\um\x64"
    if ($LASTEXITCODE -ne 0) { throw "$name shortcut test compilation failed." }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "$name shortcut regression failed: $LASTEXITCODE" }
    Write-Output "SHORTCUT_TEST_PASS configuration=$name debug_keys=F1-F5 normal_keys=Escape,H,Enter,R"
}
