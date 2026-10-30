#include "Particle.h"

// C++
#include <cassert>
#include <cmath>

// 自作
#include "ParticleRenderer.h"
#include "Mesh.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "ResourceManager.h"
#include "TextureManager.h"
#include "MathUtility.h"
#include "Random.h"

//=================================================================================================//
// 初期化処理

void Particle::Initialize(ParticleRenderer* renderer) {

	assert(renderer);

	renderer_ = renderer;

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
	descriptorAllocated_ = true;

	srvManager->CreateSRVforStructuredBuffer(
		instancingSrvIndex_,
		instancingResource_.Get(),
		kNumMaxInstance,
		sizeof(ParticleForGPU));
}

//=================================================================================================//
// 更新処理

void Particle::Update(const Matrix4x4& viewProjectionMatrix, const Matrix4x4& cameraMatrix, bool simulate, bool emit) {
	// 一時停止中は発生・移動・経過時間を止め、カメラに合わせた描画データだけを更新する。

	// 頻度による発生
	if (simulate && emit) {
		emitter_.frequencyTime += kDeltaTime;
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

	if (useBillboard_) {
		billboardMatrix = cameraMatrix;
		billboardMatrix.m[3][0] = 0.f;
		billboardMatrix.m[3][1] = 0.f;
		billboardMatrix.m[3][2] = 0.f;
	}

	numInstance_ = 0;

	for (std::list<ParticleData>::iterator particleIterator = particles_.begin(); particleIterator != particles_.end();) {

		if (particleIterator->lifeTime <= particleIterator->currentTime) {

			particleIterator = particles_.erase(particleIterator);

			continue;
		}

		if (simulate) {
			if (useAccelerationField_) {

				particleIterator->velocity = MathUtility::Add(particleIterator->velocity, MathUtility::Multiply(kDeltaTime, accelerationField_.acceleration));
			}

			particleIterator->transform.translate = MathUtility::Add(particleIterator->transform.translate, MathUtility::Multiply(kDeltaTime, particleIterator->velocity));

			particleIterator->currentTime += kDeltaTime;
		}

		if (numInstance_ < kNumMaxInstance) {

			Matrix4x4 scaleMatrix = MathUtility::MakeScaleMatrix(particleIterator->transform.scale);

			Matrix4x4 translateMatrix = MathUtility::MakeTranslateMatrix(particleIterator->transform.translate);

			Matrix4x4 worldMatrix = MathUtility::Multiply(MathUtility::Multiply(scaleMatrix, billboardMatrix), translateMatrix);

			instancingData_[numInstance_].WVP = MathUtility::Multiply(worldMatrix, viewProjectionMatrix);

			instancingData_[numInstance_].World = worldMatrix;

			instancingData_[numInstance_].color = particleIterator->color;

			float remainingAlpha = 1.0f - particleIterator->currentTime / particleIterator->lifeTime;
			if (remainingAlpha < 0) {
				remainingAlpha = 0;
			}
			instancingData_[numInstance_].color.w = particleIterator->color.w * remainingAlpha;

			++numInstance_;
		}

		++particleIterator;
	}
}

//=================================================================================================//
// 描画処理

void Particle::Draw() {

	// 確保したResourceの数を超えて描画しない
	assert(numInstance_ <= kNumMaxInstance);

	//=============================================================================================//
	// 描画前設定

	renderer_->PreDraw(blendMode_);

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

//=================================================================================================//
// 終了処理

void Particle::Finalize() {
	if (descriptorAllocated_) {
		renderer_->GetSrvManager()->Free(instancingSrvIndex_);
		descriptorAllocated_ = false;
	}

	if (material_ != nullptr) {

		delete material_;

		material_ = nullptr;
	}
}

//=================================================================================================//
// 色設定

void Particle::SetColor(const Vector4& color) {

	material_->SetColor(color);
}

//=================================================================================================//
// Texture設定

void Particle::SetTexture(const std::string& filePath) {

	uint32_t textureIndex = TextureManager::GetInstance()->Load(filePath);

	material_->SetTextureIndex(textureIndex);
	appliedTexture_ = filePath;
}

//=================================================================================================//
// BlendMode設定

void Particle::SetBlendMode(BlendMode blendMode) {

	blendMode_ = blendMode;
}

//=================================================================================================//
// 新しいParticle生成

Particle::ParticleData Particle::MakeNewParticle(const Vector3& translate) {

	ParticleData particle{};

	particle.transform.scale = settings_.particleScale;
	particle.transform.rotate = { 0.0f,0.0f,0.0f };

	Vector3 randomTranslate = {Random::Range(-settings_.spread.x, settings_.spread.x),
		Random::Range(-settings_.spread.y, settings_.spread.y), Random::Range(-settings_.spread.z, settings_.spread.z)};

	particle.transform.translate = MathUtility::Add(translate, randomTranslate);

	particle.velocity = {Random::Range(settings_.velocityMinimum.x, settings_.velocityMaximum.x),
		Random::Range(settings_.velocityMinimum.y, settings_.velocityMaximum.y), Random::Range(settings_.velocityMinimum.z, settings_.velocityMaximum.z)};

	particle.color = settings_.color;
	if (settings_.randomColor) {
		particle.color.x *= Random::Range(0.0f, 1.0f);
		particle.color.y *= Random::Range(0.0f, 1.0f);
		particle.color.z *= Random::Range(0.0f, 1.0f);
	}

	particle.lifeTime = Random::Range(settings_.lifetimeMinimum, settings_.lifetimeMaximum);

	particle.currentTime = 0.0f;

	return particle;
}

//=================================================================================================//
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

// 描画用リソースを残したまま、実行中の粒子と発生タイマーだけを初期化する。
void Particle::ResetSimulation() {
	particles_.clear();
	emitter_.frequencyTime = 0;
	numInstance_ = 0;
}

// 不正な編集中の値は実行へ反映せず、直前の正常な設定でプレビューを続ける。
bool Particle::ApplySettings(const ParticleSettings& settings) {
	if (!ValidateParticleSettings(settings)) {
		return false;
	}
	if (settings.texture != appliedTexture_) {
		SetTexture(settings.texture);
	}
	settings_ = settings;
	emitter_.transform.translate = settings.emitterPosition;
	emitter_.count = settings.spawnCount;
	emitter_.frequency = settings.interval;
	accelerationField_.acceleration = settings.acceleration;
	useAccelerationField_ = settings.accelerationEnabled;
	useBillboard_ = settings.billboard;
	blendMode_ = settings.blendMode;
	material_->SetColor({1,1,1,1});
	return true;
}

// 設定のコピーを作らず、シーン側で解決した発生位置だけを反映する。
void Particle::SetEmitterPosition(const Vector3& position) {
	if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
		return;
	}
	emitter_.transform.translate = position;
}
