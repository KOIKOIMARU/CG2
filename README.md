# チーム制作向け DirectX 12 エンジン

このブランチは、個人制作で使用していたDirectX 12基盤をチーム制作向けに整理したものです。
通常起動では汎用エディタが開き、2.5D横スクロール基準シーンが自動再生されます。個人制作ゲームのコードや素材は含めていません。

## 最初に知っておくこと

- `project/include/engine/`と`project/src/engine/`が再利用するエンジン本体です。
- `project/include/app/`と`project/src/app/`は起動処理とシーン構成を行うアプリケーション層です。
- `project/include/samples/side_scroller/`と`project/src/samples/side_scroller/`は2.5D構成の開始例です。新作ゲームの本体を置く場所ではありません。
- `project/enc_temp_folder/`は保護領域です。閲覧、編集、生成、移動、削除を行わないでください。
- `generated/`はビルド生成物です。ゲームコードや素材を置かないでください。

より詳しい責務と依存方向は[エンジン構成資料](docs/ENGINE_ARCHITECTURE.md)を参照してください。
2.5D横スクロールの座標・操作・拡張方法は[横スクロール基準シーン](docs/SIDE_SCROLLER_STARTER.md)を参照してください。

## 必要な環境

- Windows 10またはWindows 11
- Visual Studio Community（C++デスクトップ開発）
- Windows SDK
- Git

## ビルド

Visual Studioで`project/CG2.sln`を開き、`x64`の`Debug`または`Release`を選択してビルドします。

コマンドから確認する場合はPowerShellで次を実行します。

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' project\CG2.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' project\CG2.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

実行ファイルは`generated/outputs/<Configuration>/CG2.exe`へ生成されます。

## 画面表示

- `F11`または`Alt+Enter`：ボーダーレスフルスクリーンと通常表示を切り替えます。
- もう一度同じキーを押すと、フルスクリーン前の位置とサイズへ戻ります。

## 起動後の流れ

1. `main.cpp`が`MyGame`を生成します。
2. `Framework::Run`が初期化とメインループを管理します。
3. `MyGame`が共通システムを`SceneManager`へ渡します。
4. 通常起動では`EditorScene`が開き、プレイモードを自動開始します。
5. エディタのプレイモードでは、`AppSceneFactory`から注入された`SideScrollRuntime`を使用します。

## 新しいゲームを組み込む場所

ゲーム固有のプレイヤー、敵、ルール、UI、ステージ処理は`engine/`へ追加せず、`app/`またはチームで作成したゲーム用フォルダへ置いてください。

エディタのプレイモードへゲームを接続する場合は、`IEditorPlayRuntime`を実装したクラスを用意し、`AppSceneFactory.cpp`の`SetPlayRuntimeFactory`へ生成処理を登録します。これにより、エンジン側は具体的なゲームクラスへ依存しません。

## エンジンの主な機能

- DirectX 12の初期化、フレーム描画、同期
- SpriteとObject3dの描画
- glTF/OBJモデル、スキニング、アニメーション
- テクスチャとモデルのキャッシュ
- SRV/UAVディスクリプタ管理
- GPUパーティクル
- シーン管理とシリアライズ
- ImGuiベースのシーンエディタ
- ポストエフェクト、ブルーム、アウトライン
- キーボードとマウス入力
- ゲーム操作と物理キーを分離する固定長入力マッピング
- 透視投影／正投影を切り替えられるカメラ
- AABBの交差判定とXZ平面の重なり判定
- サウンド再生

## チーム内の実装ルール

- `engine`は`app`や`samples`をインクルードしません。依存方向は必ずアプリケーションからエンジンへ向けます。
- ポインタを受け取る関数では、所有権を移すのか借用するのかをコメントで明示します。
- 新しい公開クラスと公開関数には、用途、呼び出し順、注意点を日本語で記述します。
- 描画リソースの状態、ディスクリプタ寿命、コマンドリスト順序に確信がない変更は、Debug Layerで確認してから共有します。
- 実行中に繰り返す処理では、可能な限り固定プールまたは事前確保を使用します。
- 機能追加は小さく分け、Debugビルドと実画面確認をしてから統合します。

## サンプルについて

`samples/side_scroller`はチーム作品の開始地点です。箱の仮プレイヤー、重力、ジャンプ、足場判定、Z固定、カメラ追従を最小構成で実装しています。ゲーム仕様が固まったら`app`側の正式なランタイムへ移し、サンプル固有処理を増やし続けないでください。

## 問題が起きたとき

1. Debug構成で再現する。
2. `generated/smoke-tests/`のアプリログとDirectX 12診断ログを確認する。
3. 変更前後のGPUリソース状態とディスクリプタ所有者を確認する。
4. 原因が不明なまま同期処理やリソースバリアを追加しない。
5. 再現手順、期待結果、実際の結果をチームへ共有する。
