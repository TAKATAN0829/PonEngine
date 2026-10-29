#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// C++
#include <cstdint>
#include <string>
#include <list>

// 自作
#include "EngineStructs.h"
#include "Material.h"
#include "BlendMode.h"

class ParticleRenderer;

class Particle {
public:

	//=============================================================================================//
	// 最大インスタンス数

	static const uint32_t kNumMaxInstance = 100;

	//=============================================================================================//
	// 初期化処理

	void Initialize (ParticleRenderer* renderer);

	//=============================================================================================//
	// 更新処理

	void Update (const Matrix4x4& viewProjectionMatrix,const Matrix4x4& cameraMatrix);

	//=============================================================================================//
	// 描画処理

	void Draw ();

	//=============================================================================================//
	// 終了処理

	void Finalize ();

	//=============================================================================================//
	// Particle追加

	void AddParticles() { particles_.splice(particles_.end(), Emit()); }

	//=============================================================================================//
	// EmitterのTransform

	TransformData& GetEmitterTransform() { return emitter_.transform; }	

	//=============================================================================================//
	// Setter

	// 色
	void SetColor (const Vector4& color);
	// テクスチャ
	void SetTexture (const std::string& filePath);
	// ブレンドモード
	void SetBlendMode (BlendMode blendMode);
	// ビルボード
	void SetUseBillboard(bool useBillboard) { useBillboard_ = useBillboard; }
	// 加速度Field
	void SetUseAccelerationField(bool useAccelerationField) { useAccelerationField_ = useAccelerationField; }

private:
	//=============================================================================================//
	// Particle1粒分のデータ
	
	struct ParticleData {
		TransformData transform;
		Vector3 velocity;
		Vector4 color;
		float lifeTime;
		float currentTime;
	};

	//=============================================================================================//
	// Particle用インスタンシングデータ（GPU転送用）

	struct ParticleForGPU {
		Matrix4x4 WVP;
		Matrix4x4 World;
		Vector4 color;
	};

	//=============================================================================================//
	// 発生源

	struct Emitter {
		TransformData transform;
		uint32_t count;
		float frequency;
		float frequencyTime;
	};

	//=============================================================================================//
	// 加速度Field

	struct AccelerationField {
		Vector3 acceleration;
		AABB area;
	};

	//=============================================================================================//
	// 新しいParticle生成

	ParticleData MakeNewParticle(const Vector3& translate);

	//=============================================================================================//
	// Emitterから指定個数のParticleを作る

	std::list<ParticleData> Emit();

	//=============================================================================================//
	// Δt

	static constexpr float kDeltaTime = 1.0f / 60.0f;

	//=============================================================================================//
	// Renderer

	ParticleRenderer* renderer_ = nullptr;

	//=============================================================================================//
	// Material

	Material* material_ = nullptr;

	//=============================================================================================//
	// Instancing用のTransformationMatrixリソース

	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_ = nullptr;

	//=============================================================================================//
	// 書き込むためのアドレス

	ParticleForGPU* instancingData_ = nullptr;

	//=============================================================================================//
	// Instancing用SRV番号

	uint32_t instancingSrvIndex_ = 0;

	//=============================================================================================//
	// Particle

	std::list<ParticleData> particles_;

	//=============================================================================================//
	// Emitter

	Emitter emitter_ = {
		{ { 1.0f,1.0f,1.0f },{ 0.0f,0.0f,0.0f },{ 0.0f,0.0f,0.0f } },
		3,
		0.5f,
		0.0f
	};

	//=============================================================================================//
	// 加速度Field

	AccelerationField accelerationField_ = {
		{ 15.0f,0.0f,0.0f },
		{ { -1.0f,-1.0f,-1.0f },{ 1.0f,1.0f,1.0f } }
	};

	bool useAccelerationField_ = false;

	//=============================================================================================//
	// 描画するインスタンス数

	uint32_t numInstance_ = 0;

	//=============================================================================================//
	// BlendMode

	BlendMode blendMode_ = kBlendModeNormal;

	//=============================================================================================//
	// ビルボード

	bool useBillboard_ = true;
};
