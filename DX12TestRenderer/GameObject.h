#pragma once

#include "Mesh.h"
#include "TextureMaterial.h"
#include "glm.h"
#include <vector>

using namespace glm;

const int frameBufferCount = 3;

class GameObject
{
public:
	GameObject(std::string pName, vec3 pPosition);
	~GameObject();
	void SetMaterial(TextureMaterial* pMaterial);
	TextureMaterial* GetMaterial() const;
	void SetMesh(Mesh* pMesh);
	Mesh* GetMesh() const;
	glm::mat4 GetTransform() const;
	void SetTransform(mat4 pTransform);
	void scale(vec3 pScale);
	void Add(GameObject* pChild);
	void SetParent(GameObject* pParent);
	const int _constantBufferID;

protected:
	static int _gameobjects;
	// update children list administration
	void _innerAdd(GameObject* pChild);
	void _innerRemove(GameObject* pChild);

	std::vector<GameObject*> _children;
	GameObject* _parent;
	glm::mat4 _transform;

	Mesh* _mesh;
	TextureMaterial* _material;

};

