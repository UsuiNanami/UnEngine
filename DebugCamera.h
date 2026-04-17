#pragma once
#include "MyMath.h"
#include <dinput.h>

class DebugCamera{

public:
	void Initialize(IDirectInputDevice8* mouse);
	void Update();

	const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }

private:
	// 累積回転行列
	Matrix4x4 matRot_ = MyMath::Identity();
	// ピボット座標
	Vector3 pivot_ = { 0.0f,0.0f,0.0f };
	// カメラとの距離
	float distance_ = 20.0f;
	// ローカル座標
	Vector3 translation_ = { 0,0,-20 };
	// ビュー行列
	Matrix4x4 viewMatrix_;
	// 射影行列
	Matrix4x4 projectionMatrix_;

	// 速度設定
	float moveSpeed_ = 0.3f;
	float rotateSpeed_ = 0.02f;

	IDirectInputDevice8* mouseDevice_ = nullptr;
};


