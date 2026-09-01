// 全画面ポストエフェクトで共有する、画面位置とUVの頂点出力。
struct VertexShaderOutput {
    float32_t4 position : SV_POSITION; // ラスタライザーへ渡すクリップ座標
    float32_t2 texcoord : TEXCOORD0;   // 入力画像を読む0～1のUV座標
};
