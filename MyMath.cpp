#define _USE_MATH_DEFINES
#include "MyMath.h"
#include <assert.h>
#include <cmath>

// 正規化 
Vector3 MyMath::Normalize(const Vector3& v) {
	float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	assert(len != 0.0f);
	return { v.x / len, v.y / len, v.z / len };
}


// ============================================
// 球の表示 
// ============================================
void DrawSphere(const Sphere& sphere, const Matrix4x4& viewProjectionMatrix, const Matrix4x4& viewportMatrix, uint32_t color) {
	const uint32_t kSubdivision = 16;                                       // 球の分割数
	const float kLonEvery = 2.0f * static_cast<float>(M_PI) / kSubdivision; // 経度の1セグメントあたりの角度
	const float kLatEvery = static_cast<float>(M_PI) / kSubdivision;        // 緯度の1セグメントあたりの角度
	// 緯度の方向に分割 -π/2 ～ π/2
	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {
		float lat = -static_cast<float>(M_PI) / 2.0f + kLatEvery * static_cast<float>(latIndex);
		float nextLat = lat + kLatEvery;
		// 経度の方向に分割 0 ～ 2π
		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
			// 現在の経度 lon と、次のステップの経度 nextLon を計算
			float lon = static_cast<float>(lonIndex) * kLonEvery;
			float nextLon = lon + kLonEvery;

			// 球の表面上の点 a, b, c を計算する
			Vector3 a = {
				sphere.center.x + sphere.radius * std::cos(lat) * std::cos(lon), sphere.center.y + sphere.radius * std::sin(lat), sphere.center.z + sphere.radius * std::cos(lat) * std::sin(lon) };

			// 次の緯度・現在の経度
			Vector3 b = {
				sphere.center.x + sphere.radius * std::cos(nextLat) * std::cos(lon), sphere.center.y + sphere.radius * std::sin(nextLat),
				sphere.center.z + sphere.radius * std::cos(nextLat) * std::sin(lon) };

			// 現在の緯度・次の経度
			Vector3 c = {
				sphere.center.x + sphere.radius * std::cos(lat) * std::cos(nextLon), sphere.center.y + sphere.radius * std::sin(lat),
				sphere.center.z + sphere.radius * std::cos(lat) * std::sin(nextLon) };


			// スクリーン座標系まで変換する
			Vector3 aScreen = MyMath::Transform(MyMath::Transform(a, viewProjectionMatrix), viewportMatrix);
			Vector3 bScreen = MyMath::Transform(MyMath::Transform(b, viewProjectionMatrix), viewportMatrix);
			Vector3 cScreen = MyMath::Transform(MyMath::Transform(c, viewProjectionMatrix), viewportMatrix);

			// a から b への線（縦方向の線）
			//Novice::DrawLine(int(aScreen.x), int(aScreen.y), int(bScreen.x), int(bScreen.y), color);
			// a から c への線（横方向の線）
			//Novice::DrawLine(int(aScreen.x), int(aScreen.y), int(cScreen.x), int(cScreen.y), color);
		}
	}
}

// ============================================
// 球の頂点データの作成   
// ============================================ 
void CreateSphereVertices(const Sphere& sphere, VertexData* vertexData, uint32_t kSubdivision) {
	const float kLonEvery = 2.0f * static_cast<float>(M_PI) / static_cast<float>(kSubdivision); // 経度の1セグメントあたりの角度
	const float kLatEvery = static_cast<float>(M_PI) / static_cast<float>(kSubdivision);        // 緯度の1セグメントあたりの角度
	// 緯度の方向に分割
	for (uint32_t latIndex = 0; latIndex <= kSubdivision; ++latIndex) {
		float lat = -static_cast<float>(M_PI) / 2.0f + kLatEvery * static_cast<float>(latIndex);
		// 経度の方向に分割 0 ～ 2π
		for (uint32_t lonIndex = 0; lonIndex <= kSubdivision; ++lonIndex) {
			// 現在の経度 lon と、次のステップの経度 nextLon を計算
			float lon = static_cast<float>(lonIndex) * kLonEvery;

			// 球の表面上の点を計算する 
			Vector3 position = {
			sphere.center.x + sphere.radius * std::cos(lat) * std::cos(lon),
			sphere.center.y + sphere.radius * std::sin(lat),
			sphere.center.z + sphere.radius * std::cos(lat) * std::sin(lon)
			};

			// テクスチャのUV座標を計算
			float u = static_cast<float>(lonIndex) / static_cast<float>(kSubdivision);
			float v = 1.0f - (static_cast<float>(latIndex) / static_cast<float>(kSubdivision)); // -1で逆転させる

			// 頂点データの書き込み
			uint32_t vertexIndex = latIndex * (kSubdivision + 1) + lonIndex;

			// 頂点データの設定
			vertexData[vertexIndex].position = { 
				position.x,
				position.y,
				position.z,
				1.0f
			};

			vertexData[vertexIndex].texcoord = {
				u,
				v
			};

			vertexData[vertexIndex].normal = {
				position.x,
				position.y,
				position.z
			};
		}
	}

}



// ============================================
// 球のインデックスデータの作成
// ============================================
void CreateSphereIndices(uint32_t* indexData, uint32_t kSubdivision) {
	uint32_t index = 0;

	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {
		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {

			// 四角形の4頂点
			uint32_t leftTop = latIndex * (kSubdivision + 1) + lonIndex;
			uint32_t leftBottom = (latIndex + 1) * (kSubdivision + 1) + lonIndex;
			uint32_t rightTop = leftTop + 1;
			uint32_t rightBottom = leftBottom + 1;

			// 三角形1
			indexData[index++] = leftTop;
			indexData[index++] = leftBottom;
			indexData[index++] = rightTop;

			// 三角形2
			indexData[index++] = rightTop;
			indexData[index++] = leftBottom;
			indexData[index++] = rightBottom;
		}
	}
}



// ============================================
// グリッドの表示
// ============================================
void DrawGrid(const Matrix4x4& viewProjectionMatrix, const Matrix4x4& viewportMatrix) {
	const float kGridHalfWidth = 2.0f;                                      // Gridの半分の幅
	const uint32_t kSubdivision = 10;                                       // 分割数
	const float kGridEvery = (kGridHalfWidth * 2.0f) / float(kSubdivision); // 1つ分の長さ

	// 奥から手前への線を順々に引いていく
	for (uint32_t xIndex = 0; xIndex <= kSubdivision; ++xIndex) {
		// 現在の線のワールドX座標を計算
		float x = -kGridHalfWidth + (float(xIndex) * kGridEvery);

		// 始点と終点のワールド座標を設定
		Vector3 startWorld = { x, 0.0f, -kGridHalfWidth }; // 手前
		Vector3 endWorld = { x, 0.0f, kGridHalfWidth };    // 奥

		// スクリーン座標への変換
		Vector3 startScreen = MyMath::Transform(MyMath::Transform(startWorld, viewProjectionMatrix), viewportMatrix);
		Vector3 endScreen = MyMath::Transform(MyMath::Transform(endWorld, viewProjectionMatrix), viewportMatrix);

		// 色の決定 (原点黒)
		uint32_t color = (std::abs(x) < 0.001f) ? 0x000000FF : 0xAAAAAAFF;

		// 線を描画
		//Novice::DrawLine(int(startScreen.x), int(startScreen.y), int(endScreen.x), int(endScreen.y), color);
	}

	// 左から右も同じように順々に引いていく
	for (uint32_t zIndex = 0; zIndex <= kSubdivision; ++zIndex) {
		// 現在の線のワールドZ座標を計算
		float z = -kGridHalfWidth + (float(zIndex) * kGridEvery);

		// 始点と終点のワールド座標を設定
		Vector3 startWorld = { -kGridHalfWidth, 0.0f, z }; // 左
		Vector3 endWorld = { kGridHalfWidth, 0.0f, z };    // 右

		// スクリーン座標への変換
		Vector3 startScreen = MyMath::Transform(MyMath::Transform(startWorld, viewProjectionMatrix), viewportMatrix);
		Vector3 endScreen = MyMath::Transform(MyMath::Transform(endWorld, viewProjectionMatrix), viewportMatrix);

		// 色の決定 (原点黒)
		uint32_t color = (std::abs(z) < 0.001f) ? 0x000000FF : 0xAAAAAAFF;

		// 線を描画
		//Novice::DrawLine(int(startScreen.x), int(startScreen.y), int(endScreen.x), int(endScreen.y), color);
	}
}


// ============================================
// 行列演算
// ============================================
// 行列の加法
Matrix4x4 MyMath::Add(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result;
	for (int i = 0; i < 4; ++i) {     // 行
		for (int j = 0; j < 4; ++j) { // 列
			result.m[i][j] = m1.m[i][j] + m2.m[i][j];
		}
	}
	return result;
}

// 行列の減法
Matrix4x4 MyMath::Subtract(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result;
	for (int i = 0; i < 4; ++i) {     // 行
		for (int j = 0; j < 4; ++j) { // 列
			result.m[i][j] = m1.m[i][j] - m2.m[i][j];
		}
	}
	return result;
}

// 行列の積
Matrix4x4 MyMath::Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result;
	for (int i = 0; i < 4; ++i) {     // 行
		for (int j = 0; j < 4; ++j) { // 列
			result.m[i][j] = 0.0f;
			for (int k = 0; k < 4; ++k) {
				result.m[i][j] += m1.m[i][k] * m2.m[k][j];
			}
		}
	}
	return result;
}

// 逆行列
float Determinant3x3(float a11, float a12, float a13, float a21, float a22, float a23, float a31, float a32, float a33) {
	return a11 * a22 * a33 + a12 * a23 * a31 + a13 * a21 * a32 - a13 * a22 * a31 - a12 * a21 * a33 - a11 * a23 * a32;
}

Matrix4x4 MyMath::Inverse(const Matrix4x4& m) {
	float det = m.m[0][0] * Determinant3x3(m.m[1][1], m.m[1][2], m.m[1][3], m.m[2][1], m.m[2][2], m.m[2][3], m.m[3][1], m.m[3][2], m.m[3][3]) -
		m.m[0][1] * Determinant3x3(m.m[1][0], m.m[1][2], m.m[1][3], m.m[2][0], m.m[2][2], m.m[2][3], m.m[3][0], m.m[3][2], m.m[3][3]) +
		m.m[0][2] * Determinant3x3(m.m[1][0], m.m[1][1], m.m[1][3], m.m[2][0], m.m[2][1], m.m[2][3], m.m[3][0], m.m[3][1], m.m[3][3]) -
		m.m[0][3] * Determinant3x3(m.m[1][0], m.m[1][1], m.m[1][2], m.m[2][0], m.m[2][1], m.m[2][2], m.m[3][0], m.m[3][1], m.m[3][2]);

	if (std::abs(det) < 1.0e-6f)
		return {};

	float invDet = 1.0f / det;
	Matrix4x4 result;

	// 余因子行列
	result.m[0][0] = Determinant3x3(m.m[1][1], m.m[1][2], m.m[1][3], m.m[2][1], m.m[2][2], m.m[2][3], m.m[3][1], m.m[3][2], m.m[3][3]) * invDet;
	result.m[0][1] = -Determinant3x3(m.m[0][1], m.m[0][2], m.m[0][3], m.m[2][1], m.m[2][2], m.m[2][3], m.m[3][1], m.m[3][2], m.m[3][3]) * invDet;
	result.m[0][2] = Determinant3x3(m.m[0][1], m.m[0][2], m.m[0][3], m.m[1][1], m.m[1][2], m.m[1][3], m.m[3][1], m.m[3][2], m.m[3][3]) * invDet;
	result.m[0][3] = -Determinant3x3(m.m[0][1], m.m[0][2], m.m[0][3], m.m[1][1], m.m[1][2], m.m[1][3], m.m[2][1], m.m[2][2], m.m[2][3]) * invDet;

	result.m[1][0] = -Determinant3x3(m.m[1][0], m.m[1][2], m.m[1][3], m.m[2][0], m.m[2][2], m.m[2][3], m.m[3][0], m.m[3][2], m.m[3][3]) * invDet;
	result.m[1][1] = Determinant3x3(m.m[0][0], m.m[0][2], m.m[0][3], m.m[2][0], m.m[2][2], m.m[2][3], m.m[3][0], m.m[3][2], m.m[3][3]) * invDet;
	result.m[1][2] = -Determinant3x3(m.m[0][0], m.m[0][2], m.m[0][3], m.m[1][0], m.m[1][2], m.m[1][3], m.m[3][0], m.m[3][2], m.m[3][3]) * invDet;
	result.m[1][3] = Determinant3x3(m.m[0][0], m.m[0][2], m.m[0][3], m.m[1][0], m.m[1][2], m.m[1][3], m.m[2][0], m.m[2][2], m.m[2][3]) * invDet;

	result.m[2][0] = Determinant3x3(m.m[1][0], m.m[1][1], m.m[1][3], m.m[2][0], m.m[2][1], m.m[2][3], m.m[3][0], m.m[3][1], m.m[3][3]) * invDet;
	result.m[2][1] = -Determinant3x3(m.m[0][0], m.m[0][1], m.m[0][3], m.m[2][0], m.m[2][1], m.m[2][3], m.m[3][0], m.m[3][1], m.m[3][3]) * invDet;
	result.m[2][2] = Determinant3x3(m.m[0][0], m.m[0][1], m.m[0][3], m.m[1][0], m.m[1][1], m.m[1][3], m.m[3][0], m.m[3][1], m.m[3][3]) * invDet;
	result.m[2][3] = -Determinant3x3(m.m[0][0], m.m[0][1], m.m[0][3], m.m[1][0], m.m[1][1], m.m[1][3], m.m[2][0], m.m[2][1], m.m[2][3]) * invDet;

	result.m[3][0] = -Determinant3x3(m.m[1][0], m.m[1][1], m.m[1][2], m.m[2][0], m.m[2][1], m.m[2][2], m.m[3][0], m.m[3][1], m.m[3][2]) * invDet;
	result.m[3][1] = Determinant3x3(m.m[0][0], m.m[0][1], m.m[0][2], m.m[2][0], m.m[2][1], m.m[2][2], m.m[3][0], m.m[3][1], m.m[3][2]) * invDet;
	result.m[3][2] = -Determinant3x3(m.m[0][0], m.m[0][1], m.m[0][2], m.m[1][0], m.m[1][1], m.m[1][2], m.m[3][0], m.m[3][1], m.m[3][2]) * invDet;
	result.m[3][3] = Determinant3x3(m.m[0][0], m.m[0][1], m.m[0][2], m.m[1][0], m.m[1][1], m.m[1][2], m.m[2][0], m.m[2][1], m.m[2][2]) * invDet;

	return result;
}

// 転置行列
Matrix4x4 MyMath::Transpose(const Matrix4x4& m) {
	Matrix4x4 result;
	for (int i = 0; i < 4; ++i) {     // 行
		for (int j = 0; j < 4; ++j) { // 列
			result.m[i][j] = m.m[j][i];
		}
	}
	return result;
}

// 単位行列の作成 
Matrix4x4 MyMath::Identity() {
	Matrix4x4 result = {};
	for (int i = 0; i < 4; ++i) {
		result.m[i][i] = 1.0f; // 対角成分を1に設定
	}
	return result;
}

// ============================================
// 変換行列 
// ============================================
// 平行移動行列の作成
Matrix4x4 MyMath::MakeTranslateMatrix(const Vector3& translate) {
	Matrix4x4 result = Identity();
	result.m[3][0] = translate.x;
	result.m[3][1] = translate.y;
	result.m[3][2] = translate.z;
	return result;
}

// 拡大縮小行列の作成
Matrix4x4 MyMath::MakeScaleMatrix(const Vector3& scale) {
	Matrix4x4 result = {};
	result.m[0][0] = scale.x;
	result.m[1][1] = scale.y;
	result.m[2][2] = scale.z;
	result.m[3][3] = 1.0f; // w成分
	return result;
}

// 座標変換
Vector3 MyMath::Transform(const Vector3& vector, const Matrix4x4& matrix) {
	Vector3 result;
	// (x, y, z, 1) として行列計算　
	result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + 1.0f * matrix.m[3][0];
	result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + 1.0f * matrix.m[3][1];
	result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + 1.0f * matrix.m[3][2];
	float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + 1.0f * matrix.m[3][3];

	// wで割ってデカルト座標に戻す
	assert(w != 0.0f);
	result.x /= w;
	result.y /= w;
	result.z /= w;
	return result;
}

// X軸回転行列の作成
Matrix4x4 MyMath::MakeRotateXMatrix(float radian) {
	Matrix4x4 result = Identity();
	float cosTheta = std::cos(radian);
	float sinTheta = std::sin(radian);
	result.m[1][1] = cosTheta;
	result.m[1][2] = sinTheta;
	result.m[2][1] = -sinTheta;
	result.m[2][2] = cosTheta;
	return result;
}

// Y軸回転行列の作成
Matrix4x4 MyMath::MakeRotateYMatrix(float radian) {
	Matrix4x4 result = Identity();
	float cosTheta = std::cos(radian);
	float sinTheta = std::sin(radian);
	result.m[0][0] = cosTheta;
	result.m[0][2] = -sinTheta;
	result.m[2][0] = sinTheta;
	result.m[2][2] = cosTheta;
	return result;
}

// Z軸回転行列の作成
Matrix4x4 MyMath::MakeRotateZMatrix(float radian) {
	Matrix4x4 result = Identity();
	float cosTheta = std::cos(radian);
	float sinTheta = std::sin(radian);
	result.m[0][0] = cosTheta;
	result.m[0][1] = sinTheta;
	result.m[1][0] = -sinTheta;
	result.m[1][1] = cosTheta;
	return result;
}

// ============================================
// アフィン変換
// ============================================
// アフィン変換行列の作成
Matrix4x4 MyMath::MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
	Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);             // 拡大縮小行列 (S)
	Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotate.x);      // 回転X行列 (Rx)
	Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotate.y);      // 回転Y行列 (Ry)
	Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotate.z);      // 回転Z行列 (Rz)
	Matrix4x4 translateMatrix = MakeTranslateMatrix(translate); // 平行移動行列 (T)

	// R = Rx * Ry * Rz
	Matrix4x4 rotateMatrix = Multiply(Multiply(rotateXMatrix, rotateYMatrix), rotateZMatrix);

	// W = S * R * T
	Matrix4x4 affineMatrix = Multiply(Multiply(scaleMatrix, rotateMatrix), translateMatrix);
	return affineMatrix;
}

// ============================================
// 座標変換行列
// ============================================
// 透視投影行列の作成
Matrix4x4 MyMath::MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
	Matrix4x4 result = {};
	float f = 1.0f / std::tan(fovY / 2.0f);
	result.m[0][0] = f / aspectRatio;
	result.m[1][1] = f;
	result.m[2][2] = farClip / (farClip - nearClip);
	result.m[2][3] = 1.0f;
	result.m[3][2] = (-nearClip * farClip) / (farClip - nearClip);
	return result;
}

// 正射影行列の作成
Matrix4x4 MyMath::MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
	Matrix4x4 result = {};

	result.m[0][0] = 2.0f / (right - left);
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[2][2] = 1.0f / (farClip - nearClip);
	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = nearClip / (nearClip - farClip);
	result.m[3][3] = 1.0f;

	return result;
}
// ビューポート変換行列の作成
Matrix4x4 MyMath::MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
	Matrix4x4 result = {};

	result.m[0][0] = width / 2.0f;
	result.m[1][1] = -height / 2.0f;
	result.m[2][2] = maxDepth - minDepth;

	result.m[3][0] = left + width / 2.0f;
	result.m[3][1] = top + height / 2.0f;
	result.m[3][2] = minDepth;
	result.m[3][3] = 1.0f;

	return result;
}

// ============================================
// クロス積
// ============================================
// クロス積
Vector3 MyMath::Cross(const Vector3& v1, const Vector3& v2) {
	Vector3 result;
	// X成分
	result.x = v1.y * v2.z - v1.z * v2.y;
	// Y成分
	result.y = v1.z * v2.x - v1.x * v2.z;
	// Z成分
	result.z = v1.x * v2.y - v1.y * v2.x;
	return result;
}

// ============================================
// ドット積
// ============================================ 
// 正射影ベクトル
Vector3 MyMath::Project(const Vector3& v1, const Vector3& v2) {
	float dot = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;           // ドット積  (a・b = |a||b|cosθ)
	float lengthSquared = v2.x * v2.x + v2.y * v2.y + v2.z * v2.z; // v2の長さの二乗 (||b||^2)
	assert(lengthSquared != 0.0f);                                 // ゼロ除算を防止
	float scale = dot / lengthSquared;                             // スケーリングファクター
	Vector3 result;
	result.x = scale * v2.x;
	result.y = scale * v2.y;
	result.z = scale * v2.z;
	return result;
}

// Add
Vector3 MyMath::Add(const Vector3& v1, const Vector3& v2) {
	Vector3 result;
	result.x = v1.x + v2.x;
	result.y = v1.y + v2.y;
	result.z = v1.z + v2.z;
	return result;
}

// Subtract
Vector3 MyMath::Subtract(const Vector3& v1, const Vector3& v2) {
	Vector3 result;
	result.x = v1.x - v2.x;
	result.y = v1.y - v2.y;
	result.z = v1.z - v2.z;
	return result;
}

// 点から線分への最近接点
Vector3 MyMath::ClosestPoint(const Vector3& point, const Segment& segment) {
	Vector3 segmentStart = segment.origin;                                                                                           // 線分の始点
	Vector3 segmentEnd = { segment.origin.x + segment.diff.x, segment.origin.y + segment.diff.y, segment.origin.z + segment.diff.z };  // 線分の終点
	Vector3 segmentVector = { segmentEnd.x - segmentStart.x, segmentEnd.y - segmentStart.y, segmentEnd.z - segmentStart.z };           // 線分のベクトル
	Vector3 pointVector = { point.x - segmentStart.x, point.y - segmentStart.y, point.z - segmentStart.z };                            // 点から線分の始点へのベクトル
	float dot = pointVector.x * segmentVector.x + pointVector.y * segmentVector.y + pointVector.z * segmentVector.z;                 // ドット積 (a・b)
	float lengthSquared = segmentVector.x * segmentVector.x + segmentVector.y * segmentVector.y + segmentVector.z * segmentVector.z; // 線分ベクトルの長さの二乗 (||b||^2)
	assert(lengthSquared != 0.0f);                                                                                                   // ゼロ除算を防止
	float t = dot / lengthSquared;                                                                                                   // スケーリングファクター
	// tを0～1の範囲にクランプ
	if (t < 0.0f) {
		t = 0.0f;
	}
	else if (t > 1.0f) {
		t = 1.0f;
	}
	Vector3 closestPoint;
	closestPoint.x = segmentStart.x + t * segmentVector.x;
	closestPoint.y = segmentStart.y + t * segmentVector.y;
	closestPoint.z = segmentStart.z + t * segmentVector.z;
	return closestPoint;
}

// ============================================
// 衝突判定
// ============================================
float MyMath::Length(const Vector3& v) {
	return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

bool MyMath::IsCollision(const Sphere& s1, const Sphere& s2) {
	// 2つの球の中心点間の距離を求める
	float distance = MyMath::Length(MyMath::Subtract(s2.center, s1.center));

	// 半径の合計よりも短ければ衝突
	if (distance <= s1.radius + s2.radius) {
		return true; // 衝突した
	}
	return false; // 衝突していない
}

// 内積（ドット積）の計算
float MyMath::Dot(const Vector3& v1, const Vector3& v2) {
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

bool MyMath::IsCollision(const Sphere& sphere, const Plane& plane) {
	float t = MyMath::Dot(plane.normal, sphere.center) - plane.distance;

	// 距離の絶対値を求める
	float distance = std::abs(t);

	// 半径以下なら衝突
	if (distance <= sphere.radius) {
		return true;
	}
	return false;
}