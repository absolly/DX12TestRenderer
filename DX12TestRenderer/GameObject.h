#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include "Mesh.h"
#include "TextureMaterial.h"
using namespace DirectX;

const int frameBufferCount = 3;

class GameObject
{
public:
	GameObject(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, std::string pName);
	~GameObject();
	void SetMaterial(TextureMaterial* pMaterial);
	TextureMaterial* GetMaterial() const;
	void SetMesh(Mesh* pMesh);
	Mesh* GetMesh() const;

protected:
	ID3D12Device* device;
	ID3D12GraphicsCommandList* commandList;

	

	XMFLOAT4X4 cube1WorldMat; //first cubes world matrix
	XMFLOAT4X4 cube1RotMat; //keep track of the first cubes rotation
	XMFLOAT4 cube1Position; //first cubes position

	Mesh* _mesh;
	TextureMaterial* _material;

};

