# CG4 評価課題2 補足ドキュメント

## 実行方法

1. 提出フォルダ内の`実行ファイルセット/CG2.exe`を起動する。
2. タイトル画面で `Enter` を押してゲームを開始する。
3. 自機の左側を飛行する「護衛サポートユニット」で、各加点要素を確認できる。

開発環境から直接確認する場合は、
`generated/outputs/Release/CG2.exe`を作業場所の`project`をカレント
ディレクトリとして起動する。

Debugビルドでは、ゲーム中に `B` キーを押すと護衛ユニットの骨デバッグ表示を切り替えられる。

## ゲーム本編へ組み込んだ加点要素

### 1. Skinningモデルの表示

- `resources/human/walk.gltf` のスキニング済み人型モデルを、ゲーム中に自機と並走する護衛サポートユニットとして表示している。
- モデルが持つJointとWeightを読み取り、各頂点を骨の姿勢に従って変形している。
- 実装: `project/src/app/GameBonusActor.cpp`、`project/src/engine/3d/Object3d.cpp`

### 2. ComputeShaderによるスキニング

- 護衛ユニットの頂点変形は `Skinning.CS.hlsl` を用いたCompute Shaderで処理する。
- CPUはアニメーション時刻から各Jointの姿勢とMatrix Paletteを更新し、GPUが頂点ごとのWeightとJoint Indexを使って変形する。
- Compute結果の頂点バッファは、UAVからVertex Bufferへリソースバリアで状態遷移してから描画に使用する。
- 実装: `project/shaders/Skinning.CS.hlsl`、`project/src/engine/3d/Object3d.cpp`

### 3. MultiMesh & MultiMaterial対応

- Assimpから読み取った各Meshを、Index Offset・Index Count・Material Indexを持つDraw Rangeとして保持する。
- 描画時はDraw Range単位でMaterialのTextureを切り替え、`DrawIndexedInstanced`を実行する。
- 対応確認用の`multiMesh.obj`と`multiMaterial.obj`を、護衛ユニットの左右に浮く小型支援モジュールとしてゲーム中に表示している。
- 実装: `project/src/engine/3d/Model.cpp`、`project/src/app/GameBonusActor.cpp`

### 4. Animation補間

- Animation Channelの前後のKeyframeを時刻から検索し、移動と拡縮は線形補間、回転はQuaternionの球面線形補間で補間する。
- 補間したTransformから各JointのLocal Matrixを作り、親子階層を反映してSkeleton Space Matrixを更新する。
- ゲーム中の護衛ユニットへ歩行アニメーションをループ適用している。
- 実装: `project/src/engine/3d/Model.cpp`、`project/src/engine/3d/Object3d.cpp`

### 5. 骨のデバッグ表示

- Debugビルドでゲーム中に`B`キーを押すと、Jointを水色の球、親子Joint間のBoneを橙色の線として表示する。
- 表示用オブジェクトは毎フレーム、アニメーション後のSkeleton Space Matrixと護衛ユニットのWorld Matrixから位置・向き・長さを求めて更新する。
- 実装: `project/src/app/GameBonusActor.cpp`

### 6. 手からパーティクルを出す

- 護衛ユニットの左手JointをEmitter位置として、シアン・紫・ピンクの粒子を継続的に発生させる。
- 32個の固定オブジェクトプールを再利用し、ゲーム中の動的確保を避けている。
- 粒子は寿命、速度、縮小、色のばらつきを持ち、左手のアニメーションへ追従する。
- 実装: `project/src/app/GameBonusActor.cpp`

### 7. 武器を手に持たせる

- 右手Jointと右前腕Jointのワールド座標から腕の方向を求め、グリップ・ガード・発光ブレードで構成した武器を右手へ追従させる。
- アニメーションによって腕が動いても、武器の位置と向きが毎フレーム更新される。
- 実装: `project/src/app/GameBonusActor.cpp`

### 8. GPU Particle

- 自機の左右エンジン噴射をGPU Particleで描画し、左右それぞれに外炎とコアを持つ4グループをゲーム本編で使用している。
- Emit処理は1スレッドで全粒子を処理せず、64スレッドのCompute Shaderで粒子生成を並列化した。
- CPU側は発生数から必要なThread Group数を計算してDispatchする。
- 空きParticle IndexはGPU上のFree Listで管理し、枯渇時はCounterを復元して範囲外アクセスを防ぐ。
- 実装: `project/shaders/EmitParticle.CS.hlsl`、`project/src/engine/3d/ParticleManager.cpp`、`project/src/app/GameRuntime.cpp`

## その他の実装

### ゲーム状態に連動するPost Effect

- 通常時、フィーバー中、低HP、ゲームクリア、ゲームオーバーでPost Effectを切り替える。
- Vignette、Gaussian Filter、Radial Blur、Luminance/Depth Based Outline、Dissolveなどを実装し、単独の技術展示ではなくゲーム中の速度感や危険状態の表現に利用している。
- 実装: `project/src/engine/base/DirectXCommon.cpp`、`project/src/app/GameRuntime.cpp`、`project/shaders/`

### ゲーム進行中の確保抑制

- 手のパーティクルだけでなく、自機弾、敵弾、ヒットエフェクト、自機エンジン炎にも固定プールまたは事前生成を使用している。
- 頻繁に生成される演出でヒープ確保を繰り返さず、フレーム時間の急な変動を抑えている。

## 実装上の構成

- 加点要素は専用の展示シーンへ分離せず、`GameRuntime`が所有する`GameBonusActor`としてシューティング本編へ統合した。
- タイトル画面の段階的プリロードへ人型モデルと確認用モデルを追加し、ゲーム開始時にまとめて重い読込が走らないようにした。
- `GameBonusActor`は表示と追従演出を担当し、既存の敵、弾、衝突、ウェーブ進行へ依存しない構成にしている。
