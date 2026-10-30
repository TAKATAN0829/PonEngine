# Editorの最初の実装

Debug / Development で起動し、Hierarchy のモデルを選択すると、Inspector でモデル種類・位置・回転・拡縮・色・ライティング・光沢・RootNode行列の適用を編集できる。Clear Selection で選択を解除する。パネルは既存のDockSpaceにドッキングできる。

HierarchyのAdd Plane / Add Axisで追加、Duplicateで選択対象を複製、Deleteで削除する。追加・複製後は新しい対象を選択する。複製は同じ位置に作られるため、Inspectorで位置を変更すると分離できる。最後のモデルを削除して空のシーンにすることもできる。

## 依存と責務

```text
Engine → Editor → HierarchyPanel / InspectorPanel → ImGui
            ↓
       IEditableScene ← SceneObjects ← GameScene
Engine → SceneManager → IScene
```

- ImGuiSystem: ImGuiの初期化、フレーム開始、DX12への描画、終了。
- Editor: 選択状態とパネル呼び出しの調整。Engineが値として所有する。
- HierarchyPanel: 編集対象一覧の表示と選択。
- InspectorPanel: 渡されたデータの表示と編集。
- IEditableScene: ImGuiを含まない編集対象の列挙・操作契約。
- SceneObjects: 設定データの一覧、ID採番、追加・削除・複製、全件置換と検証。描画機能には依存しない。
- GameScene: SceneObjectsの設定を使い、IDごとのObject3dを所有して更新・描画する。

GameSceneはEditorの具体クラスを知らない。EditorもGameSceneを知らず、IEditableSceneを受け取る。この引数渡しが依存性の注入に当たる。最初は複雑なイベント機構や汎用リフレクションを導入せず、小さなインターフェースで境界を設けている。

## 所有権と更新順

EditableObjectView内のポインタは借用であり、Editor側から破棄しない。追加・削除・読込で無効化されるため、操作後に取得し直し、保持しない。選択にはオブジェクトIDを保持し、毎フレーム検索して対象が存在しなければ解除する。IDはシーン内で一意で、削除や読込後も同一実行中の採番は巻き戻さない。

ImGuiフレーム開始 → Editorで値を編集 → GameSceneの更新 → シーン描画 → ImGui描画、の順に処理し、モデルへの反映が翌フレームへ遅れないようにしている。

設定データとGPUリソースの寿命を分け、GameSceneの更新時に追加・削除を描画用一覧へ反映する。現在のDirectXCommon::PostDrawは毎フレームGPUの完了を待つため、前フレームの参照が終わってからGPUリソースを解放できる。将来フレーム並列化する場合はFenceに基づく遅延解放へ変更する。

メッシュはModelManagerのModelを共有し、色・ライティング・光沢・テクスチャ指定を持つMaterialはObject3dごとに所有する。同じモデルを複製しても、一方の色編集が他方へ波及しない。

## 現在の範囲と次の課題

既定ではPlaneとAxisの二体を作成する。モデルの種類は読み込み済みのplane.gltfとaxis.objに限定し、最大10,000体まで扱う。ライト、パーティクル、カメラの既存デバッグUIはそのまま使える。回転・拡縮ギズモ、任意アセットの取り込み、ユーザースクリプトはまだ実装していない。埋込みScene/Gameビュー、ドッキング、Particle Editorは実装済み。

シーン切替はまだ存在しないため、現在はシーンインターフェースのアドレス変更で選択を解除する。シーンの再生成・切替を導入する際は、同じアドレスが再利用されても識別できるシーン世代番号や明示的な切替通知を追加する。

Undo/Redoは現在、設定データ一覧のスナップショットを最大64状態保持する。ゲームオブジェクトや描画リソースを履歴へ格納しない。将来、大規模なシーンで履歴のメモリを抑える場合は「対象ID・変更前・変更後」を持つCommandへ移行する。

## シーンの保存・読込

「シーン / Scene → 保存」またはCtrl+Sで `project/CG2/resources/scenes/GameScene.ponscene` に保存し、同メニューの読込またはCtrl+Oで復元する。実行時のパスは作業ディレクトリ基準の `resources/scenes/GameScene.ponscene`。既存のテクスチャと同様、作業ディレクトリは `project/CG2` とする。

保存済みファイルがあればGameSceneの初期化時に自動読込する。Releaseでも読込できる。ファイルがない場合は既定値で開始し、壊れたファイルは値を反映せずログにエラーを出す。保存はメニューまたはショートカットから明示的に行う。

SceneFileはEditorに依存せず、IEditableSceneを引数に受け取る。コレクション対応シーンはテキスト形式 `PON_SCENE 5` で、個数、ID、Transform・色・光沢・ライティング、名前、モデル種類、RootNode行列の適用、Active、Mesh Rendererの有無、オブジェクトの種類、光源値、Emitter設定を記録する。浮動小数点は往復可能な桁数を使う。

読込ではバージョン、件数、IDの重複、モデル種類、プロパティ、有限値、光沢の範囲、末尾の余分なデータを確認してから一覧を一括置換する。0体の保存ファイルもそのまま復元する。保存は隣接する一時ファイルに書き終えてからWindowsのファイル置換APIで置き換える。

以前の `PON_SCENE 1` ファイルはIDが一致する既定モデルへ値を適用し、他の既定モデルを残す。バージョン2はActiveとMesh Rendererを有効にして復元する。次にシーンを保存するとバージョン5で保存する。既存ファイルは読み込むだけでは書き換えない。World Settingsの共通光源、共通Particleプリセット、カメラのデバッグ値はシーン保存の対象外。配置した点光源とEmitterの設定は保存する。

## 編集・再生・Component

Toolbarのアイコンから再生・停止・一時停止・再開・ステップを操作する。編集モードではゲーム更新を止め、カメラ移動と描画データ更新だけを行う。Play時にオブジェクトの設定一覧を保存し、Stop時にその一覧を復元する。再生中の追加・削除・Component変更は保存しない。再生中は保存・新規・読込・Undo/Redoを無効化する。

Pauseはシミュレーションだけを止め、Stepは一度だけ1フレーム進める。現在のGameSceneで時間経過する処理はパーティクルで、Play/Stopの切替時に粒子と発生タイマーをリセットする。ReleaseはEditorを経由せず通常のゲーム更新を行う。今後ゲーム固有の処理をGameScene::Updateへ追加すると、編集・Pause中には呼ばれず、再生またはStep時に実行される。描画とカメラの更新はUpdateEditorFrameで維持する。

Inspectorは必須のTransformと、追加・削除できるMesh Rendererを表示する。Remove Mesh Rendererはモデル設定を保ったまま描画Componentだけを外し、Add Mesh Rendererで戻す。Activeを外したオブジェクトも描画しない。名前・モデル・色・光沢・有効状態・Componentの有無は保存、Undo/Redo、Stop時の復元対象。

ライトとSpriteはWorld Settingsへ集約し、再生中は編集を無効にしている。Particleは専用Editorと独立したプリセット保存に対応する。ライト・Spriteの保存や、Particle設定の履歴対応は今後の範囲。DebugCameraはEditor用カメラとして扱い、キーボード入力をUIが使う間やCtrlキーを押している間は移動しない。

## 履歴と未保存変更

EditorSessionはImGui/DX12に依存しない。Inspectorのドラッグや文字入力中は履歴を確定せず、操作が終わった時点の状態を1件として記録する。追加・削除・複製、名前、Component設定をUndo/Redoできる。Undo後に新たに編集するとRedo側の履歴を破棄する。

保存時の設定と現在の設定を比較して未保存表示を出す。新規作成・読込・ウィンドウ終了で設定が失われる場合は、保存して続行・破棄して続行・キャンセルの確認を表示する。再生中の終了で保存を選ぶ場合は、Stopで編集用設定へ戻してから保存する。読込成功時は履歴を初期化する。New Sceneは空の一覧を作成し、Undo可能。

ショートカットはCtrl+Sで保存、Ctrl+ZでUndo、Ctrl+YまたはCtrl+Shift+ZでRedo、Ctrl+Dで複製、Deleteで削除、Ctrl+PでPlay/Stop。文字入力中・ドラッグ中・ポップアップ表示中は無効化する。

## 描画画面での選択と移動

Sceneパネルは、専用レンダーターゲットの画像に選択枠とワールドXYZ軸の移動ギズモを重ねる。モデルをクリックして選択し、何もない場所をクリックすると解除する。UIパネル上のクリックはシーンの選択に使わない。無効なモデルやMesh Rendererを外したモデルはクリック選択の対象外だが、Hierarchyから選択してTransformを編集できる。

モデル読込時に全頂点からローカルAABBを求める。GameSceneはRootNode行列とTransformを描画と同じ順序で適用し、ワールドAABBをEditorへ公開する。クリック位置から近・遠クリップ面の間にレイを作り、交差する範囲のうち手前のモデルを選ぶ。判定はAABB単位であり、穴や透明な部分を含む形状のピクセル単位の選択ではない。

選択すると黄色の範囲枠と移動軸が現れる。軸を左ドラッグして移動し、CtrlでMove Snapの間隔へ合わせる。Escでドラッグ開始位置へ戻す。移動中は履歴の確定を待ち、マウスを離した時点で一件のUndo操作にする。視線に平行で画面上の長さがない軸は操作対象から外すため、その軸を動かす場合はカメラを横へ移すかInspectorを使う。

FまたはFocus Selectionで選択位置を注視する。背景上の右ドラッグでカメラを回転し、WASDとQEで移動する。Gridで原点のXZ平面の補助線を表示・非表示にできる。移動ギズモを操作中はカメラ移動と編集ショートカットを抑止する。

SceneViewMathは画面投影、レイ生成、交差判定、範囲の変換、軸上の移動量を担当し、GPUやImGuiから独立してテストする。SceneViewPanelは描画と入力だけを担当する。表示領域はSceneパネルの画像サイズに追従する。SceneRenderTargetが専用RTV/DSV/SRVを所有し、描画後にバックバッファを復元してImGuiへ画像を渡す。SRV番号はリサイズしても維持する。バックバッファもウィンドウサイズに追従する。

### 検証

Visual StudioのDeveloper PowerShellでリポジトリのルートから実行する。

```powershell
MSBuild tests/SceneFileTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
./generated/scene-file-tests/bin/SceneFileTests.exe
```

追加・削除・複製、IDの維持と採番、モデル種類・個数の保存・復元、空のシーン、旧形式の移行、不正ファイルでのデータ保持、Undo/Redo、再生前データの復元、Pause/Stepを検証する。SceneViewMathでは画面投影とレイの一致、描画奥行の制限、平行なレイや平面モデルとの交差、負の拡縮・回転後の範囲、軸ドラッグの計算を検証する。

## レイアウト・Particle Editorの追加

操作と検証手順は[Editorの使い方](editor-guide.md)を参照する。EditorLayoutが初期ドッキングを構築し、ImGuiSystemが日本語フォントと配色を設定する。ParticleSettingsとParticlePresetFileはUI・GPUに依存しない。ParticleEditorPanelは設定への借用参照とプレビュー操作だけを持ち、GameSceneがParticleへの適用と実行を担当する。


### 種類別オブジェクトと保存形式4

SceneObjectDataはModel・Empty・PointLight・Particleを識別し、光源値とParticleSettingsを値として保持する。
EditorSessionは種類別の値も比較し、複製・Undo/Redo・再生終了時の復元へ含める。
SceneFileの形式4は種類とComponent値を保存する。形式1〜3ではModelとして復元し、既存シーンとの互換性を保つ。
ParticlePresetFileの設定部分の読込・書込を共有し、シーン内Emitterとプリセットで項目の並びを統一する。

GameSceneは最大8個の配置点光源をWorld Settingsの光源へ加算し、PointLightBufferを各メッシュに転送する。
C++とHLSLで先頭16バイト、各光源48バイトの配置を一致させる。
最大32個のEmitterはID別に実行用Particleを所有し、削除・無効化・終了時にSRVを解放する。
Particle Editorは選択IDから毎フレーム設定を取得し、一覧再配置後の借用参照を持ち越さない。

EditorLayoutがScene/Viewメニューと通知欄を描き、選択した操作だけをEditorへ返す。
Toolbarは再生操作だけを図形アイコンで表示し、履歴操作はキーボードショートカットで行う。
フォント倍率は初期化時から0.85に固定する。


## 記述規則と責務の分類

`Sprite.h`と同じ区切り線を使い、公開する初期化・更新・描画・終了、Getter/Setter、内部のメンバ関数、メンバ変数を分類する。各分類の中では、カメラ・履歴・保存確認・Particleなど用途ごとにカテゴリを分ける。ヘッダーには借用参照の寿命、所有者、状態の意味を記載する。

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

SceneObjectDataはparentIdで親を参照し、Transformはローカル値として保存する。形式5に親IDを追加し、形式1〜4は親なしとして復元する。SceneObjectsはIDから配列位置への検索表を持ち、親の存在・循環・深さを全件検証してから一覧を置き換える。

ワールド行列は子のローカル行列に祖先の行列を順に掛けて求める。モデル描画、選択範囲、ライト、Emitter、ギズモ、注視で同じ計算を共有する。Object3dには解決済みワールド行列を渡す更新オーバーロードを追加し、モデルのRootNodeは最後に適用する。

SetObjectParentは旧ワールド行列から新しい親に対するローカル行列を求め、TRSへ分解して再構成の誤差を検証する。逆行列を作れない親や、TRSで表せないせん断は近似せず拒否する。位置のギズモ操作はSetObjectWorldPositionで親座標へ戻し、Escでは操作前のローカル値を復元する。

Hierarchyは一度構築した親別の添字一覧でツリーを表示する。ドロップ結果は描画後に適用し、一覧の置換で借用参照が無効になるタイミングを分離する。複製では子孫のIDをすべて新規採番し、親IDを複製先へ付け替える。削除では子孫をまとめて取り除く。各操作は履歴スナップショット一件として扱う。


Hierarchyの操作ボタンは右クリックメニューへまとめる。行の右ボタン解放時には対象IDと要求だけを記録し、PushIDとツリーの描画を終了してから共通Popupを開く。空白部分の右クリックは対象なしの新規作成メニューとする。追加・削除・複製による一覧の変更はツリー描画後に行い、古い借用参照を使わない。最上位へのドロップ先も常時表示の行から空白部分へ移す。
