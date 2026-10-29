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

既定ではPlaneとAxisの二体を作成する。モデルの種類は読み込み済みのplane.gltfとaxis.objに限定し、最大10,000体まで扱う。ライト、パーティクル、カメラの既存デバッグUIはそのまま使える。埋込みのSceneビューポート、回転・拡縮ギズモ、任意アセットの取り込み、ユーザースクリプトはまだ実装していない。

シーン切替はまだ存在しないため、現在はシーンインターフェースのアドレス変更で選択を解除する。シーンの再生成・切替を導入する際は、同じアドレスが再利用されても識別できるシーン世代番号や明示的な切替通知を追加する。

Undo/Redoは現在、設定データ一覧のスナップショットを最大64状態保持する。ゲームオブジェクトや描画リソースを履歴へ格納しない。将来、大規模なシーンで履歴のメモリを抑える場合は「対象ID・変更前・変更後」を持つCommandへ移行する。

## シーンの保存・読込

SceneパネルのSave Sceneで `project/CG2/resources/scenes/GameScene.ponscene` に保存し、Load Sceneで復元する。実行時のパスは作業ディレクトリ基準の `resources/scenes/GameScene.ponscene`。既存のテクスチャと同様、作業ディレクトリは `project/CG2` とする。

保存済みファイルがあればGameSceneの初期化時に自動読込する。Releaseでも読込できる。ファイルがない場合は既定値で開始し、壊れたファイルは値を反映せずログにエラーを出す。保存はボタンを押したときのみ行う。

SceneFileはEditorに依存せず、IEditableSceneを引数に受け取る。コレクション対応シーンはテキスト形式 `PON_SCENE 3` で、個数、ID、Transform・色・光沢・ライティング、名前、モデル種類、RootNode行列の適用、Active、Mesh Rendererの有無を記録する。浮動小数点は往復可能な桁数を使う。

読込ではバージョン、件数、IDの重複、モデル種類、プロパティ、有限値、光沢の範囲、末尾の余分なデータを確認してから一覧を一括置換する。0体の保存ファイルもそのまま復元する。保存は隣接する一時ファイルに書き終えてからWindowsのファイル置換APIで置き換える。

以前の `PON_SCENE 1` ファイルはIDが一致する既定モデルへ値を適用し、他の既定モデルを残す。バージョン2はActiveとMesh Rendererを有効にして復元する。次にSave Sceneを押すとバージョン3で保存する。既存ファイルは読み込むだけでは書き換えない。テクスチャの任意変更、ライト、パーティクル、カメラのデバッグUIの値は保存対象外。

## 編集・再生・Component

SceneパネルからPlay / Stop / Pause / Resume / Stepを操作する。編集モードではゲーム更新を止め、カメラ移動と描画データ更新だけを行う。Play時にオブジェクトの設定一覧を保存し、Stop時にその一覧を復元する。再生中の追加・削除・Component変更は保存しない。再生中は保存・新規・読込・Undo/Redoを無効化する。

Pauseはシミュレーションだけを止め、Stepは一度だけ1フレーム進める。現在のGameSceneで時間経過する処理はパーティクルで、Play/Stopの切替時に粒子と発生タイマーをリセットする。ReleaseはEditorを経由せず通常のゲーム更新を行う。今後ゲーム固有の処理をGameScene::Updateへ追加すると、編集・Pause中には呼ばれず、再生またはStep時に実行される。描画とカメラの更新はUpdateEditorFrameで維持する。

Inspectorは必須のTransformと、追加・削除できるMesh Rendererを表示する。Remove Mesh Rendererはモデル設定を保ったまま描画Componentだけを外し、Add Mesh Rendererで戻す。Activeを外したオブジェクトも描画しない。名前・モデル・色・光沢・有効状態・Componentの有無は保存、Undo/Redo、Stop時の復元対象。

ライト、Sprite、パーティクル設定の既存デバッグUIは再生中の編集を無効にしている。これらの設定を編集する専用Componentや、その保存・履歴対応は今後の範囲。DebugCameraはEditor用カメラとして扱い、キーボード入力をUIが使う間やCtrlキーを押している間は移動しない。

## 履歴と未保存変更

EditorSessionはImGui/DX12に依存しない。Inspectorのドラッグや文字入力中は履歴を確定せず、操作が終わった時点の状態を1件として記録する。追加・削除・複製、名前、Component設定をUndo/Redoできる。Undo後に新たに編集するとRedo側の履歴を破棄する。

保存時の設定と現在の設定を比較して未保存表示を出す。新規作成・読込・ウィンドウ終了で設定が失われる場合は、保存して続行・破棄して続行・キャンセルの確認を表示する。再生中の終了で保存を選ぶ場合は、Stopで編集用設定へ戻してから保存する。読込成功時は履歴を初期化する。New Sceneは空の一覧を作成し、Undo可能。

ショートカットはCtrl+Sで保存、Ctrl+ZでUndo、Ctrl+YまたはCtrl+Shift+ZでRedo、Ctrl+Dで複製、Deleteで削除、Ctrl+PでPlay/Stop。文字入力中・ドラッグ中・ポップアップ表示中は無効化する。

## 描画画面での選択と移動

Scene Toolsパネルは、既存の描画画面に選択枠とワールドXYZ軸の移動ギズモを重ねる。モデルをクリックして選択し、何もない場所をクリックすると解除する。UIパネル上のクリックはシーンの選択に使わない。無効なモデルやMesh Rendererを外したモデルはクリック選択の対象外だが、Hierarchyから選択してTransformを編集できる。

モデル読込時に全頂点からローカルAABBを求める。GameSceneはRootNode行列とTransformを描画と同じ順序で適用し、ワールドAABBをEditorへ公開する。クリック位置から近・遠クリップ面の間にレイを作り、交差する範囲のうち手前のモデルを選ぶ。判定はAABB単位であり、穴や透明な部分を含む形状のピクセル単位の選択ではない。

選択すると黄色の範囲枠と移動軸が現れる。軸を左ドラッグして移動し、CtrlでMove Snapの間隔へ合わせる。Escでドラッグ開始位置へ戻す。移動中は履歴の確定を待ち、マウスを離した時点で一件のUndo操作にする。視線に平行で画面上の長さがない軸は操作対象から外すため、その軸を動かす場合はカメラを横へ移すかInspectorを使う。

FまたはFocus Selectionで選択位置を注視する。背景上の右ドラッグでカメラを回転し、WASDとQEで移動する。Gridで原点のXZ平面の補助線を表示・非表示にできる。移動ギズモを操作中はカメラ移動と編集ショートカットを抑止する。

SceneViewMathは画面投影、レイ生成、交差判定、範囲の変換、軸上の移動量を担当し、GPUやImGuiから独立してテストする。SceneViewPanelは描画と入力だけを担当する。表示領域は現在のDX12描画に合わせた固定1280×720で、自由にリサイズする埋込みSceneビューポートは今後の範囲。

### 検証

Visual StudioのDeveloper PowerShellでリポジトリのルートから実行する。

```powershell
MSBuild tests/SceneFileTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
./generated/scene-file-tests/bin/SceneFileTests.exe
```

追加・削除・複製、IDの維持と採番、モデル種類・個数の保存・復元、空のシーン、旧形式の移行、不正ファイルでのデータ保持、Undo/Redo、再生前データの復元、Pause/Stepを検証する。SceneViewMathでは画面投影とレイの一致、描画奥行の制限、平行なレイや平面モデルとの交差、負の拡縮・回転後の範囲、軸ドラッグの計算を検証する。
