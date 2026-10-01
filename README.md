# アズレイド / AZRAID

個人制作の3Dレールシューティングゲーム。射撃・回避・残像連撃を使って戦います。
Windows向けに、C++ / DirectX 12で開発しています。

リポジトリ: [KOIKOIMARU/AZRAID](https://github.com/KOIKOIMARU/AZRAID)

## 開発・起動

- ソリューション: `project/CG2.sln`
- プロジェクト: `project/CG2.vcxproj`
- ビルド環境: Visual StudioのC++開発環境、MSVC v145、Windows SDK 10.0.26100.0
- 構成: `Debug / x64` または `Release / x64`

既存のビルド構成を維持するため、フォルダ・ソリューション・実行ファイルの内部名は `CG2` のままです。
ゲームの正式名称は「アズレイド」、英字表記は「AZRAID」です。

Releaseの出力先は `generated/outputs/Release/CG2.exe` です。
リソースを読み込めるよう、作業ディレクトリを `project` にして起動してください。
リポジトリのルートからPowerShellで起動する場合:

```powershell
Start-Process -FilePath '.\generated\outputs\Release\CG2.exe' -WorkingDirectory '.\project'
```

## 操作

| 操作 | 入力 |
|---|---|
| 移動 | W / A / S / D |
| 照準 | マウス |
| 射撃・連射 | SPACE / 長押し |
| 回避 | A または D + SHIFT |
| 残像連撃 | Q |
| ポーズ | ESC |
| 全画面切替 | F11 / Alt + Enter |

タイトル画面からチュートリアルと操作方法を確認できます。

## 素材

第三者素材の利用条件は各素材のクレジット・ライセンスを参照してください。
戦闘効果音の配布元は [CREDITS.md](project/resources/audio/combat/CREDITS.md) に記載しています。
開発用の元素材と試聴データは `assets_source/` と `generated/` に置き、Gitの管理対象から除外しています。
