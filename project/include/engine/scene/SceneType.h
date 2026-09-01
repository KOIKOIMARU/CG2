#pragma once

// アプリケーションが生成可能なシーン種別。
// 新しい種別を追加した場合はapp/AppSceneFactoryの生成分岐も追加する。
enum class SceneType {
    // チーム制作で通常起動する汎用エディタ。
    Editor,
};
