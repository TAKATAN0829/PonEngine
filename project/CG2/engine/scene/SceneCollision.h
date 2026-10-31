#pragma once
#include "IEditableScene.h"
#include <array>
#include <set>
#include <utility>
#include <unordered_set>

// 描画と判定が共有するワールド形状。Boxはせん断を含む平行六面体として扱う。
struct WorldCollider {
	SceneObjectId identifier = 0;
	ColliderShape shape = ColliderShape::Box;
	Vector3 center{};
	std::array<Vector3,3> halfEdges{};
	float radius = 0;
	AABB bounds{}; // 候補の絞り込みだけに使用し、最後は実形状で判定する。
};

struct SceneContactEvent {
	SceneObjectId first = 0;
	SceneObjectId second = 0;
	bool entering = false; // trueは接触開始、falseは接触終了。Stayは通知しない。
};

// 法線はfirstからsecondへの向き。押し戻すときfirstを逆向き、secondを同じ向きへ動かす。
struct SceneCollisionContact {
	Vector3 normal = {0,1,0};
	float penetration = 0;
};

// GPUやログに依存しない接触検出。GameSceneがゲーム更新時だけ呼び出す。
class SceneCollision {
public:
	//=============================================================================================//
	// 初期化・終了処理

	void Initialize(); // Play/Stopで接触履歴を消し、前の再生の通知を持ち越さない。
	void Finalize();

	//=============================================================================================//
	// 更新処理

	void Update(IEditableScene& scene);

	//=============================================================================================//
	// Getter / 接触通知・形状

	static constexpr size_t kMaxColliders = kMaxSceneColliders;
	const std::vector<SceneContactEvent>& GetEvents() const { return events_; }
	size_t GetContactCount() const { return contacts_.size(); }
	bool IsTouching(SceneObjectId identifier) const { return touching_.contains(identifier); }
	static bool BuildWorld(const SceneColliderSettings& settings, const Matrix4x4& world, WorldCollider& result);
	static bool Intersects(const WorldCollider& first, const WorldCollider& second);
	static bool GetContact(const WorldCollider& first, const WorldCollider& second, SceneCollisionContact& contact);
	static Vector3 GetCorner(const WorldCollider& box, size_t cornerIndex);

private:
	//=============================================================================================//
	// メンバ変数 / 保存対象外の接触履歴

	std::set<std::pair<SceneObjectId, SceneObjectId>> contacts_; // IDを昇順に並べ、順番で別接触にしない。
	std::vector<SceneContactEvent> events_; // 一更新の開始・終了通知だけを所有する。
	std::unordered_set<SceneObjectId> touching_; // SceneViewの接触色を一定時間で確認する。
};
