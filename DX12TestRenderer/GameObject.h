#pragma once

#include <DirectXMath.h>
#include "Mesh.h"
#include "TextureMaterial.h"
using namespace DirectX;

const int frameBufferCount = 3;

class GameObject
{
public:
	GameObject(std::string pName);
	~GameObject();
	void SetMaterial(TextureMaterial* pMaterial);
	TextureMaterial* GetMaterial() const;
	void SetMesh(Mesh* pMesh);
	Mesh* GetMesh() const;
	int cbIndex = -1;
	XMFLOAT4X4 WorldMat; //world matrix
	XMFLOAT4X4 RotMat; //keep track of the rotation
	XMFLOAT4 Position; //position
protected:

	Mesh* _mesh;
	TextureMaterial* _material;

};

