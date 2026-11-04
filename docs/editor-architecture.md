# Editorの最初の実装

Debug / Development で起動し、Hierarchy のモデルを選択すると、Inspector でモデル種類・位置・回転・拡縮・色・ライティング・光沢・RootNode行列の適用を編集できる。Clear Selection で選択を解除する。パネルは既存のDockSpaceにドッキングできる。

Hierarchyの右クリック→新規作成から追加し、複製・削除も右クリックメニューで行う。Cube・Sphere・Planeは3D Objectのサブメニューにまとめる。追加・複製後は新しい対象を選択する。複製は同じ位置に作られるため、Inspectorで位置を変更すると分離できる。最後のモデルを削除して空のシーンにすることもできる。

WorldPrimitivesはCube・Sphereの作成時だけ使う関数群で、通常のWorldObjectType::ModelにOBJパスと初期Colliderを設定する。resources/primitivesのメッシュは既存のModelManagerで読み込み、専用の保存形式や描画経路を増やさない。作成時にだけシーンの値を複製し、ReplaceObjectsでモデル読込・ID・Collider上限を検証してから置換する。失敗時はシーンを変更しない。毎フレームの処理にはスナップショット作成を追加しない。

## 依存と責務

```text
Engine → Editor → HierarchyPanel / InspectorPanel → ImGui
            ↓
       IEditableWorld ← WorldObjects ← WorldScene ← GameScene / TitleScene
Engine → SceneManager → IScene
```

- ImGuiSystem: ImGuiの初期化、フレーム開始、DX12への描画、終了。
- Editor: 選択状態とパネル呼び出しの調整。Engineが値として所有する。
- HierarchyPanel: 編集対象一覧の表示と選択。
- InspectorPanel: 渡されたデータの表示と編集。
- IEditableWorld: ImGuiを含まない編集対象の列挙・操作契約。
- WorldObjects: 設定データの一覧、ID採番、追加・削除・複製、全件置換と検証。描画機能には依存しない。
- WorldScene: WorldObjectsの設定を使い、IDごとのObject3d・Particle・プレビューと物理の実行状態を所有する。
- GameScene: 既定配置・保存ファイル・共通Particleプリセットの読込とサンプルHUDを定義する。
- TitleScene: ゲーム配置を読み込まず、専用のCubeとカメラを作り、Enterでゲームへ進む。

GameSceneはEditorの具体クラスを知らない。EditorもGameSceneを知らず、IEditableWorldを受け取る。この引数渡しが依存性の注入に当たる。最初は複雑なイベント機構や汎用リフレクションを導入せず、小さなインターフェースで境界を設けている。

## 所有権と更新順

EditableObjectView内のポインタは借用であり、Editor側から破棄しない。追加・削除・読込で無効化されるため、操作後に取得し直し、保持しない。選択にはオブジェクトIDを保持し、毎フレーム検索して対象が存在しなければ解除する。IDはシーン内で一意で、削除や読込後も同一実行中の採番は巻き戻さない。

ImGuiフレーム開始 → Editorで値を編集 → WorldSceneと派生シーンの更新 → シーン描画 → ImGui描画、の順に処理し、モデルへの反映が翌フレームへ遅れないようにしている。

設定データとGPUリソースの寿命を分け、WorldSceneの更新時に追加・削除を描画用一覧へ反映する。現在のDirectXCommon::PostDrawは毎フレームGPUの完了を待つため、前フレームの参照が終わってからGPUリソースを解放できる。将来フレーム並列化する場合はFenceに基づく遅延解放へ変更する。

メッシュはModelManagerのModelを共有し、色・ライティング・光沢・テクスチャ指定を持つMaterialはObject3dごとに所有する。同じモデルを複製しても、一方の色編集が他方へ波及しない。

## 現在の範囲と次の課題

既定ではPlaneとAxisの二体を作成する。resources内のOBJ・glTF・GLB・FBXをモデルとして読み込み、最大10,000体まで扱う。ライト、パーティクル、カメラの既存デバッグUIはそのまま使える。移動・回転・拡縮ギズモ、Projectからのモデル配置、3Dサムネイルに対応する。複数マテリアル・Prefab・ボーンアニメーションに対応する。ユーザースクリプトは未対応。埋込みScene/Gameビュー、ドッキング、Particle Editorは実装済み。

シーン切替はまだ存在しないため、現在はシーンインターフェースのアドレス変更で選択を解除する。シーンの再生成・切替を導入する際は、同じアドレスが再利用されても識別できるシーン世代番号や明示的な切替通知を追加する。

Undo/Redoは現在、設定データ一覧のスナップショットを最大64状態保持する。ゲームオブジェクトや描画リソースを履歴へ格納しない。将来、大規模なシーンで履歴のメモリを抑える場合は「対象ID・変更前・変更後」を持つCommandへ移行する。

## シーンの保存・読込

SceneDocumentが編集中のresources内`.ponscene`と保存先を所有する。SceneAssetDialogはファイル選択・上書き確認だけを担当し、Editorが未保存確認と成功後の状態切替を行う。Ctrl+Sは現在の保存先、Ctrl+Shift+Sは別名保存、Ctrl+Oはファイル選択。Projectからのダブルクリックも同じ切替処理へ送る。作業ディレクトリは`project/CG2`とする。

保存済みファイルがあればGameSceneの初期化時に自動読込する。Releaseでも読込できる。ファイルがない場合は既定値で開始し、壊れたファイルは値を反映せずログにエラーを出す。保存はメニューまたはショートカットから明示的に行う。

SceneFileはEditorに依存せず、IEditableWorldを引数に受け取る。コレクション対応シーンはテキスト形式 `PON_SCENE 6` で、個数、ID、Transform・色・光沢・ライティング、名前、モデル種類、RootNode行列の適用、Active、Mesh Rendererの有無、オブジェクトの種類、光源値、Emitter設定を記録する。浮動小数点は往復可能な桁数を使う。

読込ではバージョン、件数、IDの重複、モデル種類、プロパティ、有限値、光沢の範囲、末尾の余分なデータを確認してから一覧を一括置換する。0体の保存ファイルもそのまま復元する。保存は隣接する一時ファイルに書き終えてからWindowsのファイル置換APIで置き換える。

以前の `PON_SCENE 1` ファイルはIDが一致する既定モデルへ値を適用し、他の既定モデルを残す。バージョン2はActiveとMesh Rendererを有効にして復元する。次にシーンを保存すると現在のバージョン13で保存する。既存ファイルは読み込むだけでは書き換えない。World Settingsの共通光源、共通Particleプリセット、カメラのデバッグ値はシーン保存の対象外。配置した点光源とEmitterの設定は保存する。

## 編集・再生・Component

Toolbarのアイコンから再生・停止・一時停止・再開・ステップを操作する。編集モードではゲーム更新を止め、カメラ移動と描画データ更新だけを行う。Play時にオブジェクトの設定一覧を保存し、Stop時にその一覧を復元する。再生中の追加・削除・Component変更は保存しない。再生中は保存・新規・読込・Undo/Redoを無効化する。

Pauseはシミュレーションだけを止め、Stepは一度だけ1フレーム進める。WorldSceneはMotion・物理・接触・アニメーション・Particleを更新し、Play/Stopの切替時に粒子と発生タイマーをリセットする。ReleaseはEditorを経由せず通常のゲーム更新を行う。今後ゲーム固有の処理をGameScene::Updateへ追加すると、編集・Pause中には呼ばれず、再生またはStep時に実行される。描画とカメラの更新はUpdateEditorFrameで維持する。

Inspectorは必須のTransformと、追加・削除できるMesh Rendererを表示する。Remove Mesh Rendererはモデル設定を保ったまま描画Componentだけを外し、Add Mesh Rendererで戻す。Activeを外したオブジェクトも描画しない。名前・モデル・色・光沢・有効状態・Componentの有無は保存、Undo/Redo、Stop時の復元対象。

ライトとSpriteはWorld Settingsへ集約し、再生中は編集を無効にしている。Particleは専用Editorと独立したプリセット保存に対応する。ライト・Spriteの保存や、Particle設定の履歴対応は今後の範囲。DebugCameraはEditor用カメラとして扱い、Scene画像上のマウスだけで操作する。文字入力・ギズモ・ポップアップ・他パネルの操作中は入力を渡さない。

## 履歴と未保存変更

EditorSessionはImGui/DX12に依存しない。Inspectorのドラッグや文字入力中は履歴を確定せず、操作が終わった時点の状態を1件として記録する。追加・削除・複製、名前、Component設定をUndo/Redoできる。Undo後に新たに編集するとRedo側の履歴を破棄する。

保存時の設定と現在の設定を比較して未保存表示を出す。新規作成・読込・ウィンドウ終了で設定が失われる場合は、保存して続行・破棄して続行・キャンセルの確認を表示する。再生中の終了で保存を選ぶ場合は、Stopで編集用設定へ戻してから保存する。読込成功時は履歴を初期化する。New Sceneは保存先のない空の一覧を作成する。新規・読込の成功時は選択・Undo履歴・操作状態・実行中の一時データ・プレビュー・Editorカメラを初期化する。別名保存ではUndo履歴を保持する。失敗・キャンセル時は現在の配置・保存先・履歴を保持する。共通Particleプリセットの変更状態はシーン切替後も保持する。

ショートカットはCtrl+Sで保存、Ctrl+ZでUndo、Ctrl+YまたはCtrl+Shift+ZでRedo、Ctrl+Dで複製、Deleteで削除、Ctrl+PでPlay/Stop。文字入力中・ドラッグ中・ポップアップ表示中は無効化する。

## 描画画面での選択と移動

Sceneパネルは、専用レンダーターゲットの画像に選択枠とワールドXYZ軸の移動ギズモを重ねる。モデルをクリックして選択し、何もない場所をクリックすると解除する。UIパネル上のクリックはシーンの選択に使わない。無効なモデルやMesh Rendererを外したモデルはクリック選択の対象外だが、Hierarchyから選択してTransformを編集できる。

モデル読込時に全頂点からローカルAABBを求める。WorldSceneはRootNode行列とTransformを描画と同じ順序で適用し、ワールドAABBをEditorへ公開する。クリック位置から近・遠クリップ面の間にレイを作り、交差する範囲のうち手前のモデルを選ぶ。判定はAABB単位であり、穴や透明な部分を含む形状のピクセル単位の選択ではない。

選択すると黄色の範囲枠と移動軸が現れる。軸を左ドラッグして移動し、CtrlでMove Snapの間隔へ合わせる。Escでドラッグ開始位置へ戻す。移動中は履歴の確定を待ち、マウスを離した時点で一件のUndo操作にする。視線に平行で画面上の長さがない軸は操作対象から外すため、その軸を動かす場合はカメラを横へ移すかInspectorを使う。

注視ボタンで選択位置を注視する。背景上の右ドラッグでカメラを回転し、中ボタンドラッグで平行移動、ホイールで前後移動する。WASD・QE・Fによるカメラ操作は行わない。Gridで原点のXZ平面の補助線を表示・非表示にできる。移動ギズモを操作中はカメラ移動と編集ショートカットを抑止する。

SceneViewMathは画面投影、レイ生成、交差判定、範囲の変換、軸上の移動量を担当し、GPUやImGuiから独立してテストする。SceneViewPanelは描画と入力だけを担当する。表示領域はSceneパネルの画像サイズに追従する。SceneRenderTargetが専用RTV/DSV/SRVを所有し、描画後にバックバッファを復元してImGuiへ画像を渡す。SRV番号はリサイズしても維持する。バックバッファもウィンドウサイズに追従する。

### 検証

Visual StudioのDeveloper PowerShellでリポジトリのルートから実行する。

```powershell
MSBuild tests/SceneFileTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
./generated/scene-file-tests/bin/SceneFileTests.exe
```

追加・削除・複製、IDの維持と採番、モデル種類・個数の保存・復元、空のシーン、旧形式の移行、不正ファイルでのデータ保持、Undo/Redo、再生前データの復元、Pause/Stepを検証する。SceneViewMathでは画面投影とレイの一致、描画奥行の制限、平行なレイや平面モデルとの交差、負の拡縮・回転後の範囲、軸ドラッグの計算を検証する。

## レイアウト・Particle Editorの追加

操作と検証手順は[Editorの使い方](editor-guide.md)を参照する。EditorLayoutが初期ドッキングを構築し、ImGuiSystemが日本語フォントと配色を設定する。ParticleSettingsとParticlePresetFileはUI・GPUに依存しない。ParticleEditorPanelは設定への借用参照とプレビュー操作だけを持ち、WorldSceneがParticleへの適用と実行を担当する。


### 種類別オブジェクトと保存形式4

WorldObjectDataはModel・Empty・PointLight・Particleを識別し、光源値とParticleSettingsを値として保持する。
EditorSessionは種類別の値も比較し、複製・Undo/Redo・再生終了時の復元へ含める。
SceneFileの形式4は種類とComponent値を保存する。形式1〜3ではModelとして復元し、既存シーンとの互換性を保つ。
ParticlePresetFileの設定部分の読込・書込を共有し、シーン内Emitterとプリセットで項目の並びを統一する。

WorldSceneはWorldLightingでHierarchy内の有効なPoint / Spot / Directionalを収集し、固定長の各LightBufferをメッシュへ渡す。World Settingsの固定ライトは廃止した。旧シーンの移行と現在の上限は「ライトの統合」を参照。

## engineの基盤フォルダ

`core`はEngineの起動・実行ループ・終了の組み立てだけを担当する。共有の構造体・ID・enum・上限は`types`、WinAppのWindows処理は`platform`、LogSystemとFrameProfilerは`diagnostics`、ImGuiのWin32／DirectX連携は`ui`へ分ける。従来の`base`は使用しない。

typesへ新しい状態管理や初期化処理を追加しない。coreへ新しい機能の実装を積み上げず、各機能を呼び出す順序を管理する。Editorの画面内容はeditorに置き、uiにはImGuiの基盤処理だけを置く。ログと計測はどの機能からでも参照できるdiagnosticsに集める。

## engine/worldのファイル分類

`world/runtime/WorldScene`はISceneとWorldObjectsを継承する再利用用の場面基底であり、保存先や初期配置は決めない。GameScene/TitleSceneが初期配置を決め、InitializeSimulationで最初の移動前に物理の初期位置を取得する。GameScene固有のHUDは共通基底へ置かない。

`world`直下には`IScene`・`IWorldObjects`・`IEditableWorld`と`SceneManager`を置く。ゲーム側・Editor側が共有するシーンの窓口と、実行中のシーン切り替えを担当する。

| フォルダ | 担当ファイルと責務 |
| --- | --- |
| `objects` | `WorldObjectData`の共有データ定義、`WorldObjects`の一覧・ID・親子関係・検証と、`WorldPrimitives`の基本形状作成 |
| `settings` | `AssetSettings`、`ParticleSettings`、Motion・Collider・Body・Lightの設定値と設定単体の処理 |
| `serialization` | `SceneFile`・`PrefabFile`・`ParticlePresetFile`の保存・読込・旧形式への対応 |
| `systems` | `WorldMotion`・`WorldCollision`・`WorldPhysics`の実行時更新と、`WorldLighting`のライト収集・方向計算 |

includeは`objects/WorldObjects.h`や`serialization/SceneFile.h`のように、既存の`engine/world`を起点に分類を明記する。各サブフォルダを追加のinclude検索先にせず、同名ファイルの曖昧な解決を避ける。実際のディレクトリとVisual Studioのフィルターも揃える。

`Scene`はGameScene・TitleScene・シーン遷移などの場面に使う。共通の実行基盤はWorld、Componentの設定はRigidBodySettings・ColliderSettings・LightSettingsなどの機能名に揃える。IScene・SceneContext・SceneManagerは場面の管理、SceneFileは場面の保存、SceneViewはEditorの画面名として残す。

型名・ファイル名と対応するAPI名を変更しても、所有関係・処理内容・保存形式は維持する。`settings`は設定値を持ち、`systems`がワールドを通して実行時の処理を行う。保存処理は`serialization`に集め、UIとGPUリソースの実装を持ち込まない。

### オブジェクト操作とEditor操作の境界

`types/WorldObjectTypes.h`はIDと種類を定義し、シーンの実装に依存しない。`WorldObjectData.h`はその共通型を使い、カメラ設定・保存用データと、フレーム内だけ借用する`EditableObjectView`を定義する。操作のインターフェースをincludeせず、UI・GPUリソースの所有権も持たない。

`IWorldObjects`は一覧・親子関係・Transform・作成・置換・ゲームカメラの共通操作を定義する。Motion・Collision・Physics・Lightingの実行処理、Scene・Prefabの保存処理、基本形状作成はこの型を受け取る。`IEditableWorld`は`IWorldObjects`を継承し、Editorの表示領域・カメラ入力・Asset／Particleプレビューなどを追加する。インターフェースに不要なInitialize／Update／Drawの空実装は増やさない。

`WorldObjects`は既存の`IEditableWorld`実装を維持し、既存のEditorとGameSceneは同じオブジェクトを共通窓口として渡す。変換用オブジェクト・追加の所有権・毎フレームの一覧コピーは導入しない。保存時の`ReplaceObjects`は従来どおり仮想呼出となり、WorldScene側のモデル・テクスチャ準備と失敗時の一括取消を保持する。保存形式は13のまま。

## ライトの統合

WorldObjectTypeの既存数値0〜4を維持し、SpotLight=5 / DirectionalLight=6を追加する。LightSettingsはSpotの内角・外角（全開角・度数）を保存する。既存の色・強度・範囲・減衰とTransformを共有し、別の位置・方向状態を重複所有しない。

ライトの種別・名前・設定検証はsettings/LightSettings.cpp、配列の上限はtypes/LightLimits.hに集める。WorldLighting.cppはワールド方向とGPU用一覧の収集だけを担当し、具体的なWorldObjectsや保存処理を参照しない。旧固定値の移行はserialization/LegacyLightMigration.cppへ分離し、仮の配列を全件検証してから反映する。GameSceneの既定ライト作成も同じ旧固定値を再利用し、従来の初期表示を維持する。

InspectorPanel.cppのLight区間は編集UI、WorldLights.hlsliはライト配列と共通の照明計算を担当する。GameSceneは収集したWorldLightCollectionを各モデルへ適用する。固定長のDirectional 4 / Point 9 / Spot 8を使い、収集時のベクター確保を不要にする。SpotのCPU要素はHLSL配列と同じ64バイトに揃え、各Bufferをstatic_assertとGPUの色比較で確認する。

Scene形式13ではSpot角度を各レコードの末尾に追加する。Editor・GameSceneの通常読込だけでSceneFile::LoadのmigrateLegacyLightsを有効にし、形式1〜12には旧Directional・Point・Spotを1個ずつ追加する。部分データやPrefabの読込には追加しない。移行はローカル配列を検証してから一括置換し、IDは採番済みの範囲を再利用しない。形式1の再読込でも現在の移行ライトを持ち越さず、形式13は再移行しない。
C++とHLSLで先頭16バイト、各光源48バイトの配置を一致させる。
最大32個のEmitterはID別に実行用Particleを所有し、削除・無効化・終了時にSRVを解放する。
Particle Editorは選択IDから毎フレーム設定を取得し、一覧再配置後の借用参照を持ち越さない。

EditorLayoutがScene/Viewメニューと通知欄を描き、選択した操作だけをEditorへ返す。
Toolbarは再生操作だけを図形アイコンで表示し、履歴操作はキーボードショートカットで行う。
フォント倍率は初期化時から0.85に固定する。


## 記述規則と責務の分類

同じ型・用途の変数宣言が続く場合を除き、処理・関数宣言・役割の違う変数の間は空行を1行入れる。説明コメントは対象の宣言・処理に付け、複数行の式・引数・初期化リストの途中には空行を入れない。includeのまとまりは維持する。

`Sprite.h`と同じ区切り線を使い、公開する初期化・更新・描画・終了、Getter/Setter、内部の処理・状態を分類する。見出しには「メンバ関数」「メンバ変数」を付けず、役割を示す名前を使う。各分類の中では、カメラ・履歴・保存確認・Particleなど用途ごとにカテゴリを分ける。ヘッダーには借用参照の寿命、所有者、状態の意味を記載する。

EditorとDebugCameraはInitialize/Update/Drawへ役割を分け、EditorSessionはInitializeで履歴を初期化し、Updateで確定した編集を記録する。保存・比較の専用クラスはその責務をカテゴリとして示す。

三項演算子、ラムダ式、auto、一文字の変数・関数名は使用しない。ifの本体は必ず改行する。修飾キーとキー押下、対象の種類と設定値、ポインタの有無と参照先の値など、役割が違う条件は別の段階へ分ける。同じ数値の範囲など、同じカテゴリの条件はまとめて判定できる。

ローカル変数は再利用する計算値や操作中の借用参照など、必要な範囲だけに置く。一度しか使用しない別名を減らし、長い処理を役割別の関数へ分ける。フレーム内の一時値を不用意にメンバ変数へ移して寿命を延ばさない。

## Sceneカメラ操作時の入力・更新負荷

WinAppはフレーム開始時にメッセージキューを空になるまで処理する。1フレーム1件だけの処理ではマウスとキーのメッセージが後ろへ溜まり、視点操作が遅延し得る。

EditorSessionは読み取り専用のGetObjectDataから保存済み状態や履歴と比較する。シーンのコピーは変更を履歴へ記録する時、保存基準を更新する時、再生状態を退避する時に作る。従来のCaptureObjectsだけを公開するシーンにもフォールバックする。

カメラの投影行列は初期化・表示サイズ変更時に更新する。GameSceneは同じViewportサイズが続く間、カメラとSpriteへ同じサイズを再設定しない。

CPU試験は未変更の120フレームでシーンの複製が増えず、実際の編集で一件だけ複製されることを確認する。GPU試験は自スレッドのキューへ128件の試験メッセージを送り、一回のProcessMessageですべて取り除かれることも検証する。


## 基本タブと任意の補助画面

初期表示はHierarchy・Inspector・Project・Consoleと、中央のSceneView/GameView切替に絞る。中央のScene Viewportはドッキング用の器で、外側のタブバーを非表示にして二重のタブを避ける。再生操作はWorkspace上部へ描き、専用Toolbarウィンドウは作らない。

EditorLayout::Beginがメニューと親領域を開き、Editorが再生操作を描いた後、EditorLayout::DrawがDockSpaceと通知欄を描く。EditorToolVisibilityはEditorが所有する。Viewメニュー・閉じるボタン・InspectorやProjectの表示ボタンがこの表示状態を更新する。

シーン固有の補助画面はDrawEditorToolsで、ImGui受付中に表示要求があったものだけ描く。表示状態は呼出中だけ参照する。Particle Editorを閉じると編集用プレビューを停止するが、ゲーム再生中の粒子には干渉しない。

タブの選択・非選択・ホバーの背景色を揃え、選択上線を透明にする。フォーカス時のタイトルバーも通常時と同じ色にする。レイアウト識別子V4で古い補助タブ入りの初期配置を更新する。


## 親子関係とワールド変換

WorldObjectDataはparentIdで親を参照し、Transformはローカル値として保存する。形式5に親IDを追加し、形式1〜4は親なしとして復元する。WorldObjectsはIDから配列位置への検索表を持ち、親の存在・循環・深さを全件検証してから一覧を置き換える。

ワールド行列は子のローカル行列に祖先の行列を順に掛けて求める。モデル描画、選択範囲、ライト、Emitter、ギズモ、注視で同じ計算を共有する。Object3dには解決済みワールド行列を渡す更新オーバーロードを追加し、モデルのRootNodeは最後に適用する。

SetObjectParentは旧ワールド行列から新しい親に対するローカル行列を求め、TRSへ分解して再構成の誤差を検証する。逆行列を作れない親や、TRSで表せないせん断は近似せず拒否する。位置のギズモ操作はSetObjectWorldPositionで親座標へ戻し、Escでは操作前のローカル値を復元する。

Hierarchyは一度構築した親別の添字一覧でツリーを表示する。ドロップ結果は描画後に適用し、一覧の置換で借用参照が無効になるタイミングを分離する。複製では子孫のIDをすべて新規採番し、親IDを複製先へ付け替える。削除では子孫をまとめて取り除く。各操作は履歴スナップショット一件として扱う。


Hierarchyの操作ボタンは右クリックメニューへまとめる。行の右ボタン解放時には対象IDと要求だけを記録し、PushIDとツリーの描画を終了してから共通Popupを開く。空白部分の右クリックは対象なしの新規作成メニューとする。追加・削除・複製による一覧の変更はツリー描画後に行い、古い借用参照を使わない。最上位へのドロップ先も常時表示の行から空白部分へ移す。


## SceneカメラとProjectブラウザ

形式6にCameraSettingsとmainCameraを追加する。SetMainCameraで一意に指定し、複製先はメイン指定を引き継がない。GetGameCameraViewは親子変換から位置・向きを解決し、カメラ基底を正規化して拡縮を視野に持ち込まない。GameViewとReleaseで同じ計算を共有する。

ProjectPanelはInitialize/Update/Drawでフォルダ一覧を管理する。ファイル走査は初期化・フォルダ変更・明示更新時に限り、毎フレームはキャッシュ済み一覧を描く。resources外への移動とシンボリックリンクの追跡は行わない。画像は可視タイルから一フレーム一件だけ読み込み、TextureManager所有のSRVを借用する。TryLoadは画像デコード失敗を返し、不完全なTextureDataをキャッシュへ登録しない。

## アセット編集・Prefab・アニメーション

AssetSettingsはGPUに依存しないスロット上書きと再生設定を定義する。SceneFile形式7、EditorSession、PrefabFileが同じデータを使う。PrefabはSceneFileの検証と原子的保存を再利用し、一つのルートだけを許可する。Instantiateは一時WorldObjectsでID・親・配置・個数上限を検証し、最後にIEditableWorld.ReplaceObjectsへ渡す。GameSceneはモデルと画像を準備してから編集データを置き換える。

Modelは共有する元頂点・マテリアル情報・描画範囲・ModelAnimationを所有する。Object3dはスロットごとのMaterial、再生時計、変形用Meshを個別に所有する。ModelAnimationはAssimpのノード階層・キー・ボーンと逆バインド行列を保持し、位置・回転・拡縮を補間してCPUスキニングする。頂点と法線を変形し、描画時と選択時で同じ範囲を使う。プレビューの再生時計は履歴に含めず、開始・停止時にリセットする。共有のModelの頂点は更新しない。

AssetWorkflowTestsは保存・履歴・Prefabの再配置と失敗時の非変更を検証する。EditorGraphicsTestsは自作glTFで複数スロット、ボーンの移動・回転・拡縮、ループ・終端、独立した再生速度とPauseを確認する。


## 複数選択・表面スナップ・Prefab接続

EditorSelectionは順序付きID一覧と選択判定用ハッシュ集合を持つ。WorldObjectsのID検索を公開し、選択確認のために毎フレーム一覧やハッシュ表を作り直さない。一括複製・削除は子孫を一度だけ列挙し、所有スナップショットを一回置換する。WorldObjectsの次回採番値を使い、削除済みIDを再利用しない。

表面スナップはAABBで候補を絞り、Object3dの現在のスキニング済み頂点またはModelの元頂点と、描画と同じRootNode・ワールド行列で三角形交差を求める。三角形走査はドロップ確定時だけ実施する。通常クリックの選択は引き続きAABBで判定する。テクスチャの透明部分は表面判定へ反映しない。

形式8はルートのprefabSourceをresources相対パスとして保存する。Applyは原子的なPrefab保存を再利用し、Revertは一時シーン内で子孫を入れ替えてからリソースを検証する。ルートID・親・Transformを維持し、子孫IDは再採番する。参照先は正規化してresources内であることを確認する。自動追従・入れ子Prefab・個別Override管理は行わない。

## CPUフレーム計測

FrameProfilerはsteady_clockと固定長600サンプルで計測し、RAII Scopeで区間時間を積算する。Engineのフレーム境界でリセット／確定し、MeshのDrawIndexedInstanced直前で描画回数・三角形・インスタンスを記録する。ImGuiは描画領域とクリップを考慮してコマンド数を別集計する。モデル読込とテクスチャ読込は別区間で、親区間への二重積算を避ける。

DirectXCommonのPresent、フェンス待機、FPS制限も別区間に分ける。CPU区間はsteady_clockによる経過時間、GPU executionはDirectXCommonのGPUタイムスタンプ差分として分離する。フレーム全体には入力処理や区間外の描画先準備も含む。履歴停止はゲームを止めず、ピークはリング外へ流れた後も保持する。表示メニューから任意に開く画面で詳細と履歴CSVを確認する。

EditorWorkflowTestsは選択ルートの重複排除、一括操作の履歴・上限失敗・ID非再利用、三角形交差、Prefabリンクの保存・Apply・Revertと失敗時の非変更、計測リング・カウンター・CSVを検証する。実GPU試験では表面判定、任意表示のPerformance画面、描画カウンターも確認する。

GPU計測は2要素のTimestamp QueryHeapとREADBACKバッファを使用する。PreDrawで開始、PostDrawで終了とResolveQueryDataを記録する。既存フェンス完了後だけMapし、キューのGetTimestampFrequencyでmsへ変換してFrameProfilerの現在フレームへ渡す。計測のための追加フェンス待機は行わない。初期化・Map・デバイス喪失時の失敗や未取得は有効フラグで区別し、古いフレームのGPU値を再利用しない。


## ゲーム動作Component

MotionSettingsは回転・往復移動の存在、有効フラグ、速度、変位、周期を保持する保存用の値型。SceneFile形式9、EditorSession、Prefabの既存スナップショットに統合し、非有限値、不正フラグ、0以下の周期等は部分反映せず拒否する。

WorldMotionはGPUやImGuiに依存せず、IDごとの開始位置・時計だけを所有する。回転は度/秒からローカルEuler角へ変換し、往復移動は親座標系の変位を余弦で補間する。Activeでない階層や無効なComponentは進めない。削除・取り外し済みの移動状態は破棄する。

GameScene::Updateのカメラ・描画用行列の更新より前に呼ぶ。UpdateEditorFrameの非シミュレーション経路では呼ばず、Pauseと編集時に動かないようにする。OnPlayStateChangedで実行状態を初期化し、Transformの復元はEditorSessionの停止時スナップショットに任せる。Releaseも同じUpdateを使う。固定更新時間は既存の1/60秒を共有する。

MotionTestsは周期・回転・親子変換・無効状態・取り外し後の再追加・保存・Prefab・履歴・再生停止・旧形式互換・不正値の拒否を確認する。実GPU試験は本番のGameSceneとEditorSessionを通し、Play/Pause/Step/StopとInspector描画を確認する。


## Colliderと接触検出

ColliderSettingsは存在・有効・形状・ローカル中心・Box全幅・球半径を保持する。SceneFile形式10に追加し、WorldObjectsの検証と256個の上限、EditorSessionの比較、Prefabの既存保存経路へ統合する。

WorldCollisionはImGuiとログに依存しない。ワールド形状を構築してX軸の下限で並べ、AABB候補だけを詳細判定へ送る。Box同士は面法線6本と辺の交差9本の分離軸判定、Boxと球は内部判定と12三角形上の最近点、球同士は中心間距離を使う。Boxは変換された半辺ベクトルで保持し、親子のせん断にも対応する。球の外接半径はGram行列の最大固有値に対する行和上界を使う。通常の直交基底では最大拡縮と一致する。

IDを昇順にした接触ペア集合を比較し、開始・終了だけを通知する。GameSceneはWorldMotionの後で検出し、Consoleへ一更新最大32件を記録する。残りは省略件数として通知する。編集・Pause中は更新せず、Play/Stopで接触履歴を初期化する。SceneViewは同じWorldColliderから選択中の形状を描き、GPU側の別Collider資源を作らない。

CollisionTestsは三種の組み合わせ、接線、回転Boxの空白、負の拡縮・せん断、階層の無効化・削除と通知差分、保存・Prefab・Undo/Redo・上限・旧形式を確認する。実GPU試験はGameSceneとConsole通知、Pause、停止時のリセット、Inspector描画を通して確認する。


## Rigidbodyと並進物理

RigidBodySettingsはStatic/Dynamic/Kinematic、重力、質量、初速、反発、摩擦、減衰を保持する。形式11で旧Collider設定の後へTriggerとRigidbodyを追加し、旧形式1〜10を維持する。EditorSessionは設定を比較し、再生前にWorldObjectsの全体検証も行う。親子の独立したDynamicによる二重移動は階層検証で拒否する。

WorldCollision.GetContactは既存形状計算を共有し、first→secondの法線と侵入深さを返す。Box同士は最小分離軸、Box/Sphereは最近面・最近点、Sphere同士は中心間距離から求める。

WorldPhysicsはWorldMotionの後、接触通知と描画用行列の更新の前に実行する。1/60秒を4サブステップに分けて重力と速度を積分し、X軸の候補絞り込みと8回の反復で質量比に応じた位置補正・法線インパルス・接線摩擦を適用する。回転の慣性や角速度は扱わない。Kinematicの平行移動は親から子の順で各サブステップへ補間し、相手の速度との比較にも反映する。Dynamicに往復移動の位置更新を重ねない。

子ColliderはIEditableWorld.GetBodyOwnerで最も近い有効なRigidbodyへ結び付け、同じ所有者の接触は解決・通知ともに除外する。Triggerは通知だけを行う。速度・前回位置はIDに対応するWorldPhysicsの実行状態に保持し、シーンファイル・Prefab・履歴へ入れない。Play時に初速を取得し、Stopで破棄する。

PhysicsTestsはBox/Sphereの着地、壁、質量比、反発、摩擦、Trigger、親座標への変換、子Collider、動く床、重力オフ、無効化、坂、保存・履歴・停止・旧形式互換を検証する。実GPU試験でも本番のGameScene経路で落下・接地・Pause・停止時の復元とRigidbodyのInspectorを確認する。


## Editorパネルの責務と実装ファイル

`editor/panels`はタブ単位のフォルダへ分類し、各クラスの宣言は同名の.h、実装は同名の.cppへまとめる。複数cppへ同じクラスを分散せず、機能ごとの区切りコメントで分類する。型やinline関数だけの.hは許可し、空のcppや形式だけのヘッダーは増やさない。起動入口のmain.cppは同名ヘッダーを必要としない。

GameScene・Editor・Asset・描画・入力・物理など、対応するcppがある自作コード全体で、cppの分類名と関数の並びをhの宣言に合わせる。関数区分は「メンバ関数 / Motion」ではなく「Motion」のように内容だけを書く。コンストラクタとデストラクタは生成・破棄として分類する。補助関数・内部データの定義はcpp内の実装用の枠へ分ける。h内で完結するGetter／Setterやメンバー変数には、cppへ対応する空の見出しを作らない。namespaceや通常の関数も、宣言側の分類と並びに合わせる。

| タブ | フォルダ | .h / .cppの名前 | 担当 |
| --- | --- | --- | --- |
| Hierarchy | hierarchy | HierarchyPanel | 親子ツリー・作成・削除・複製・右クリック |
| Inspector | inspector | InspectorPanel | 単体／複数選択・各Componentの編集 |
| Project | project | ProjectPanel | フォルダ走査・一覧・プレビュー・Asset操作 |
| Console | console | ConsolePanel | ログの検索・一覧・全文確認・コピー |
| SceneView | scene_view | SceneViewPanel | 共通タブ・編集カメラ・選択・配置・ギズモ・補助表示 |
| GameView | game_view | GameViewPanel | ゲームカメラの状態と画像表示 |
| Particle Editor | particle_editor | ParticleEditorPanel | Emitter設定・プリセット・編集プレビュー |
| Performance | performance | PerformancePanel | 計測履歴・ピーク表示・CSV出力結果 |

Editorは選択と履歴を所有し、InspectorPanel::DrawSelectionへ選択を借用させる。InspectorPanelが単体と複数選択の表示を切り替え、Editorのメンバー関数を別名cppへ実装する形は使わない。

SceneViewPanelは選択・ドラッグ・スナップ・共通タブの状態を所有し、その全実装をSceneViewPanel.cppへまとめる。GameViewPanelは状態を重複所有しない関数群として、共通の画像ハンドルと寸法を借用する。SceneViewDrawing.hは画面位置への変換と線分描画のinline関数だけを持つ、ヘッダーのみの補助として残す。

ProjectPanelはディレクトリ・選択・プレビューの状態を所有し、画面構成とAsset操作もProjectPanel.cppへ置く。Consoleはログを描画中だけ借用し、Projectのキャッシュや更新処理には依存しない。PerformancePanelは履歴の選択とCSV出力結果を自身のメンバーで保持する。

今回の統合では保存形式と操作仕様を維持する。CPUの保存・履歴テスト、専用ImGuiコンテキストでのギズモ・タブ切替・単体／複数Inspectorの操作テストと、実GPUの描画テストで確認する。

## 場面遷移とEditor停止

ISceneは未初期化の次シーンをunique_ptrで要求し、SceneManagerが所有権を引き取る。更新・描画中は現在の場面を保持し、EngineのPostDrawがGPU完了を待った後にApplyPendingSceneで切り替える。初期化済みの場面はFinalize後に破棄し、初期化前に取り消した要求はデストラクタだけで解放する。同じフレームの複数要求は最初の有効なものを採用する。

再生中の最初の遷移で、SceneManagerは再生開始時の編集シーンをeditScene_へ退避し、同じインスタンスを停止まで保持する。それ以降の一時シーンは遷移の都度解放する。EditorはplayOrigin_を借用し、EditorSessionの履歴・保存基準・再生前スナップショットを保持する。Stopでは元の編集シーンにだけスナップショットを復元し、フレーム境界で元のインスタンスへ戻す。停止と遷移要求が重なった場合は停止を優先する。

OnSceneChangedでは選択・ドラッグ・Inspectorの操作結果とParticleプレビュー対象を初期化し、Projectが借用するモデル画像ハンドルのキャッシュを破棄する。履歴は遷移先の配置で作り直さない。ParticleEditorPanelの元の保存基準も遷移中だけ退避し、共通プリセットの未保存状態を保持する。ツールバーの場面名も現在のISceneから渡す。SceneManagerとWorldSceneはEditorやapplicationの具体クラスをincludeしない。

CPU試験は描画完了前に破棄しないこと、Pause、連続遷移、停止優先、同じ編集インスタンスへの復帰、Finalizeの回数を確認する。GPU試験は実際のTitleScene→GameScene→TitleSceneを描画し、ImGui自身へのCtrl+P入力で再生・停止して、保存基準x=2から編集したx=3が再生中にx=9へ変わっても、停止後にx=3へ戻り、Undoでx=2・Redoでx=3へ戻せることを確認する。

## Assetの移動・削除と参照更新

ProjectPanelはAssetActionDialogへ操作要求を渡す。AssetActionDialogが使用箇所と確認画面を表示し、AssetFileOperationsが検証・ステージング・確定・失敗時の復元を担当する。AssetDocumentはScene/Prefab/Particleの既存シリアライズと、OBJ/MTL/glTF/GLBの外部参照を扱う。ファイル操作とパネル描画を一つのクラスへ集めない。

AssetPathChangeはresourcesのルートと変更前後の相対パスだけを保持する。フォルダの要素単位で比較し、Windowsの序数比較で大文字・小文字を扱う。WorldAssetReferencesが保存データの参照を共通規則で列挙・更新する。移動の成功後、IEditableWorld::RemapAssetPathsを通じて実行側へ通知し、EditorSessionが履歴・保存基準を、ParticleEditorPanelが共通プリセットの保存基準を更新する。EditorからModelManagerやTextureManagerを直接操作しない。

WorldSceneはモデル・テクスチャ・モデルプレビューのキャッシュキーを変更し、既存のGPUリソース・モデルへのポインタ・画像ハンドルを保つ。移動先に過去のキャッシュが残っている場合、その古いリソースは受付済みの描画を壊さないよう終了時まで保持する。追加のGPU待機や全モデルの再インポートは行わない。Projectの表示キャッシュは成功通知で更新する。

移動先にファイルがない準備段階では、WorldObjects::ReplaceObjectsとSceneFile::SaveのrequireModelFiles=falseを明示して、パスとデータの形を検証する。通常の追加・保存・読込は既定値trueを使い、モデルの存在検証を維持する。Particleの画像は安全なresources相対パスとして検証し、実行側では読込に失敗した画像を適用しない。

確定前に文書の原本が変わっていないことを確認し、更新文書と変更前のコピーを非表示の回復ディレクトリへ用意する。Assetを二段階のrenameで移し、更新文書を原子的な置換APIで順番に確定する。途中失敗時は適用済みの原本とAssetのパスを復元する。これは複数ファイルをまたぐOSトランザクションではないため、プロセス強制終了への自動回復は実装せず、回復情報と原本を残す。

削除時は再調査し、編集中の配置・保存文書・Undo/Redoの使用箇所が残っていれば拒否する。未使用のものも物理削除せずゴミ箱へ移す。復元と操作範囲の制限はREADMEの「Projectのファイル操作」を参照する。

CPU試験では、Scene・Prefab・Particle・OBJ/MTL・glTF/GLBの更新、空白・Unicode・大文字小文字、旧形式のライト移行、既存ファイルへの上書き拒否、範囲外と自己配下への移動拒否、使用中Assetの削除拒否、ファイルロックによる確定失敗時の復元を確認する。GPU試験では読込済みのモデル・画像・プレビューの同一性、移動後のUndo、空白入り画像名でのAssimp再インポート、継続描画を確認する。専用ImGuiコンテキストで右クリックメニューの使用箇所確認も操作する。
