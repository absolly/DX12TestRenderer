#include "GameObject.h"

int GameObject::_gameobjects = 0;

GameObject::GameObject(std::string pName, vec3 pPosition) : _transform(translate(pPosition)), _constantBufferID(_gameobjects)
{
	_gameobjects++;
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

glm::mat4 GameObject::GetTransform() const
{
	return _transform;
}

void GameObject::SetTransform(mat4 pTransform)
{
	_transform = pTransform;
}

void GameObject::scale(vec3 pScale)
{
	_transform = glm::scale(_transform, pScale);
}

void GameObject::Add(GameObject * pChild)
{
	pChild->SetParent(this);
}

void GameObject::SetParent(GameObject * pParent)
{
	//remove from previous parent
	if (_parent != NULL) {
		_parent->_innerRemove(this);
		_parent = NULL;
	}

	//set new parent
	if (pParent != NULL) {
		_parent = pParent;
		_parent->_innerAdd(this);
	}
}

void GameObject::_innerAdd(GameObject* pChild)
{
	//set new parent
	pChild->_parent = this;
	_children.push_back(pChild);
}

void GameObject::_innerRemove(GameObject* pChild) {
	for (auto i = _children.begin(); i != _children.end(); ++i) {
		if (*i == pChild) {
			(*i)->_parent = NULL;
			_children.erase(i);
			return;
		}
	}
}
