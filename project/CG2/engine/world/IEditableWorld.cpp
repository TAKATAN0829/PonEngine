#include "IEditableWorld.h"

//=============================================================================================//
// Getter / Sceneカメラ・選択範囲

// 対応するカメラや形状を持たないシーンではfalseを返す。
bool IEditableWorld::GetEditorViewInfo(EditorViewInfo&) const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return false;
}

bool IEditableWorld::GetEditableObjectBounds(size_t, AABB&) const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return false;
}

// 表面スナップはメッシュを持つ対象だけを調べる。距離はワールド単位。
bool IEditableWorld::IntersectObjectSurface(size_t, const Vector3&, const Vector3&, float&) const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return false;
}

//=============================================================================================//
// 描画処理 / 任意の補助画面

void IEditableWorld::DrawEditorTools(EditorToolVisibility& visibility) {
	// 既定実装では値を変更せず、対応する派生クラスの処理に任せる。
	(void)visibility;
}

//=============================================================================================//
// Setter / Scene表示・入力

void IEditableWorld::SetEditorViewport(uint32_t width, uint32_t height) {
	// 既定実装では値を変更せず、対応する派生クラスの処理に任せる。
	(void)width;

	(void)height;
}

void IEditableWorld::SetEditorCameraInput(bool isEnabled) {
	// 既定実装では値を変更せず、対応する派生クラスの処理に任せる。
	(void)isEnabled;
}

void IEditableWorld::SetEditorGameView(bool isEnabled) {
	// 既定実装では値を変更せず、対応する派生クラスの処理に任せる。
	(void)isEnabled;
}

void IEditableWorld::FocusEditorCamera(const Vector3& position) {
	// 既定実装では値を変更せず、対応する派生クラスの処理に任せる。
	(void)position;
}

//=============================================================================================//
// Getter / Particle

ParticleSettings* IEditableWorld::GetParticleSettings() {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return nullptr;
}

size_t IEditableWorld::GetParticleCount() const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return 0;
}

//=============================================================================================//
// Setter / Particleプレビュー

void IEditableWorld::SetParticlePreview(const ParticlePreviewState& state) {
	// 既定実装では値を変更せず、対応する派生クラスの処理に任せる。
	(void)state;
}

void IEditableWorld::SetParticlePreviewViewport(uint32_t, uint32_t) {
	// Particle描画先を持たないWorldではサイズ要求を無視する。
}

uint64_t IEditableWorld::GetParticlePreviewTexture() const {
	// 専用描画先を持たないWorldでは画像なしを返す。
	return 0;
}

void IEditableWorld::SetParticlePreviewEffect(const ParticleEffectDocument& document) {
	// 既定実装では値を変更せず、対応する派生クラスの処理に任せる。
	(void)document;
}

void IEditableWorld::ResetParticlePreview() {
	// この機能に対応する派生クラスで処理を定義する。
}

void IEditableWorld::BurstParticles() {
	// この機能に対応する派生クラスで処理を定義する。
}

bool IEditableWorld::BurstParticleEffect(WorldObjectId) {
	// Particle実行を持たないWorldでは発生要求を受け付けない。
	return false;
}

//=============================================================================================//
// Asset参照の更新

// ディスク上の移動が成功した後に呼ぶ。GPUリソースの所有権は変更しない。
void IEditableWorld::RemapAssetPaths(const AssetPathChange& change) {
	// 既定実装では値を変更せず、対応する派生クラスの処理に任せる。
	(void)change;
}

// 新規作成・Sceneファイル読込後に、保存対象外の実行状態だけを初期化する。
void IEditableWorld::OnSceneContentsChanged() {
	// この機能に対応する派生クラスで処理を定義する。
}

//=============================================================================================//
// オブジェクト操作

// 3Dプレビュー用の画像ハンドルを返す。描画を持たないシーンは0を返す。
uint64_t IEditableWorld::GetModelPreview(const std::string&, std::string&) {
	// 画像を生成できない場合は0を返し、Projectパネルに代替表示を任せる。
	return 0;
}

// 実行側でモデルを準備し、失敗理由を返す。CPUだけのシーンは準備を省略する。
bool IEditableWorld::PrepareModel(const std::string&, std::string& error) {
	// CPUだけの既定実装ではGPUの読込を省略し、成功として扱う。
	error.clear();

	return true;
}

// Inspectorは描画実装に触れず、モデルの編集可能な情報だけを取得する。
std::vector<ModelMaterialInfo> IEditableWorld::GetModelMaterials(const std::string&) const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return {};
}

std::vector<ModelAnimationInfo> IEditableWorld::GetModelAnimations(const std::string&) const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return {};
}

bool IEditableWorld::PrepareTexture(const std::string&, std::string& error) {
	// CPUだけの既定実装ではGPUの読込を省略し、成功として扱う。
	error.clear();

	return true;
}

void IEditableWorld::SetAnimationPreview(WorldObjectId, bool, bool) {
	// この機能に対応する派生クラスで処理を定義する。
}

float IEditableWorld::GetAnimationTime(WorldObjectId) const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return 0;
}

bool IEditableWorld::IsAnimationPreviewPlaying(WorldObjectId) const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return false;
}
