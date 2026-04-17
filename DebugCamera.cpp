#include "DebugCamera.h"
#include <dinput.h>

extern BYTE key[256];
extern BYTE keyPre[256];
bool IsPressKey(uint8_t keyCode);
bool IsTriggerKey(uint8_t keyCode);

//====================================
// 初期化処理
//====================================
void DebugCamera::Initialize(IDirectInputDevice8* mouse) {
	mouseDevice_ = mouse;
	// 累積回転行列を単位行列で初期化
	matRot_ = MyMath::Identity();
}


//====================================
// 更新処理
//====================================
void DebugCamera::Update() {

	const float moveSpeed = moveSpeed_;
	const float rotateSpeed = rotateSpeed_;

	//  --- 入力によるカメラ移動・回転 ---
	Vector3 move = { 0.0f, 0.0f, 0.0f };
	// 前後移動
	if (IsPressKey(DIK_W)) { move.z += moveSpeed; }
	if (IsPressKey(DIK_S)) { move.z -= moveSpeed; }
	// 左右移動
	if (IsPressKey(DIK_A)) { move.x -= moveSpeed; }
	if (IsPressKey(DIK_D)) { move.x += moveSpeed; }
	// 上下移動
	if (IsPressKey(DIK_UP)) { move.y += moveSpeed; }
	if (IsPressKey(DIK_DOWN)) { move.y -= moveSpeed; }

	// 移動量を現在の回転行列(matRot_)で変換して平行移動量を足す
	move = MyMath::Transform(move, matRot_);
	pivot_ = MyMath::Add(pivot_, move);

	// 回転 pivot_
	DIMOUSESTATE mouseState{};
	if (mouseDevice_) {
		mouseDevice_->GetDeviceState(sizeof(mouseState), &mouseState);
	}

	// 右クリックドラッグ中
	if (mouseState.rgbButtons[0]) {
		// 今回の回転角度を計算
		float rotX = mouseState.lY * rotateSpeed; // マウス上下移動でX軸回転
		float rotY = mouseState.lX * rotateSpeed; // マウス左右移動でY軸回転

		// 追加回転分の回転行列を生成
		Matrix4x4 matRotDelta = MyMath::Identity();
		matRotDelta = MyMath::Multiply(MyMath::MakeRotateXMatrix(rotX), MyMath::MakeRotateYMatrix(rotY));

		// 累積の回転行列を合成 ( matRot_ = matRotDelta * matRot_ )
		matRot_ = MyMath::Multiply(matRotDelta, matRot_);
	}


	// --- ビュー行列の更新 ---
	// カメラまでの距離
	Matrix4x4 distanceMatrix = MyMath::MakeTranslateMatrix({ 0.0f, 0.0f, -distance_ });
	// ピボット位置
	Matrix4x4 pivotMatrix = MyMath::MakeTranslateMatrix(pivot_);
	// 回転行列と平行移動行列からワールド行列を計算する
	Matrix4x4 worldMatrix = MyMath::Multiply(distanceMatrix, MyMath::Multiply(matRot_, pivotMatrix));
	// ワールド行列の逆行列をビュー行列に代入する
	viewMatrix_ = MyMath::Inverse(worldMatrix);
}
