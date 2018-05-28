#include "GameObject.h"



GameObject::GameObject(std::string pName)
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