# エンジン構成と責務

## 依存方向

```text
main
  ↓
app（起動・シーン構成）
  ├─→ engine（再利用基盤）
  └─→ samples/side_scroller（2.5D開始例）
          ↓
        engine
```

`engine`から`app`または`samples`を参照してはいけません。ゲーム固有機能をエディタから実行したい場合は、`IEditorPlayRuntime`のようなエンジン側インターフェースをアプリケーション側が実装し、生成関数を注入します。

## フォルダごとの責務

### `engine/base`

ウィンドウ、DirectX 12デバイス、フレーム同期、ImGui、ログなど、すべての描画機能が利用する土台です。`Framework`が主要オブジェクトを所有します。

### `engine/2d`

Sprite描画を担当します。Spriteを生成する前に`SpriteCommon`を初期化し、各Spriteへ共通オブジェクトを渡します。

### `engine/3d`

Camera、Model、Object3d、Skybox、Texture、Particleを担当します。`ModelManager`と`TextureManager`はキャッシュを所有し、Object3dは取得したModelを借用します。

### `engine/io`

入力状態を毎フレーム更新します。`PushKey`は継続入力、`TriggerKey`は押した瞬間の入力です。

### `engine/scene`

シーンのライフサイクルと遷移、シーンファイルの保存・読込を担当します。ゲーム固有シーンの生成規則はここではなく`app/AppSceneFactory`に置きます。

### `engine/editor`

ImGuiベースの編集操作とプレイモード制御を担当します。`EditorPlayController`は具体的なゲームを生成せず、`IEditorPlayRuntime`のファクトリを受け取ります。

### `app`

実行ファイル固有の構成ルートです。起動シーンと2.5Dランタイムをエディタへ接続する処理を置きます。

### `samples/side_scroller`

3D描画をX/Y平面へ制限する開始例です。移動、ジャンプ、AABB衝突、カメラ追従はゲーム側の処理であり、エンジンAPIではありません。

## 所有権

- `Framework`はWinApp、DirectXCommon、SrvManager、ImGuiManager、Input、SpriteCommonを所有します。
- `SceneManager`は現在シーンと準備中シーンを`unique_ptr`で所有します。
- `BaseScene::SetSystems`で渡されるポインタは借用です。シーン側からdeleteしません。
- `ModelManager`と`TextureManager`がキャッシュ済みリソースを所有します。
- `Object3d`はModelを借用し、自分の変換・マテリアル用GPUリソースを所有します。
- SRV/UAVインデックスを直接Allocateしたクラスは、不要になった時点で同じインデックスを一度だけFreeします。

## 1フレームの処理順

```text
Framework::Update
  ↓
ImGuiManager::Begin
  ↓
SceneManager::Update
  ↓
ImGuiManager::End
  ↓
DirectXCommon::PreDraw
  ↓
SrvManager::PreDraw
  ↓
SceneManager::Draw
  ↓
ポストエフェクト
  ↓
ImGuiManager::Draw
  ↓
DirectXCommon::PostDraw / Present / GPU同期
```

描画関数を追加するときは、この順番とコマンドリストの状態を維持してください。

## 新しいシーンを追加する手順

1. `BaseScene`を継承したクラスをアプリケーション側へ作る。
2. `Initialize`で必要なリソースを準備する。
3. `Update`でゲーム状態を更新する。
4. `Draw`では描画コマンドだけを積む。
5. `Finalize`でシーン所有のリソースを解放する。
6. `SceneType`へ識別子を追加する。
7. `AppSceneFactory::CreateScene`へ生成処理を追加する。

## レビュー時の確認項目

- engine層からゲーム固有フォルダをインクルードしていないか。
- 生ポインタの所有者が分かるか。
- GPUリソースの使用中に破棄または上書きしていないか。
- SRV/UAVを二重解放していないか。
- Update中の動的確保を避けられないか。
- DebugとReleaseの両方でビルドできるか。
- 描画変更を実画面で確認したか。
