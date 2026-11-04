#pragma once

#include "IWorldObjects.h"
#include <array>
#include <set>
#include <utility>
#include <unordered_set>

// 描画と判定が共有するワールド形状。Boxはせん断を含む平行六面体として扱う。
struct WorldCollider {
	WorldObjectId identifier = 0;

	ColliderShape shape = ColliderShape::Box;

	Vector3 center{};

	std::array<Vector3,3> halfEdges{};

	float radius = 0;

	AABB bounds{}; // 候補の絞り込みだけに使用し、最後は実形状で判定する。
};

struct CollisionEvent {
	WorldObjectId first = 0;
	WorldObjectId second = 0;

	bool entering = false; // trueは接触開始、falseは接触終了。Stayは通知しない。
};

// 法線はfirstからsecondへの向き。押し戻すときfirstを逆向き、secondを同じ向きへ動かす。
struct CollisionContact {
	Vector3 normal = {0,1,0};

	float penetration = 0;
};

// GPUやログに依存しない接触検出。GameSceneがゲーム更新時だけ呼び出す。
class WorldCollision {
public:

	//=============================================================================================//
	// 初期化・終了処理

	void Initialize(); // Play/Stopで接触履歴を消し、前の再生の通知を持ち越さない。

	void Finalize();

	//=============================================================================================//
	// 更新処理

	void Update(IWorldObjects& scene);

	//=============================================================================================//
	// Getter / 接触通知・形状

	static constexpr size_t kMaxColliders = kMaxWorldColliders;

	const std::vector<CollisionEvent>& GetEvents() const { return events_; }

	size_t GetContactCount() const { return contacts_.size(); }

	bool IsTouching(WorldObjectId identifier) const { return touching_.contains(identifier); }

	static bool BuildWorld(const ColliderSettings& settings, const Matrix4x4& world, WorldCollider& result);

	static bool Intersects(const WorldCollider& first, const WorldCollider& second);

	static bool GetContact(const WorldCollider& first, const WorldCollider& second, CollisionContact& contact);

	static Vector3 GetCorner(const WorldCollider& box, size_t cornerIndex);

private:

	//=============================================================================================//
	// 保存対象外の接触履歴

	std::set<std::pair<WorldObjectId, WorldObjectId>> contacts_; // IDを昇順に並べ、順番で別接触にしない。

	std::vector<CollisionEvent> events_; // 一更新の開始・終了通知だけを所有する。

	std::unordered_set<WorldObjectId> touching_; // SceneViewの接触色を一定時間で確認する。
};
