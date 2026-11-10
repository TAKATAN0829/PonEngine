[![DebugBuild](https://github.com/TAKATAN0829/PonEngine/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/TAKATAN0829/PonEngine/actions/workflows/DebugBuild.yml)
[![DevelopmentBuild](https://github.com/TAKATAN0829/PonEngine/actions/workflows/DevelopmentBuild.yml/badge.svg)](https://github.com/TAKATAN0829/PonEngine/actions/workflows/DevelopmentBuild.yml)
[![ReleaseBuild](https://github.com/TAKATAN0829/PonEngine/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/TAKATAN0829/PonEngine/actions/workflows/ReleaseBuild.yml)

# PonEngine

Windows向けのC++20 / DirectX 12製ゲームエンジンと、Dear ImGuiを使った制作用Editorです。Unityを参考に、Hierarchyでオブジェクトを管理し、SceneViewで配置、InspectorでComponentを編集する流れを実装しています。

現在はモデル・マテリアル・カメラ・Particle・簡易物理を使ったシーン制作と、再生／停止による確認ができます。機能によって対応範囲が異なるため、[現在の制限](#現在の制限)も確認してください。

- [Editor操作の詳細](docs/editor-guide.md)：各機能の操作、保存、制限
- [設計とファイル構成](docs/editor-architecture.md)：依存関係、状態の所有者、実装ファイルの担当

## ビルドと起動

### 開発環境

- WindowsとDirectX 12対応のGPU・ドライバー
- Visual StudioのC++開発環境とMSBuild
- プロジェクトが指定するMSVCツールセット **v145**、Windows SDK
- ビルド対象は **x64**、言語標準は **C++20**

CIはVisual Studio 2026のWindows環境を使っています。Dear ImGui、DirectXTex、Assimpなどの依存ファイルは`project/externals`を参照します。Assimpは構成ごとのライブラリも必要です。

### Visual Studioから起動する

1. `project/PonEngine.slnx`を開く。
2. `CG2`をスタートアッププロジェクトにする。
3. プラットフォームを`x64`、構成を`Debug`または`Development`にする。
4. `CG2`のデバッグ設定で、作業ディレクトリを`$(ProjectDir)`（`project/CG2`）にする。
5. ビルドして起動する。

| 構成 | 用途 | Editor |
|---|---|---|
| Debug | デバッグ・GPU検証 | あり |
| Development | 開発中の動作確認 | あり |
| Release | ゲーム実行 | なし |

Releaseも保存済みのシーンを読み込み、有効なメインカメラを使って描画します。実行ファイルの出力先は`generated/outputs/<構成>/CG2.exe`です。

### コマンドからビルド・起動する

Visual StudioのDeveloper PowerShellで、リポジトリのルートから実行します。

```powershell
MSBuild project/PonEngine.slnx /p:Configuration=Development /p:Platform=x64 /m:1 /nr:false
Push-Location project/CG2
& ../../generated/outputs/Development/CG2.exe
Pop-Location
```

`resources/...`は作業ディレクトリからの相対参照です。実行ファイルの場所だけへ移動して起動すると、シェーダーや画像を見つけられません。ゲームを別の場所へ配置する場合も、実行時の作業ディレクトリ直下に`resources`を置きます。

## 最初のシーンを作る

1. Hierarchyの空白を右クリックし、「新規作成」からEmpty、Axis、Light、Particle、Cameraを追加する。Cube・Sphere・Planeは「3D Object / 基本形状」から選ぶ。
2. 独自のモデルを使う場合は`project/CG2/resources`以下へ関連ファイルを置き、Projectの右クリックから「更新」を選ぶ。
3. ProjectのモデルをSceneViewへドラッグして配置する。
4. HierarchyまたはSceneViewで選択し、Inspectorで名前、Active、Transform、見た目を編集する。
5. 必要に応じてInspectorの空白を右クリックし、Componentを追加する。
6. 上部の再生ボタンで確認し、停止して編集時の配置へ戻す。
7. `Ctrl+S`でシーンを保存する。

たとえば落下を確認する場合は、床にBox Collider、落とす対象にColliderとDynamic Rigidbodyを追加します。再生中に落下・接地を確認し、停止すると再生前のTransformと設定へ戻ります。

## Editorの画面と基本操作

```text
┌──────────────────────────────────────────────────┐
│ Scene / Viewメニュー          再生・停止・一時停止 │
├───────────┬────────────────────────┬─────────────┤
│ Hierarchy │ SceneView / GameView   │ Inspector   │
│           │                        │             │
├───────────┴────────────────────────┴─────────────┤
│ Project / Console                                │
├──────────────────────────────────────────────────┤
│ 操作結果                         FPS / ms / GPU   │
└──────────────────────────────────────────────────┘
```

| パネル | 役割 |
|---|---|
| Hierarchy | オブジェクトの選択、作成、親子関係、複製、削除、Prefab操作 |
| Inspector | 選択対象の名前・Active・Transform・Component編集 |
| SceneView | 編集用カメラでの確認、選択、モデル配置、ギズモ操作 |
| GameView | メインカメラとゲーム画面の確認 |
| Project | フォルダツリー、Assetプレビュー、検索、配置・移動、名前変更、使用箇所確認、ゴミ箱 |
| Console | 実ログの検索、先頭行表示、全文確認、コピー |

通常表示はこの6タブです。Particle Editor、World Settings、Camera、Performanceは上部の「表示 / View」から開きます。タブをドラッグして配置を変更でき、「初期レイアウトに戻す」で標準配置へ戻せます。配置は`editor-layout.ini`へ保存します。

フォントはWindowsのメイリオ18pxを使用し、倍率は**0.85固定**です。

### SceneViewのカメラとギズモ

- 右ドラッグ：視点回転
- 中ボタンドラッグ：平行移動
- マウスホイール：前後移動
- SceneView内の「View / 表示」→「選択対象を注視」：選択対象へ視点を合わせる
- カメラのキーボード移動は使わない

ツール選択のMove / Rotate / Scaleで移動・回転・拡縮を切り替えます。移動・回転はワールド軸、拡縮はローカル軸を使います。Ctrlを押してドラッグするとスナップし、Escで操作開始時の値へ戻します。

SceneView内の「View / 表示」では、グリッド、常時スナップ、移動距離・回転角度・拡縮倍率の刻みを設定できます。初期の刻みは距離1、回転15度、倍率0.1です。

Cameraを選択すると撮影範囲、Point Lightを選択すると影響半径、Colliderを選択すると判定形状を補助表示します。これらはGameViewには表示しません。

### HierarchyとInspector

右クリック→「新規作成」→「3D Object / 基本形状」からCube・Sphere・Planeを原点へ追加できます。Cubeは一辺1でBox Collider付き、Sphereは直径1でSphere Collider付きです。Rigidbodyは必要に応じてInspectorから追加します。Transform・Materialの編集、複製、親子付け、Undo/Redo、Scene保存、Prefabにも対応します。メッシュは`resources/primitives`の通常のOBJとして扱い、Projectでもプレビューできます。

HierarchyのCtrlクリックで複数選択できます。オブジェクトを別のオブジェクトへドロップすると親子付け、空白へドロップすると親を解除します。親子変更はワールドでの見た目を維持し、表現できないせん断や循環は拒否します。

InspectorのTransformは親に対するローカル値で、回転は度数で表示します。TransformのリセットやComponentの削除は見出しを右クリック、追加はInspectorの空白を右クリックします。親の複製・削除では子孫もまとめて扱います。

### ショートカット

| キー | 操作 |
|---|---|
| Ctrl+N | 新規シーン |
| Ctrl+O | 既定の保存シーンを読み込む |
| Ctrl+Shift+S | 名前を付けて保存 |
| Ctrl+S | シーン保存 |
| Ctrl+P | ゲーム再生／停止 |
| Ctrl+Z | Undo |
| Ctrl+Y / Ctrl+Shift+Z | Redo |
| Ctrl+D | 選択対象を複製 |
| Delete | 選択対象を削除 |
| Ctrl＋ギズモのドラッグ | スナップ |
| Esc（ギズモ操作中） | 操作前へ戻す |

再生・停止・一時停止・1フレーム更新は上部のアイコンから操作できます。ゲームの再生操作とParticleの編集プレビューは別管理です。

## 制作機能

### モデル・マテリアル・アニメーション

- ProjectはOBJ / glTF / GLB / FBXのモデル配置に対応する。
- フォルダはダブルクリックで開き、検索は表示中フォルダの名前を対象にする。
- モデルは形状のサムネイル、PNG / JPEG / BMPは画像プレビューを表示する。
- モデルをSceneViewへドロップすると、モデル表面、XZ平面、視線の前方の順で配置先を求める。
- InspectorのMaterialsでスロットごとの色・光沢・ライティングを編集し、Texture欄へProjectの画像をドロップできる。
- スケルタルアニメーションはクリップ選択、速度、ループ、プレビュー、時間指定に対応する。頂点変形はCPUで行う。

読み込めないアセットはエラーを表示します。各オブジェクトのマテリアル設定やアニメーション時計は、同じモデルを参照する別のオブジェクトとは共有しません。

### カメラとライト

HierarchyからCameraを追加し、InspectorでMain Camera、FOV、Near / Far Clipを編集します。最初のCameraはメインに指定されます。GameViewとReleaseは有効なメインカメラを使い、未指定・無効時は既定の視点へ戻ります。

Hierarchyの右クリック→「新規作成」→「Light / ライト」から、Point / Spot / Directionalを追加できます。

| 種類 | 上限 | Inspectorで編集する項目 |
|---|---|---|
| Point | 9個 | 位置、色、強度、範囲、減衰 |
| Spot | 8個 | 位置、回転、色、強度、範囲、減衰、外角・内角 |
| Directional | 4個 | 回転、色、強度 |

Spot / Directionalの方向はTransformのローカル+Zで指定し、親の変換にも追従します。Spotの角度は円錐全体の開き角（度数）です。SceneViewではSpotの円錐とDirectionalの方向矢印を表示します。Activeで消灯でき、保存・Prefab・複製・Undo/Redoにも対応します。

World Settingsの固定ライトは廃止しました。旧Scene 1〜12をEditor・ゲームで読み込む際は、以前の固定Directional / Point / Spotを各1個、名前に`(Migrated)`が付いたオブジェクトとして追加します。旧固定値と同じ色・位置・方向・強度を設定し、保存するとScene 13として確定します。旧実行時に変更して保存されなかった固定ライトの値は復元できません。Pointの上限9個は、旧シーンの配置8個と固定1個を保持するためです。

ライトなしではライティング有効なモデルは黒くなります。Lightingを無効にしたモデルはライトによらず表示します。影の生成には未対応です。

### ColliderとRigidbody

Box / Sphere Colliderと、Static / Dynamic / Kinematic Rigidbodyに対応します。重力、初速、質量、摩擦、反発、速度減衰を設定でき、Triggerは押し戻さず接触通知だけを行います。

Colliderの補助線は通常が緑、接触中が赤、無効時が灰色です。再生中の接触開始・終了はConsoleへ通知します。Pauseでは状態を保ち、Stepで一更新、Stopで編集前へ戻します。

現在の物理は並進運動の基盤です。Dynamicを別のDynamicやKinematicの子にする構成などは拒否します。複合形状は親に一つのRigidbody、子に複数のColliderを付けます。

### Particle Editor

1. HierarchyでParticleを選び、「表示 / View」→「Particle Editor」を開く。
2. 「プリセット」から**炎 / Fire、煙 / Smoke、火花 / Sparks**を選ぶ。
3. 「プレビュー開始」または「一度だけ発生」で確認する。
4. 発生範囲、個数、間隔、速度、加速度、寿命、サイズ、色、合成を調整する。
5. Projectの画像を「画像」欄へドラッグすると、任意の画像をParticleへ適用できる。
6. 個別Emitterの設定はシーン保存、共通プリセットへの書出しは「プリセット書出し」を使う。

Lifetimeでは開始／終了サイズ倍率、終了色、フェードインを設定できます。Soft Particlesはモデルとの交差部分を深度差で薄くし、板状の境界を目立ちにくくします。「Particleだけを表示」ではモデルの深度がないため、この交差フェードは発生しません。

透明度付き画像はそのAlphaを使い、全画素が不透明な画像は明るさをマスクにします。発光はAdd、煙はNormalが基本です。Noneは半透明合成を行いません。

シーン内Emitterは最大32個、各100粒子です。Pause / Burst / Reset / 1フレーム更新に対応します。設定に応じて新しく発生する粒子だけへ反映する項目もあるため、変更後にリセットすると確認しやすくなります。

### Prefab

Hierarchyの右クリックから対象と子孫を`.ponprefab`へ保存し、ProjectからSceneViewやHierarchyへ配置できます。ルートは元Assetへのリンクを保持し、Applyで元Assetへ反映、RevertでAssetから復元します。

RevertはUndoできます。ApplyによるAssetファイルの上書きはUndo対象外です。操作の詳細は[Prefabの説明](docs/editor-guide.md#prefab)を参照してください。

### PerformanceとConsole

「表示 / View」→「Performance / パフォーマンス」で、FPS、フレーム時間、GPU時間、DrawCall、三角形数、インスタンス数、ImGuiのDrawCallを確認できます。CPUはEditor、Asset読込、アニメーション、Particle、物理、描画、Presentや待機などの区間に分けて記録します。

直近600フレームの履歴、一時停止、重かったフレームの保持、CSV出力に対応します。CSVは作業ディレクトリの`performance-capture.csv`へ出力します。CPU区間は入れ子なので全行を足し合わせず、GPU時間もCPU時間とは合算しません。

Consoleでは最近のログを検索し、ホバーで全文を表示、右クリックで全文をコピーできます。ログの永続的な全履歴検索には未対応です。

## 保存先と互換性

以下は`project/CG2`を作業ディレクトリにした場合の相対パスです。

| 対象 | 保存先 | 主な内容 |
|---|---|---|
| シーン | resources内で選んだ`.ponscene`（起動時の既定は`resources/scenes/GameScene.ponscene`） | オブジェクト、親子関係、Transform、Component、個別Emitter |
| 共通Particleエフェクト | `resources/particles/Default.ponparticle` | Groupと複数Emitter、発生・移動・見た目・寿命カーブ・発生形状・衝突・残像 |
| Prefab | `resources/prefabs/*.ponprefab` | 対象と子孫の設定 |
| Editor配置 | `editor-layout.ini` | ドッキング配置 |

現在の書出し形式は**Scene 14 / Particle Effect 2**です。Scene 1〜13、Particle 1/2、Particle Effect 1も読み込み、新しいParticle項目は既定値で補完します。Scene内の各Particle ComponentはEmitterノード階層を保存し、旧単一Particle設定からも自動移行します。旧シーンの固定ライトは上記の移行を行います。再生中の速度・粒子・時計・接触履歴などは保存しません。

新規・読込・終了などで未保存の変更があれば確認を表示します。シーン保存とParticleプリセット書出しは別の操作です。Sceneメニューの「開く...」（Ctrl+O）と「名前を付けて保存...」（Ctrl+Shift+S）で、resources内の複数シーンを切り替えられます。ProjectのScene Assetはダブルクリック、または右クリック→「Sceneを開く」で開きます。

## コード構成と開発方針

```text
project/
  PonEngine.slnx
  CG2/
    application/       GameScene・TitleSceneの初期配置、HUD、場面遷移
    editor/            Editorの実装
      core/            Editor本体・各機能の接続
      state/           選択・編集履歴・再生前の状態
      ui/              レイアウト・配色・共通の入力欄
      assets/          Asset操作・パネル間のドラッグ情報
      scene/           シーンの保存先・ファイル選択
      panels/          タブごとに実装を分類
        hierarchy/     Hierarchyのツリーと右クリック操作
        inspector/     InspectorのComponent編集・複数選択
        project/       Projectのフォルダ・Asset表示
        console/       Consoleの検索とログ表示
        scene_view/    SceneViewの操作・ギズモ・共通のタブ管理
        game_view/     GameViewのカメラ画像表示
        particle_editor/ Particle Editorの設定・プレビュー操作
        performance/   Performanceの履歴表示・CSV出力
    engine/
      core/            Engineの起動・実行ループ・終了の組み立て
      types/           共有する構造体・ID・種類・上限定数
      platform/        Windowsのウィンドウ・メッセージ処理
      diagnostics/     ログ・CPU/GPU負荷の記録と出力
      ui/              ImGuiの初期化・Win32/DirectX連携
      dx12/            DirectX 12・描画先・記述子・Pipeline
      renderer/        モデル・Particleなどの描画設定
      object/          描画オブジェクト・実行中のParticle
      world/           ゲームワールドの共通基盤・シーン切り替え管理
        runtime/       WorldSceneによる共通の更新・描画・Editor窓口
        objects/       オブジェクト一覧・親子関係・基本形状の作成
        settings/      Material・Animation・Particle・物理・ライトなどの設定値
        serialization/ Scene・Particleエフェクト／プリセットの保存と読込
        prefab/        Prefab階層の保存・配置・Apply／Revertの手順
        systems/       動作・衝突判定・物理更新・ライトの収集
      asset/           モデル・テクスチャ・アニメーションの読込
      math/            行列・投影・選択用の計算
    resources/         モデル・画像・シェーダー・保存データ
  externals/           外部ライブラリ
tests/                 CPU・UI操作・実GPU・起動の検証
docs/                  操作と設計の詳細
generated/             ビルド結果・テスト生成物（Git管理外）
```

オブジェクトの保存用データは`world/objects/WorldObjectData.h`にまとめ、実行・保存処理は`IWorldObjects`を窓口にします。Editorは、その操作を継承してカメラ・プレビュー操作を加えた`IEditableWorld`を使います。保存用データと実行中の状態を分け、UIはシーン所有の参照をフレームをまたいで保持しない方針です。

Prefab操作は`world/prefab/PrefabOperations.h/.cpp`にまとめています。Editorが保存・配置・Apply／Revertを依頼し、PrefabOperationsが`SceneFile`へ読書きを委譲します。`WorldObjects`はデータ一覧と親子関係・Transformの操作を担当し、Prefabや保存処理を参照しません。配置は一時データで検証し、最後に配置先の`ReplaceObjects`へ渡すため、実行側のモデル準備や失敗時の復元も維持します。詳しい呼出順は[依存関係](docs/dependencies.md)に記載しています。

命名ではSceneをGameScene・TitleSceneなどの場面とその管理に使い、共通の実行処理にはWorld、設定値にはRigidBodySettings・ColliderSettingsなどの機能名を使います。SceneFileは場面の保存、SceneViewはEditorの画面名として残しています。

現在の参照方向、所有関係、相互依存と整理の優先順位は[依存関係の調査](docs/dependencies.md)にまとめています。

各パネルはタブごとのフォルダへ置き、各クラスの.h/.cpp一組へ実装をまとめています。ProjectPanelは表示・フォルダ管理・プレビューを担当し、確認画面はeditor/assets/AssetActionDialog、参照文書はAssetDocument、ファイル操作はAssetFileOperationsへ分けます。SceneViewのギズモ・選択・配置などは、同じcpp内で分類コメントにより区切ります。担当ファイルはヘッダーの分類コメントと[設計資料](docs/editor-architecture.md#editorパネルの責務と実装ファイル)から確認できます。

追加・修正するコードは以下を基準にします。

ファイルは原則としてクラスごとの同名.h/.cpp一組に揃えます。型・定数・宣言だけの.hは単独でも構いません。起動入口のmain.cppは例外です。

.hを目次として読めるよう、.cppの見出し名と関数の実装順は宣言側の分類・順番に揃えます。見出しには「メンバ関数」「メンバ変数」を付けず、Motion・Colliderなど内容を示す名前を使います。cpp内だけのクラス外補助関数と、状態の定義は別枠にまとめます。

- 三項演算子、ラムダ式、`auto`、一文字の変数名・関数名を使わない。
- `.h`には型・定数・宣言・クラスのメンバーの初期値を置く。Getter／Setterやインターフェースの既定処理を含む関数の処理は、同名の`.cpp`へ書く。
- 構造体のデータメンバーは宣言だけを書き、インスタンスの生成時に初期値を指定する。型として初期化処理を持たせる場合は、コンストラクターを同名の`.cpp`へ書く。
- 共有するstatic変数は、`.h`のinline変数として定義・初期化する。入れ子の型は、型定義が揃った後でinline定義してよい。
- 値を取得するGetterは`Get○○`、設定するSetterは`Set○○`とする。boolのフラグ・状態確認は疑問文の形に揃え、`Get()`だけのような取得対象が不明な名前は使わない。
- bool型のメンバー・ローカル変数・引数は、`isPlaying_`・`hasChanged`・`canContinue`・`shouldSimulate`・`wasSaved`のように真偽の意味を読める名前にする。状態確認の関数も`Is…`・`Has…`・`Can…`・`Should…`・`Does…`・`Are…`などの形にする。`Save()`・`Initialize()`など、処理を実行して成否を返す関数は動詞の処理名を維持する。
- 条件式の比較結果をそのままreturnせず、`if`と`return true`／`return false`で明示する。
- if文の本体は改行し、種類の異なる判定を一つの条件へ詰め込まない。
- 意味の通る名前と必要なローカル変数を使い、意図を日本語コメントで残す。
- 関数内にも、判定・計算・状態更新などの目的を短い日本語コメントで書く。処理のまとまりごとに付け、必要なら一つずつ説明する。コメントは対象の処理の直前に置く。
- 同じ型・用途の変数宣言が続く場合を除き、処理・関数宣言・役割の違う変数の間は空行を1行入れる。説明コメントは対象の宣言・処理に付け、複数行の式・引数・初期化リストの途中には空行を入れない。includeのまとまりは維持する。
- ヘッダーは初期化・更新・描画、Getter / Setterと、処理・状態の役割ごとに分類する。
- ファイルは責務で分け、重複する処理をまとめる。性能改善は計測しながら進める。

既存コード全体の責務整理は継続中です。機能の動作確認と、構成・可読性の改善を分けて確認します。

## Projectのファイル操作

Assetとフォルダの右クリックから「名前を変更」「移動」「使用箇所を確認」「ゴミ箱へ移動」を選べます。フォルダツリー・Asset一覧のフォルダへドラッグして移動先を選ぶこともできます。移動先は確認画面に表示し、「実行」で確定します。背景やフォルダの右クリックから新しいフォルダも作成できます。

「移動」の入力欄は`resources`からの相対フォルダです。例えば`models/characters`と入力すると、そのフォルダの中へ選択中のAssetを移します。ルートへ戻す場合は`.`を指定します。「名前を変更」は拡張子を含む名前だけを入力し、元の拡張子は維持します。同名Assetへの上書きや、フォルダ自身の子への移動は拒否します。再生・Pause中は閲覧と参照確認だけを許可します。

Editor経由の移動では、編集中の配置、保存済みScene・Prefab・Particleプリセット、Undo/Redo・保存基準の参照を更新します。OBJ→MTL、MTL→画像、glTF/GLB→画像・外部バッファの相対参照も移動後の位置へ合わせます。GLBのバイナリチャンクは変更しません。これはパス参照の一括更新であり、GUIDによるAsset管理や、Explorerでの移動の自動追跡ではありません。

「使用箇所を確認」では、ファイル名・オブジェクト名・Materialスロット・Undo/Redoなどの参照元を表示します。フォルダの場合は中のAssetへの外部参照をまとめます。現在の配置から消しても、保存ファイルやUndo履歴が使っているAssetは削除できません。フォルダの内部だけで完結する参照は、そのフォルダ全体をゴミ箱へ移す妨げにはしません。

未使用Assetの削除は、`resources/.editor-trash/<操作番号>/`への移動です。Consoleに復元用の実パスを残し、同じ場所の`recovery.txt`に元のパスを記録します。必要なら元の場所へ戻してProjectを更新してください。ゴミ箱はProjectの一覧・参照走査・Gitの対象から除外します。ファイル操作はCtrl+Zの対象外で、オブジェクトのUndo/Redoは移動後の参照を保ったまま利用できます。

移動では、全参照の調査と更新ファイルの準備を終えてから元のAssetを動かします。通常の書込失敗時は、更新済みファイルとAssetを元の状態へ復元します。変更前の保存ファイルは同じ非表示フォルダに退避します。プロセスの強制終了や復元自体が失敗した場合は、Consoleのパスと`recovery.txt`を確認して復旧できます。

安全に調査できない操作は理由を表示して止めます。FBXの再配置や、依存を調べられないFBXが存在する場合の画像・フォルダ操作、未対応のMTLオプション、壊れた参照文書、リンク・Junctionを含む構成などが対象です。組込モデル・標準画像・シェーダー・既定保存先はコードから直接参照するため、移動・削除を禁止しています。独自のC++コードや未対応形式に書かれたパスの自動更新は行いません。

走査は操作要求時と確定直前だけで、通常の毎フレーム描画には追加しません。上限は10000ファイル、参照文書1件64 MiB・合計128 MiB、移動後の各パス240 UTF-8バイトです。通常のScene保存形式は14、Particleエフェクトは2です。従来のParticleプリセット形式2も読み込みます。参照を更新した旧Sceneは、従来の固定ライト相当を引き継いで形式14へ保存します。

## GameSceneとTitleScene

通常起動はこれまでどおりGameSceneです。起動引数に`--title`を指定すると、回転する青いCubeと専用カメラを持つ最小のTitleSceneから開始します。Visual Studioではデバッグの「コマンド引数」に指定できます。

- Debug / Developmentでは再生し、GameViewを選んで操作します。文字入力中・メニュー表示中は場面を切り替えません。
- TitleSceneでEnterを押すとGameSceneへ進み、GameSceneでEscapeを押すとTitleSceneへ戻ります。Releaseでは通常のゲーム更新中に受け付けます。
- Pause中は遷移しません。Stepはゲーム更新を一回行うため、その回で入力が成立すれば遷移要求を受け付けます。
- Editorで停止すると、最初に再生したシーンの同じインスタンスへ戻り、再生前のオブジェクトを復元します。遷移中の一時シーンは保存・Undo履歴の対象にしません。
- Editorの編集中ファイルと、C++のGameScene／TitleSceneの実行時遷移は別に管理します。ゲーム起動時は引き続き`GameScene.ponscene`を読み込みます。Build Settingsや任意のScene Assetへの実行時遷移は今後の範囲です。

`engine/world/runtime/WorldScene`は実際の場面を作るための共通基底です。カメラ・WorldMotion/WorldPhysics/WorldCollisionの実行、モデル・Particle描画、Asset準備・モデルプレビューを担当し、`application/GameScene`と`application/TitleScene`が各場面の初期配置や遷移先を決めます。サンプルSpriteとその設定UI、既定シーン・Particleプリセットの読込はGameSceneに残しています。

`IScene::RequestSceneChange`は未初期化の次シーンを所有権付きで要求します。SceneManagerが更新中に要求を取り出し、EngineがPostDrawのGPU完了後に切り替えます。シーン遷移専用のGPU待機や、毎フレームの配置データ複製は追加していません。

## 検証方法

Developer PowerShellで、リポジトリのルートから実行します。

```powershell
# CPU：保存・互換性・履歴・親子関係・配置計算・物理・Particle設定など
MSBuild tests/SceneFileTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
./generated/scene-file-tests/bin/SceneFileTests.exe

# UI操作と実GPU：ギズモ、リサイズ、描画、Particle合成・寿命変化・Soft Particlesなど
./tests/RunEditorGraphicsTests.ps1
```

GraphicsTestsの実行時リソースは`generated`以下へコピーして使い、制作中のシーンやプリセットを上書きしません。DirectX 12を使うため、実行可能なGPU環境が必要です。

## 現在の制限

- ライトの影生成・環境光・Image Based Lightingには未対応。
- 物理は並進運動が中心。角速度・トルク・回転への衝撃・ジョイント・連続衝突判定には未対応。
- アニメーションはCPUスキニング。遷移グラフ、クリップ間ブレンド、ルートモーション、GPUスキニングには未対応。
- Particle同士のSoft Particles、共通プリセットのUndo/Redo、イベント発火型の子Emitterには未対応。
- 入れ子Prefab、任意パスのシーン切替、自動Asset監視・再インポートには未対応。
- モデルの埋込みテクスチャは白画像へフォールバックする。

操作に失敗した場合は、まず画面下部の操作結果とConsoleを確認してください。リソースが見つからなければ作業ディレクトリを確認します。クラス定義を変えた後に起動時のアクセス違反やC1902などのビルド不整合が出た場合は、実行中のデバッグを停止してソリューション全体を再ビルドします。
