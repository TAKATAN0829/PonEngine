# Editorの構成

## 依存と責務

```text
Engine → Editor → HierarchyPanel / InspectorPanel → ImGui
  ↓
SceneManager → ModelCollection ← GameScene
                   ↑                 ↓
                SceneFile       ModelRenderer
```

- `ModelCollection`: シーン内のモデル一覧と、モデルごとの編集対象データを持つ。
- `Editor`: `ModelCollection`を受け取り、選択状態、一覧表示、Inspectorでの編集、シーンの保存・読み込みを扱う。
- `HierarchyPanel`: モデル一覧を表示して選択する。
- `InspectorPanel`: 選択中のモデルのTransformや描画設定を編集する。
- `SceneFile`: `ModelCollection`の保存と読み込みを行う。EditorやImGuiには依存しない。
- `ModelRenderer`: `ModelCollection`のモデルIDと使用ファイルに合わせて描画用`Object3d`を管理する。
- `GameScene`: シーン更新時にRendererをCollectionへ同期し、モデルの値を描画へ反映する。

Editorは`GameScene`を直接参照せず、Engineから渡された`ModelCollection`を編集する。モデルの選択状態はIDで保持し、各フレームでCollectionから選択中のモデルを検索する。

## モデルと描画物の更新

`ModelCollection`がモデル一覧を所有する。`ModelRenderer`はIDを使って対応する描画物を管理する。Collectionに追加されたモデルは描画物を作成し、削除されたモデルは描画物も削除する。同じIDのモデルでモデルファイルまたはテクスチャファイルが変わった場合は、描画物を作り直す。

`GameScene::UpdateObject()`では、モデル値を描画へ反映する前に`ModelRenderer::Refresh()`を呼ぶ。このため、SceneFileの読み込みでモデル一覧が置き換わった場合も、次の更新で描画物が一覧に揃う。

モデルファイル由来の頂点データ・ルート変換・既定のテクスチャ番号は`Model`で共有する。色・ライティング・光沢・UV変換・描画時のテクスチャ番号を持つ`Material`は`Object3d`ごとに所有し、描画時にその個体のMaterialを使う。同じモデルファイルを複数配置しても、見た目の設定を互いに上書きしない。

## シーンの保存・読み込み

EditorのSceneパネルからファイル名を指定し、Save SceneまたはLoad Sceneを実行する。初期ファイル名は`scene.txt`。SceneFileにはシーン名`GameScene`と`ModelCollection`を渡す。

モデルが0個の一覧も保存できる。保存・読み込みを無効にするのは、ModelCollection自体が渡されていない場合だけとする。

保存ファイルは日本語の見出し付きテキスト。「シーン名：」「モデル数：」のあとに、各モデルのID・名前・モデルファイル・テクスチャファイル・位置・回転・拡縮・色・ライティング・光沢・ルートノード変換を記録する。位置などは「位置：（10, 0, 5）」の形で書き、ライティングなどの設定は「有効」「無効」で表す。名前やパスの空白はそのまま保存でき、浮動小数点数は`float`で読み戻せる精度で書き込む。

読み込みではモデルを一時的な一覧へ読み込み、IDが0、IDが重複、または途中で読み込みに失敗した場合はエラーを返す。全モデルを読み込めた場合だけ`ModelCollection::ReplaceModels()`を呼ぶため、途中の失敗では現在の一覧を置き換えない。

## 現在の範囲

保存・読み込みはモデル一覧と各モデルの値を対象にする。ライト、パーティクル、カメラの値は保存しない。Undo/Redoやシーン切り替えは未実装。
