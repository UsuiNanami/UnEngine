#pragma once
#include <cmath>
#include <stdint.h>

struct Vector3 { // 3次元ベクトル
	float x;
	float y;
	float z;
};

struct Vector2 {
	float x;
	float y;
};

struct Vector4 {
	float x;
	float y;
	float z;
	float w;
};

struct Matrix4x4 { // 4x4行列
	float m[4][4];
};

struct Matrix3x3 { // 3x3行列
	float m[3][3];
};

struct Sphere {     // 球
	Vector3 center; // 中心
	float radius;   // 半径
};

struct Line {       // 線分
	Vector3 origin; // 始点
	Vector3 diff;   // 方向ベクトル
};

struct Ray {        // 光線
	Vector3 origin; // 始点
	Vector3 diff;   // 方向ベクトル
};

struct Segment {    // 線分
	Vector3 origin; // 始点
	Vector3 diff;   // 終点への差分ベクトル
};

struct Plane {      // 平面
	Vector3 normal; // 法線ベクトル
	float distance; // 原点から平面までの距離
};

struct VertexData {
	Vector4 position; // 頂点座標（x, y, z, w）
	Vector2 texcoord; // UV座標（u, v）
	Vector3 normal;   // 法線ベクトル（x, y, z）
};



// 球の表示
void DrawSphere(const Sphere& sphere, const Matrix4x4& viewProjectionMatrix, const Matrix4x4& viewportMatrix, uint32_t color);
// 球の頂点データの作成
void CreateSphereVertices(const Sphere& sphere, VertexData* vertexData, uint32_t kSubdivision);
// 球のインデックスデータの作成
void CreateSphereIndices(uint32_t* indexData, uint32_t kSubdivision);
// グリッドの表示
void DrawGrid(const Matrix4x4& viewProjectionMatrix, const Matrix4x4& viewportMatrix);
// 平面の表示
void DrawPlane(const Plane& plane, const Matrix4x4& viewProjectionMatrix, const Matrix4x4& viewportMatrix, uint32_t color);



class MyMath { // 数学関数をまとめたクラス
public:
	// ============================================
	// ベクトルの正規化
	// ============================================
	static Vector3 Normalize(const Vector3& v);


	// ============================================
	// 行列演算
	// ============================================
	// 行列の加法
	static Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2);
	// ベクトル用
	static Vector3 Add(const Vector3& v1, const Vector3& v2);

	// 行列の減法
	static Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2);
	// ベクトル用
	static Vector3 Subtract(const Vector3& v1, const Vector3& v2);

	// 行列の積
	static Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

	// 逆行列
	static Matrix4x4 Inverse(const Matrix4x4& m);

	// 転置行列
	static Matrix4x4 Transpose(const Matrix4x4& m);

	// 単位行列の作成
	static Matrix4x4 Identity();


	// ============================================
	// 変換行列
	// ============================================
	// 平行移動行列
	static Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

	// 拡大縮小行列
	static Matrix4x4 MakeScaleMatrix(const Vector3& scale);

	// 座標変換
	static Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);

	// X軸回転行列
	static Matrix4x4 MakeRotateXMatrix(float radian);

	// Y軸回転行列
	static Matrix4x4 MakeRotateYMatrix(float radian);

	// Z軸回転行列
	static Matrix4x4 MakeRotateZMatrix(float radian);


	// ============================================
	// アフィン変換
	// ============================================
	// アフィン変換行列 (ワールド行列)
	static Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);


	// ============================================
	// 座標変換行列
	// ============================================
	// 透視投影行列 3D
	static Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

	// 正射影行列 2D
	static Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

	// ビューポート変換行列
	static Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);


	// ============================================
	// クロス積
	// ============================================
	static Vector3 Cross(const Vector3& v1, const Vector3& v2);



	// ============================================
	// ドット積
	// ============================================
	// 正射影ベクトル
	static Vector3 Project(const Vector3& v1, const Vector3& v2);

	// 点から線分への最近接点
	static Vector3 ClosestPoint(const Vector3& point, const Segment& segment);


	// ============================================
	// 衝突判定
	// ============================================ 
	// ベクトルの長さ
	static float Length(const Vector3& v);

	// 球と球の衝突判定
	static bool IsCollision(const Sphere& s1, const Sphere& s2);

	// 2つのベクトルの内積（ドット積）を求める関数
	static float Dot(const Vector3& v1, const Vector3& v2);

	// 球と平面の衝突判定
	static bool IsCollision(const Sphere& sphere, const Plane& plane);

};