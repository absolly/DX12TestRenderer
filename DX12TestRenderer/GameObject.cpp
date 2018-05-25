#include "GameObject.h"



GameObject::GameObject(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, std::string pName) : device(pDevice), commandList(pCommandList)
{

}


GameObject::~GameObject()
{
}

void GameObject::SetMaterial(TextureMaterial * pMaterial)
{
	_material = pMaterial;
}

TextureMaterial * GameObject::GetMaterial() const
{
	return _material;
}

void GameObject::SetMesh(Mesh * pMesh)
{
	_mesh = pMesh;
}

Mesh * GameObject::GetMesh() const
{
	return _mesh;
}