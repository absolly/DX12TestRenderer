#ifndef MESH_H
#define MESH_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <vector>
#include <DirectXMath.h>
#include <windows.h>
#include "d3dx12.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include "Debug.h"

using namespace DirectX; // we will be using the directxmath library

struct Vertex
{
	Vertex(XMFLOAT3 pPos, XMFLOAT2 pTexCoord, XMFLOAT3 pNormal, XMFLOAT3 pTangent, XMFLOAT3 pBitangent) 
		     : pos(pPos), texCoord(pTexCoord), normal(pNormal), tangent(pTangent), bitangent(pBitangent) {}

	XMFLOAT3 pos;
	XMFLOAT2 texCoord;
	XMFLOAT3 normal;
	XMFLOAT3 tangent;
	XMFLOAT3 bitangent;
};

class GameObject;
class World;

/**
 * A mesh represents an .OBJ file. It knows how it is constructed, how its data should be buffered to OpenGL
 * and how it should be streamed to OpenGL
 */
class Mesh
{
	public:
		Mesh(std::string pId, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);
		virtual ~Mesh();

        /**
         * Loads a mesh from an .obj file. The file has to have:
         * vertexes, uvs, normals and face indexes. See load source
         * for more format information.
         */
		static Mesh* load(std::string pFileName, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, bool pDoBuffer = true);
		//void _sendDataToOpenGL(glm::mat4 pProjectionMatrix, glm::mat4 pViewMatrix, std::vector<GameObject*> pGameObjects);

        /**
         * Streams the mesh to opengl using the given indexes for the different attributes
         */
        void streamToOpenGL(int pVerticesAttrib, int pNormalsAttrib, int pUVsAttrib, int pTangentAttrib, int pBitangentAttrib);

		void instanceToOpenGL(int pVerticesAttrib, int pNormalsAttrib, int pUVsAttrib, int pTangentAttrib, int pBitangentAttrib);

		void SetVertexIndexBuffers();

		void Draw();

		void DisableVertexAttribArrays();

        /**
         * Draws debug info (normals) for the mesh using the given matrices)
         */
        void drawDebugInfo(const XMFLOAT4X4& pModelMatrix, const XMFLOAT4X4& pViewMatrix, const XMFLOAT4X4& pProjectionMatrix);

		std::vector<XMFLOAT3>*  getVerticies();
		std::vector<DWORD>* getVertextIndices();

		//the actual data
		std::vector<XMFLOAT3> _vertices;	//vec3 with 3d coords for all vertices
		std::vector<Vertex> _vertexData;	//full vertex data

		//references to the vertices/normals & uvs in previous vectors
		std::vector<DWORD> _indices;

	protected:

	    std::string _id;
		ID3D12Device* device;
		ID3D12GraphicsCommandList* commandList;

        //OpenGL id's for the different buffers created for this mesh
		unsigned int _indexBufferId;
		unsigned int _vertexBufferId;
		unsigned int _normalBufferId;
		unsigned int _tangentBufferId;
		unsigned int _bitangentBufferId;
		unsigned int _uvBufferId;

		unsigned int _uvattr;
		unsigned int _normalattr;
		unsigned int _verticesattrb;

		int numIndices;

		ID3D12Resource* vertexBuffer; //a default buffer in gpu memory that we will load the vertex data into

		D3D12_VERTEX_BUFFER_VIEW vertexBufferView; //a structure containing a pointer to the vertex data in gpu memory (to be used by the driver), 
												   //the total size of the buffer, and the size of each element

		ID3D12Resource* indexBuffer; //a default buffer in gpu memory that we will load index data into

		D3D12_INDEX_BUFFER_VIEW indexBufferView; //a stucture holding info about the index buffer

        //buffer vertices, normals, and uv's
		void _buffer();

        //Please read the "load" function documentation on the .obj file format first.
        //FaceVertexTriplet  is a helper class for loading and converting to obj file to
        //indexed arrays.
        //If we list all the unique v/uv/vn triplets under the faces
        //section in an object file sequentially and assign them a number
        //it would be a map of FaceVertexTriplet. Each FaceVertexTriplet refers
        //to an index with the originally loaded vertex list, normal list and uv list
        //and is only used during conversion (unpacking) of the facevertextriplet list
        //to a format that OpenGL can handle.
        //So for a vertex index a FaceVertexTriplet contains the index for uv and n as well.
		class FaceIndexTriplet {
			public:
				unsigned v; //vertex
				unsigned uv;//uv
				unsigned n; //normal
				FaceIndexTriplet( unsigned pV, unsigned pUV, unsigned pN )
				:	v(pV),uv(pUV),n(pN) {
				}
				//needed for use as key in map
				bool operator<(const FaceIndexTriplet other) const{
					return memcmp((void*)this, (void*)&other, sizeof(FaceIndexTriplet))>0;
				}
		};

		//upload data to constant buffer
		static ID3D12Resource* CreateDefaultBuffer(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			const void* initData,
			UINT64 byteSize,
			ID3D12Resource*& uploadBuffer
		);

};

#endif // MESH_H
