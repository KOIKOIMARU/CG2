# CG5 評価課題1 - Post Effect

## 提出者

- クラス: LE3B
- 出席番号: 07
- 氏名: コイズミ リョウ

## 実行方法

1. `project/CG2.sln`をVisual Studioで開く。
2. 構成を`Debug`または`Release`、プラットフォームを`x64`にしてビルドする。
3. 生成された`CG2.exe`を実行する。
4. タイトル画面でEnterを押すとゲームとポストエフェクトのデモを開始する。

ゲーム開始時は必須項目のGrayscaleが選択される。画面中央上部に現在のエフェクト名と操作方法を表示する。

## 操作方法

- `F6`: 前のポストエフェクト
- `F7`: 次のポストエフェクト
- `F8`: Grayscaleへ戻す
- `F4`: ポストエフェクトの有効・無効を比較
- `F1`（Debug）: デバッグGUIを表示し、一覧から直接選択
- `F2`: タイトル画面へ戻る

## 必須内容

### Grayscale

シーンを一度オフスクリーンのRenderTextureへ描画し、フルスクリーン三角形のPixel ShaderでRGBを輝度へ変換して表示する。輝度係数は人間の視覚特性に合わせて`float3(0.2125, 0.7154, 0.0721)`を使用している。ゲーム開始時の標準エフェクトとして組み込み、実際のゲームシーン全体へ適用している。

## 加点要素

以下はすべてゲームへ組み込み済みで、F6/F7またはDebug GUIから切り替えられる。

- **Vignetting**: 画面中央からの距離を利用して周辺を暗くし、視線を中央へ誘導する。
- **BoxFilter 3x3 / 5x5**: 周囲のTexture Sampleを平均化する。カーネルサイズによるぼけ方の差も比較できる。
- **GaussianFilter**: 中央ほど大きい重みを持つGaussianカーネルで、BoxFilterより自然にぼかす。
- **LuminanceBasedOutline**: 近傍ピクセルとの輝度差から輪郭を抽出する。
- **DepthBasedOutline**: Depth TextureをShader Resourceとして参照し、深度差からオブジェクト境界を抽出する。Projection行列の逆行列を定数バッファで渡している。
- **Radial Blur**: 画面中心から外側へ複数回サンプリングし、レールシューティングの加速表現として使用する。
- **Dissolve**: Noise Textureと時間変化する閾値で画面を消去し、境界には色を付ける。
- **Random**: UV座標と時間から擬似乱数を生成し、時間変化するノイズ表現を行う。
- **Vignette + Smoothing**: ビネットと色補正を組み合わせた複合表現。

## その他のPost Effect

### 状態連動Game Tone

彩度、コントラスト、露出、黒レベル、色温度、距離フォグ、周辺減光を一つのShaderで制御する。通常、低HP、クリア、ゲームオーバーの状態に応じてパラメータを自動変更し、プレイヤーへゲーム状態を色で伝える。

### Multi-pass Bloom

高輝度抽出、縮小、Gaussian Blur、拡大、元画像との合成を複数のRenderTextureで行う。単純に画面全体を明るくせず、弾・エフェクト・ハイライト部分を中心に発光させるために利用している。

## 主な実装ファイル

- `project/src/engine/base/DirectXCommon.cpp`: RenderTexture、Resource Barrier、各Pipeline State、描画パス
- `project/include/engine/base/DirectXCommon.h`: ポストエフェクト用リソースとパラメータ
- `project/shaders/Grayscale.PS.hlsl`: 必須のGrayscale
- `project/shaders/Vignette.PS.hlsl`
- `project/shaders/BoxFilter.PS.hlsl`
- `project/shaders/BoxFilter5x5.PS.hlsl`
- `project/shaders/GaussianFilter.PS.hlsl`
- `project/shaders/LuminanceBasedOutline.PS.hlsl`
- `project/shaders/DepthBasedOutline.PS.hlsl`
- `project/shaders/RadialBlur.PS.hlsl`
- `project/shaders/Dissolve.PS.hlsl`
- `project/shaders/Random.PS.hlsl`
- `project/shaders/GameTone.PS.hlsl`
- `project/shaders/Bloom*.PS.hlsl`
- `project/src/app/GameRuntime.cpp`: ゲームへの組み込み、切替操作、現在のエフェクト表示

## DirectX 12での処理

ゲームの3D描画後、RenderTextureを`RENDER_TARGET`から`PIXEL_SHADER_RESOURCE`へ遷移してポストエフェクトへ入力する。DepthBasedOutlineやGame ToneではDepth Textureも同様にShader Resourceへ遷移する。処理終了後は次フレームの描画に備えて各リソースを元の状態へ戻す。Bloomでは縮小・Blur・拡大用のRenderTextureごとに状態を追跡し、各Passの前後でResource Barrierを設定している。
