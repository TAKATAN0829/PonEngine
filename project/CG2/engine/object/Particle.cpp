#include "Particle.h"

// C++
#include <cassert>

// 自作
#include "ParticleRenderer.h"
#include "DirectXCommon.h"
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

	DescriptorHeapManager* srvHeap = dxCommon->GetSrvHeap();

	//=============================================================================================//
	// Mesh生成

	mesh_ = new Mesh();

	mesh_->Initialize(dxCommon, MeshType::kPlane);

	//=============================================================================================//
	// Material生成

	material_ = new Material();

	material_->Initialize(dxCommon);

	//=============================================================================================//
	// Instancing用のTransformationMatrixリソースを作る

	instancingResource_ = ResourceManager::CreateBufferResource(
		device,
		sizeof(ParticleForGPU) * kNumMaxInstance,
		"instancingResource");

	//=============================================================================================//
	// 書き込むためのアドレスを取得

	instancingResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&instancingData_));

	// 単位行列を書き込んでおく
	for (uint32_t index = 0; index < kNumMaxInstance; ++index) {

		instancingData_[index].WVP = MathUtility::MakeIdentity4x4();

		instancingData_[index].World = MathUtility::MakeIdentity4x4();
	}

	//=============================================================================================//
	// SRV生成

	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};

	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = kNumMaxInstance;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);

	uint32_t instancingSrvIndex = srvHeap->Allocate();

	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU = srvHeap->GetCPUDescriptorHandle(instancingSrvIndex);

	instancingSrvHandleGPU_ = srvHeap->GetGPUDescriptorHandle(instancingSrvIndex);

	device->CreateShaderResourceView(
		instancingResource_.Get(),
		&instancingSrvDesc,
		instancingSrvHandleCPU);
}

//=================================================================================================//
// 更新処理

void Particle::Update(const Matrix4x4& viewProjectionMatrix, const Matrix4x4& cameraMatrix) {

	// 頻度による発生
	emitter_.frequencyTime += kDeltaTime;

	if (emitter_.frequency <= emitter_.frequencyTime) {
		particles_.splice(particles_.end(), Emit());
		emitter_.frequencyTime -= emitter_.frequency;
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

		if (useAccelerationField_ && MathUtility::IsCollision(accelerationField_.area, particleIterator->transform.translate)) {

			particleIterator->velocity = MathUtility::Add(
				particleIterator->velocity,
				MathUtility::Multiply(kDeltaTime, accelerationField_.acceleration));
		}

		particleIterator->transform.translate = MathUtility::Add(
			particleIterator->transform.translate,
			MathUtility::Multiply(kDeltaTime, particleIterator->velocity));

		particleIterator->currentTime += kDeltaTime;

		if (numInstance_ < kNumMaxInstance) {

			Matrix4x4 scaleMatrix = MathUtility::MakeScaleMatrix(particleIterator->transform.scale);

			Matrix4x4 translateMatrix = MathUtility::MakeTranslateMatrix(particleIterator->transform.translate);

			Matrix4x4 worldMatrix = MathUtility::Multiply(
				MathUtility::Multiply(scaleMatrix, billboardMatrix),
				translateMatrix);

			instancingData_[numInstance_].WVP = MathUtility::Multiply(worldMatrix, viewProjectionMatrix);

			instancingData_[numInstance_].World = worldMatrix;

			instancingData_[numInstance_].color = particleIterator->color;

			instancingData_[numInstance_].color.w = 1.0f - (particleIterator->currentTime / particleIterator->lifeTime);

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

	ID3D12GraphicsCommandList* commandList = renderer_->GetDxCommon()->GetCommandList();

	//=============================================================================================//
	// Material・Texture設定

	material_->BindWithoutLight();

	//=============================================================================================//
	// Instancing用のStructuredBufferのSRVを設定

	commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU_);

	//=============================================================================================//
	// インスタンス数だけ描画

	mesh_->Draw(numInstance_);
}

//=================================================================================================//
// 終了処理

void Particle::Finalize() {

	if (mesh_ != nullptr) {

		delete mesh_;

		mesh_ = nullptr;
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

	particle.transform.scale = { 1.0f,1.0f,1.0f };
	particle.transform.rotate = { 0.0f,0.0f,0.0f };

	Vector3 randomTranslate = { Random::Range(-1.0f,1.0f),Random::Range(-1.0f,1.0f),Random::Range(-1.0f,1.0f) };

	particle.transform.translate = MathUtility::Add(translate, randomTranslate);

	particle.velocity = { Random::Range(-1.0f,1.0f),Random::Range(-1.0f,1.0f),Random::Range(-1.0f,1.0f) };

	particle.color = { Random::Range(0.0f,1.0f),Random::Range(0.0f,1.0f),Random::Range(0.0f,1.0f),1.0f };

	particle.lifeTime = Random::Range(1.0f, 3.0f);

	particle.currentTime = 0.0f;

	return particle;
}

//=================================================================================================//
// Emitterから指定個数のParticleを作る

std::list<Particle::ParticleData> Particle::Emit() {

	std::list<ParticleData> particles;

	for (uint32_t count = 0; count < emitter_.count; ++count) {

		particles.push_back(MakeNewParticle(emitter_.transform.translate));
	}

	return particles;
}