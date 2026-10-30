# Editorの使い方

Visual Studioで `CG2` をスタートアップにし、DebugまたはDevelopmentで起動する。作業ディレクトリは `project/CG2`。ReleaseはEditorなしでゲームを起動する。

## 画面配置と文字

```text
┌───────────────────────────────────────────────┐
│ Scene / Viewメニュー・再生操作                 │
├──────────┬─────────────────────┬──────────────┤
│Hierarchy │ SceneView / GameView│ Inspector    │
│          │                     │              │
│          │ 描画・選択・移動      │              │
├──────────┴─────────────────────┴──────────────┤
│ Project / Console                             │
└───────────────────────────────────────────────┘
```

タイトルバーをドラッグして配置を変更できる。「表示 / View → 初期レイアウトに戻す」で標準配置へ戻す。配置は `editor-layout.ini` に保存される。

標準表示はHierarchy・Inspector・Project・Console・SceneView・GameViewの6タブ。SceneViewとGameViewは中央で切り替える。再生操作は上部に置き、Toolbarタブは作らない。選択時のタブ背景・上線・タイトルバーの強い色は使わない。

Particle Editor・World Settings・Cameraは「表示 / View」から必要なときだけ開く。各画面は閉じるボタンで閉じられる。初期レイアウトへ戻すと補助画面も閉じる。

既定フォントはWindowsのメイリオ18px。倍率は0.85固定。フォントファイルをリポジトリへコピーする必要はない。

## SceneとGame

- Scene：Editor用カメラで配置を編集する。モデルをクリックして選択し、XYZ軸をドラッグして移動する。
- Game：ゲームカメラとSpriteを含めた表示を確認する。
- Scene画像のサイズ変更に合わせて描画先・投影行列・クリック判定も更新する。
- Scene上の右ドラッグで視点回転、WASDとQEで移動、Fまたは「注視」で選択対象へカメラを移す。
- Ctrlを押して移動するとSnapの間隔へ合わせ、Escで移動開始位置へ戻す。
- 描画されないオブジェクトはHierarchyから選択する。

## Particle Editor

「表示 / View → Particle Editor」で補助画面を開く。Particle選択中のInspectorやProjectからも開ける。

1. 「プレビュー開始」でScene内の発生位置へカメラが移動し、連続発生を始める。
2. 「Particleだけを表示」でモデルを隠して確認できる。
3. 発生位置・拡散範囲・一度の発生数・間隔・寿命を調整する。
4. 最小／最大速度・加速度・粒子サイズ・色・画像・合成方法を調整する。
5. 「プリセット書出し」で `resources/particles/Default.ponparticle` へ書き込む。次回起動時にも読み込む。

「一度だけ発生」はBurstとして一回だけ生成し、その後は新しく発生させず寿命まで動かす。一時停止中は「1フレーム進める」を使える。「リセット」で現在の粒子と発生タイマーを初期化する。

設定変更は主に次に生まれる粒子へ反映する。すぐに新しい設定だけで確認したいときはリセットする。実際の管理数も100粒までに制限するため、高頻度・長寿命でも粒子リストは増え続けない。

ゲームの再生中はParticle設定の編集を無効にする。Particleのプレビュー操作は、ゲームの再生・一時停止とは独立している。

## 保存の区別

- 「シーン / Scene → 保存」またはCtrl+S：配置・名前・見た目・Component状態・Emitter設定。
- Particle Editorのプリセット書出し：発生条件・粒子の見た目などのプリセット。
- 終了時に未保存のParticle設定がある場合も保存確認を表示する。
- Particle設定の読込・既定値への変更では、現在の変更を破棄する前に確認する。
- シーン内Emitterは個別にUndo/Redoできる。共通プリセットのUndo/Redoと任意の画像のインポートは未対応。

## ProjectとConsole

Projectのモデルボタンは、Hierarchyで選択中のオブジェクトへ読み込み済みのモデルを適用する。Consoleにはシェーダー・保存・読込などの実ログを表示する。アセットのインポートやログの全履歴検索は今後の機能。

## 開発者向け検証

```powershell
MSBuild tests/SceneFileTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
./generated/scene-file-tests/bin/SceneFileTests.exe
./tests/RunEditorGraphicsTests.ps1
./tests/RunEditorStartupTests.ps1 -Configuration Debug
```

GraphicsTestsは本番と同じソースを試験用の起動コードでビルドし、実GPUで90フレーム描画する。Scene描画先の連続リサイズ、記述子ハンドルの維持、D3D12検証エラー、パネルのドッキング、日本語字形、Particleの上限・Pause・Burst・Resetを確認する。設定ファイルはgenerated配下のコピーのみを使い、制作中のシーンやプリセットは保存しない。

StartupTestsはソリューション全体を再ビルドし、本番のWinMainからGameSceneを生成して90フレーム動作させる。DevelopmentとReleaseも指定できる。クラスの定義を変えた後、古い中間ファイルや壊れたヘッダー依存記録が残っていると、新旧のクラス配置が混在して起動時にアクセス違反になる場合がある。その場合はデバッグを停止して再ビルドする。コンパイラがC1902などで失敗した後も、依存記録を再生成するため全体を再ビルドしてから確認する。

## オブジェクトの追加

Hierarchyの空白部分を右クリックし、「新規作成」から空オブジェクト、点光源、Particle、Plane、Axisを追加できる。
空オブジェクトにはInspectorからMesh Rendererを付けられる。点光源は位置、色、強度、範囲、減衰を編集できる（最大8個）。
Particleは選択してParticle Editorで編集する（最大32個、各100粒子）。発生位置はTransformと共通で、Emitterの回転・拡縮は未対応。設定はシーン保存・Undo/Redo・停止時の復元に含まれる。
Particle Editorの「プリセット書出し」は選択したEmitterを共通プリセットへ書き出す操作で、シーン保存とは別。
Scene内ではメッシュを持たないオブジェクトにも位置マーカーを表示する。

シーン管理は上部のSceneメニュー、再生操作はToolbar、結果通知は下部へ分離している。
Toolbarの三角は再生、四角は停止、二本線は一時停止、縦棒付き三角は1フレーム更新。
Undo/RedoはCtrl+Z、Ctrl+YまたはCtrl+Shift+Z。新規はCtrl+N、読込はCtrl+O、再生/停止はCtrl+P。
保存形式5は種類別Componentと親IDを含み、従来の形式1〜4も引き続き読み込める。


## Hierarchyの親子関係

オブジェクトを別のオブジェクトへドラッグ＆ドロップすると、その子にできる。右クリックメニューの「親を外す」、またはHierarchyの空白部分へドロップすると親を解除する。親子付け・解除は見た目の位置・回転・拡縮を維持する。

子のInspectorに表示するTransformは親に対するローカル値。SceneViewの移動ギズモとF注視はワールド座標を使う。親を動かすと子のモデル、点光源、Particleの発生位置も追従する。Particleは発生済みの粒子をワールド空間に残す。

親を無効にすると子孫も非表示になる。親の複製・削除は子孫もまとめて対象とし、Ctrl+Zで一回の操作として戻せる。保存・読込・再生停止時の復元にも親子関係を含める。

階層は64段まで。自分自身・子孫への親子付けを拒否する。親の拡縮が0に近い場合や、回転と非均等拡縮の組合せでせん断が必要になる場合は、見た目を保った変更ができないため操作を拒否し、Hierarchyに理由を表示する。


Hierarchyの操作は右クリックメニューにまとめている。オブジェクト上で右クリックすると、その対象を選択して新規作成・複製・削除・親解除・選択解除を表示する。空白部分の右クリックでは新規作成を表示する。新規オブジェクトは最上位へ追加する。空白部分の左クリックでも選択を解除でき、Ctrl+D・Deleteなどのショートカットも使える。
