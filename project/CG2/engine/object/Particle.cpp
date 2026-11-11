#include "Particle.h"

// C++
#include <cassert>
#include <cmath>
#include <algorithm>

// 自作
#include "ParticleRenderer.h"
#include "Mesh.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "ResourceManager.h"
#include "TextureManager.h"
#include "MathUtility.h"
#include "Random.h"

//=============================================================================================//
// 初期化処理

void Particle::Initialize(ParticleRenderer* renderer) {

	assert(renderer);

	renderer_ = renderer;
	emitterWorld_ = MathUtility::MakeIdentity4x4();

	DirectXCommon* dxCommon = renderer_->GetDxCommon();

	ID3D12Device* device = dxCommon->GetDevice();

	SrvManager* srvManager = renderer_->GetSrvManager();

	//=============================================================================================//
	// Material生成

	material_ = new Material();

	material_->Initialize(dxCommon, srvManager);

	//=============================================================================================//
	// Instancing用のTransformationMatrixリソースを作る

	instancingResource_ = ResourceManager::CreateBufferResource(device, sizeof(ParticleForGPU) * kNumMaxInstance, "instancingResource");

	//=============================================================================================//
	// 書き込むためのアドレスを取得

	instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));

	// 単位行列を書き込んでおく
	for (uint32_t index = 0; index < kNumMaxInstance; ++index) {

		instancingData_[index].WVP = MathUtility::MakeIdentity4x4();

		instancingData_[index].World = MathUtility::MakeIdentity4x4();
	}

	//=============================================================================================//
	// SRV生成

	instancingSrvIndex_ = srvManager->Allocate();

	isDescriptorAllocated_ = true;

	srvManager->CreateSRVforStructuredBuffer(
		instancingSrvIndex_,
		instancingResource_.Get(),
		kNumMaxInstance,
		sizeof(ParticleForGPU));
}

//=============================================================================================//
// 更新処理

void Particle::Update(const Matrix4x4& viewProjectionMatrix, const Matrix4x4& cameraMatrix, bool shouldSimulate, bool shouldEmit) {
	inverseViewProjection_ = MathUtility::Inverse(viewProjectionMatrix);

	// 一時停止中は発生・移動・経過時間を止め、カメラに合わせた描画データだけを更新する。

	// 頻度による発生
	if (shouldSimulate && shouldEmit) {
		emitter_.frequencyTime += kDeltaTime;

		// 経過時間に含まれる発生間隔の回数だけ粒子を出し、間隔に満たない時間は持ち越す。
		while (emitter_.frequency <= emitter_.frequencyTime) {
			if (particles_.size() >= kNumMaxInstance) {
				emitter_.frequencyTime = std::fmod(emitter_.frequencyTime, emitter_.frequency);

				break;
			}

			particles_.splice(particles_.end(), Emit());

			emitter_.frequencyTime -= emitter_.frequency;
		}
	}


	// ビルボード行列
	Matrix4x4 billboardMatrix = MathUtility::MakeIdentity4x4();

	if (isBillboardEnabled_) {
		billboardMatrix = cameraMatrix;

		billboardMatrix.m[3][0] = 0.f;

		billboardMatrix.m[3][1] = 0.f;

		billboardMatrix.m[3][2] = 0.f;
	}

	numInstance_ = 0;

	for (std::list<ParticleData>::iterator particleIterator = particles_.begin(); particleIterator != particles_.end();) {

		// 寿命に達した粒子は描画データを作る前に除く。
		if (particleIterator->lifeTime <= particleIterator->currentTime) {

			particleIterator = particles_.erase(particleIterator);

			continue;
		}

		if (shouldSimulate) {
			if (isAccelerationFieldEnabled_) {

				particleIterator->velocity = MathUtility::Add(particleIterator->velocity, MathUtility::Multiply(kDeltaTime, acceleration_));
			}

			particleIterator->transform.translate = MathUtility::Add(particleIterator->transform.translate, MathUtility::Multiply(kDeltaTime, particleIterator->velocity));

			particleIterator->currentTime += kDeltaTime;
			// 地面を下回った位置を戻し、下向き速度だけを反射させて貫通を防ぐ。
			if (!particleIterator->isTrail && settings_.isGroundCollisionEnabled && particleIterator->transform.translate.y < settings_.collisionHeight) {
				particleIterator->transform.translate.y = settings_.collisionHeight;
				if (particleIterator->velocity.y < 0) particleIterator->velocity.y = -particleIterator->velocity.y * settings_.collisionRestitution;
			}
			if (!particleIterator->isTrail && settings_.isTrailEnabled) {
				// 粒子ごとに経過間隔を持ち、最大数に達するまで通過位置へ残像を置く。
				particleIterator->trailTimer += kDeltaTime;
				while (particleIterator->trailTimer >= settings_.trailInterval && particles_.size() < kNumMaxInstance) {
					particleIterator->trailTimer -= settings_.trailInterval;
					ParticleData trail{};
					trail.transform = particleIterator->transform;
					trail.transform.scale = {settings_.trailWidth, settings_.trailWidth, settings_.trailWidth};
					trail.velocity = {};
					trail.color = particleIterator->color;
					trail.lifeTime = settings_.trailLifetime;
					trail.currentTime = 0;
					trail.isTrail = true;
					trail.trailTimer = 0;
					particles_.push_back(trail);
				}
			}
		}

		if (numInstance_ < kNumMaxInstance) {

			const ParticleAppearance appearance = EvaluateParticleAppearance(settings_,particleIterator->color,
				particleIterator->currentTime/particleIterator->lifeTime);

			Matrix4x4 scaleMatrix = MathUtility::MakeScaleMatrix(MathUtility::Multiply(appearance.size,particleIterator->transform.scale));

			Matrix4x4 translateMatrix = MathUtility::MakeTranslateMatrix(particleIterator->transform.translate);

			Matrix4x4 worldMatrix = MathUtility::Multiply(MathUtility::Multiply(scaleMatrix, billboardMatrix), translateMatrix);

			instancingData_[numInstance_].WVP = MathUtility::Multiply(worldMatrix, viewProjectionMatrix);

			instancingData_[numInstance_].World = worldMatrix;

			instancingData_[numInstance_].color = appearance.color;

			++numInstance_;
		}

		++particleIterator;
	}
}

//=============================================================================================//
// 描画処理

void Particle::Draw() {

	// 確保したResourceの数を超えて描画しない
	assert(numInstance_ <= kNumMaxInstance);

	//=============================================================================================//
	// 描画前設定

	float softDistance = 0;

	if (settings_.areSoftParticlesEnabled) {
		softDistance = settings_.softDistance;
	}

	renderer_->PreDraw(blendMode_,isBrightnessAlphaEnabled_,inverseViewProjection_,softDistance);

	//=============================================================================================//
	// Material・Texture設定

	material_->Bind();

	//=============================================================================================//
	// Instancing用のStructuredBufferのSRVを設定

	renderer_->GetSrvManager()->SetGraphicsRootDescriptorTable(1, instancingSrvIndex_);

	//=============================================================================================//
	// インスタンス数だけ描画

	renderer_->GetMesh()->Draw(numInstance_);
}

//=============================================================================================//
// 終了処理

void Particle::Finalize() {
	// 粒子が使用したMaterialとSRVを解放する。
	if (isDescriptorAllocated_) {
		renderer_->GetSrvManager()->Free(instancingSrvIndex_);

		isDescriptorAllocated_ = false;
	}

	if (material_ != nullptr) {

		delete material_;

		material_ = nullptr;
	}
}

//=============================================================================================//
// Particle追加

// 不正な編集中の値は実行へ反映せず、直前の正常な設定でプレビューを続ける。
void Particle::ApplySettings(const ParticleSettings& settings) {
	// 設定値を検証してから、Particleの実行設定へ反映する。
	if (!AreParticleSettingsValid(settings)) {
		return;
	}

	if (settings.texture != appliedTexture_) {
		std::string textureError;

		if (!TextureManager::GetInstance()->TryLoad(settings.texture, textureError)) {
			return;
		}

		SetTexture(settings.texture);
	}

	settings_ = settings;

	emitter_.transform.translate = settings.emitterPosition;

	emitter_.count = settings.spawnCount;

	emitter_.frequency = settings.interval;

	acceleration_ = settings.acceleration;

	isAccelerationFieldEnabled_ = settings.isAccelerationEnabled;

	isBillboardEnabled_ = settings.isBillboardEnabled;

	blendMode_ = settings.blendMode;

	material_->SetColor({1,1,1,1});
}

// 描画用リソースを残したまま、実行中の粒子と発生タイマーだけを初期化する。
void Particle::ResetSimulation() {
	// 現在の設定を残し、粒子と発生タイマーを消す。
	particles_.clear();

	emitter_.frequencyTime = 0;

	numInstance_ = 0;
}

//=============================================================================================//
// Getter / 現在の粒子数

size_t Particle::GetLiveCount() const {
	// 現在生存しているParticleの数を取得する。
	return particles_.size();
}

//=============================================================================================//
// Particle追加

void Particle::AddParticles() {
	// 発生源から生成した粒子を、生存中の一覧へ追加する。
	particles_.splice(particles_.end(), Emit());
}

//=============================================================================================//
// EmitterのTransform

// 設定のコピーを作らず、シーン側で解決した発生位置だけを反映する。
void Particle::SetEmitterPosition(const Vector3& position) {
	// 親子変換後のワールド位置を、粒子の発生源へ反映する。
	if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
		return;
	}

	emitter_.transform.translate = position;
}

void Particle::SetEmitterWorldTransform(const Matrix4x4& transform) {
	// 平行移動だけでなく回転・拡縮も保持し、新規粒子の位置と初速をローカル空間から変換する。
	emitterWorld_ = transform;
}

//=============================================================================================//
// Setter

void Particle::SetColor(const Vector4& color) {
	// 描画に使う色を更新する。

	material_->SetColor(color);
}

void Particle::SetTexture(const std::string& filePath) {
	// 画像を読み込み、透明度の扱いと参照するテクスチャを更新する。

	uint32_t textureIndex = TextureManager::GetInstance()->Load(filePath);

	material_->SetTextureIndex(textureIndex);

	appliedTexture_ = filePath;

	isBrightnessAlphaEnabled_ = !TextureManager::GetInstance()->HasTransparency(filePath);
}

void Particle::SetBlendMode(BlendMode blendMode) {
	// 描画時の合成方法を更新する。

	blendMode_ = blendMode;
}

//=============================================================================================//
// 新しいParticle生成

Particle::ParticleData Particle::MakeNewParticle(const Vector3& translate) {
	// 設定範囲から乱数を取り、粒子一つ分の初期値を作る。

	ParticleData particle{};

	particle.transform.scale = settings_.particleScale;

	particle.transform.rotate = { 0.0f,0.0f,0.0f };

	// 形状はEmitterローカル空間で生成し、後段で親子Transformを適用する。
	Vector3 randomTranslate{};
	if (settings_.emissionShape == ParticleEmissionShape::Box) {
		randomTranslate = {Random::Range(-settings_.spread.x, settings_.spread.x), Random::Range(-settings_.spread.y, settings_.spread.y), Random::Range(-settings_.spread.z, settings_.spread.z)};
	} else if (settings_.emissionShape == ParticleEmissionShape::Sphere) {
		const float z = Random::Range(-1.0f, 1.0f); const float angle = Random::Range(0.0f, 6.2831853f);
		const float radial = std::sqrt((std::max)(0.0f, 1.0f - z * z)); const float radius = settings_.shapeRadius * std::cbrt(Random::Range(0.0f, 1.0f));
		randomTranslate = {radial * std::cos(angle) * radius, z * radius, radial * std::sin(angle) * radius};
	} else if (settings_.emissionShape == ParticleEmissionShape::Cone) {
		const float height = Random::Range(0.0f, settings_.shapeRadius);
		const float radius = height * std::tan(settings_.coneAngle * 0.0174532925f) * std::sqrt(Random::Range(0.0f, 1.0f));
		const float angle = Random::Range(0.0f, 6.2831853f);
		randomTranslate = {radius * std::cos(angle), height, radius * std::sin(angle)};
	} else {
		randomTranslate = {Random::Range(-settings_.spread.x, settings_.spread.x), Random::Range(-settings_.spread.y, settings_.spread.y), Random::Range(-settings_.spread.z, settings_.spread.z)};
	}

	// 点のTransform同士の差を取り、位置に含まれた平行移動をオフセットから除く。
	const Vector3 worldOffset = MathUtility::Subtract(MathUtility::Transform(randomTranslate, emitterWorld_), MathUtility::Transform(Vector3{}, emitterWorld_));
	particle.transform.translate = MathUtility::Add(translate, worldOffset);

	// 速度も方向ベクトルとして変換し、Emitterの回転・拡縮を反映する。
	const Vector3 localVelocity = {Random::Range(settings_.velocityMinimum.x, settings_.velocityMaximum.x),
		Random::Range(settings_.velocityMinimum.y, settings_.velocityMaximum.y), Random::Range(settings_.velocityMinimum.z, settings_.velocityMaximum.z)};
	particle.velocity = MathUtility::Subtract(MathUtility::Transform(localVelocity, emitterWorld_), MathUtility::Transform(Vector3{}, emitterWorld_));

	particle.color = settings_.color;

	if (settings_.isRandomColorEnabled) {
		particle.color.x *= Random::Range(0.0f, 1.0f);

		particle.color.y *= Random::Range(0.0f, 1.0f);

		particle.color.z *= Random::Range(0.0f, 1.0f);
	}

	particle.lifeTime = Random::Range(settings_.lifetimeMinimum, settings_.lifetimeMaximum);

	particle.currentTime = 0.0f;
	particle.isTrail = false;
	particle.trailTimer = 0;

	return particle;
}

//=============================================================================================//
// Emitterから指定個数のParticleを作る

std::list<Particle::ParticleData> Particle::Emit() {

	std::list<ParticleData> particles;

	for (uint32_t count = 0; count < emitter_.count; ++count) {
		// 描画上限を超える粒子は生成せず、長い寿命や高頻度でもメモリを増やし続けない。
		if (particles_.size() + particles.size() >= kNumMaxInstance) {
			break;
		}

		particles.push_back(MakeNewParticle(emitter_.transform.translate));
	}

	return particles;
}
