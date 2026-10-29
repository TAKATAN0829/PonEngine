# Editorの最初の実装

Debug / Development で起動し、Hierarchy の Sphere を選択すると、Inspector で位置・回転・拡縮・色・ライティング・光沢を編集できる。Clear Selection で選択を解除する。パネルは既存のDockSpaceにドッキングできる。

## 依存と責務

```text
Engine → Editor → HierarchyPanel / InspectorPanel → ImGui
            ↓
       IEditableScene ← GameScene
Engine → SceneManager → IScene
```

- ImGuiSystem: ImGuiの初期化、フレーム開始、DX12への描画、終了。
- Editor: 選択状態とパネル呼び出しの調整。Engineが値として所有する。
- HierarchyPanel: 編集対象一覧の表示と選択。
- InspectorPanel: 渡されたデータの表示と編集。
- IEditableScene: ImGuiを含まない編集対象の列挙契約。
- GameScene: モデルと編集する値を所有し、契約を通して公開する。

GameSceneはEditorの具体クラスを知らない。EditorもGameSceneを知らず、IEditableSceneを受け取る。この引数渡しが依存性の注入に当たる。最初は複雑なイベント機構や汎用リフレクションを導入せず、小さなインターフェースで境界を設けている。

## 所有権と更新順

EditableObjectView内のポインタは借用であり、Editor側から破棄しない。取得したフレーム内だけ利用し、保持しない。選択にはオブジェクトIDを保持し、毎フレーム検索して対象が存在しなければ解除する。IDはシーン内で一意かつ有効期間中は不変とし、削除後に別の対象へ再利用しない。

ImGuiフレーム開始 → Editorで値を編集 → GameSceneの更新 → シーン描画 → ImGui描画、の順に処理し、モデルへの反映が翌フレームへ遅れないようにしている。

## 現在の範囲と次の課題

編集対象は現在のSphere一体。ライト、パーティクル、カメラの既存デバッグUIはそのまま使える。Undo/Redo、追加・削除、Sceneビューはまだ実装していない。

シーン切替はまだ存在しないため、現在はシーンインターフェースのアドレス変更で選択を解除する。シーンの再生成・切替を導入する際は、同じアドレスが再利用されても識別できるシーン世代番号や明示的な切替通知を追加する。

Undo/Redoを導入する際は、直接編集を「対象ID・変更前・変更後」を持つCommandへ移す。ドラッグ開始から終了までを一つの操作にまとめる。再生・停止では編集データと実行時データの分離も必要になる。

## シーンの保存・読込

SceneパネルのSave Sceneで `project/CG2/resources/scenes/GameScene.ponscene` に保存し、Load Sceneで復元する。実行時のパスは作業ディレクトリ基準の `resources/scenes/GameScene.ponscene`。既存のテクスチャと同様、作業ディレクトリは `project/CG2` とする。

保存済みファイルがあればGameSceneの初期化時に自動読込する。Releaseでも読込できる。ファイルがない場合は既定値で開始し、壊れたファイルは値を反映せずログにエラーを出す。保存はボタンを押したときのみ行う。

SceneFileはEditorに依存せず、IEditableSceneを引数に受け取る。バージョン付きのテキスト形式 `PON_SCENE 1` で、オブジェクトID、存在するプロパティのビットマスク、Transform・色・光沢・ライティングを記録する。浮動小数点は往復可能な桁数を使う。

読込ではバージョン、件数、IDの一致・重複、プロパティの一致、有限値、光沢の範囲、末尾の余分なデータを確認してから一括反映する。保存は隣接する一時ファイルに書き終えてからWindowsのファイル置換APIで置き換える。

現段階では既存オブジェクトの値の復元を扱い、オブジェクトの新規生成や削除、モデル・テクスチャの変更は扱わない。ライト、パーティクル、カメラのデバッグUIの値は保存対象外。

### 検証

Visual StudioのDeveloper PowerShellでリポジトリのルートから実行する。

```powershell
MSBuild tests/SceneFileTests.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
./generated/scene-file-tests/bin/SceneFileTests.exe
```

複数オブジェクトの保存・復元、既存ファイルの置換、不正バージョン、途中で切れたファイル、重複ID、未知ID、余分な末尾データ、ファイル未存在、保存失敗時の既存ファイル保持を検証する。
