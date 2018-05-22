#ifndef MESH_H
#define MESH_H

#include <vector>
#include <DirectXMath.h>
using namespace DirectX; // we will be using the directxmath library


class GameObject;
class World;

/**
 * A mesh represents an .OBJ file. It knows how it is constructed, how its data should be buffered to OpenGL
 * and how it should be streamed to OpenGL
 */
class Mesh
{
	public:
		Mesh(std::string pId);
		virtual ~Mesh();

        /**
         * Loads a mesh from an .obj file. The file has to have:
         * vertexes, uvs, normals and face indexes. See load source
         * for more format information.
         */
		static Mesh* load(std::string pFileName, bool pDoBuffer = true);
		//void _sendDataToOpenGL(glm::mat4 pProjectionMatrix, glm::mat4 pViewMatrix, std::vector<GameObject*> pGameObjects);

        /**
         * Streams the mesh to opengl using the given indexes for the different attributes
         */
        void streamToOpenGL(int pVerticesAttrib, int pNormalsAttrib, int pUVsAttrib, int pTangentAttrib, int pBitangentAttrib);

		void instanceToOpenGL(int pVerticesAttrib, int pNormalsAttrib, int pUVsAttrib, int pTangentAttrib, int pBitangentAttrib);

		void drawInstancedmesh();

		void DisableVertexAttribArrays();

        /**
         * Draws debug info (normals) for the mesh using the given matrices)
         */
        void drawDebugInfo(const XMFLOAT4X4& pModelMatrix, const XMFLOAT4X4& pViewMatrix, const XMFLOAT4X4& pProjectionMatrix);

		std::vector<XMFLOAT3>* getVerticies();
		std::vector<unsigned>* getVertextIndices();

		//the actual data
		std::vector<XMFLOAT3> _vertices;       //vec3 with 3d coords for all vertices
		std::vector<XMFLOAT3> _normals;        //vec3 with 3d normal data
		std::vector<XMFLOAT3> _tangents;        //vec3 with 3d tangent data
		std::vector<XMFLOAT3> _bitangents;      //vec3 with 3d bitangent data
		std::vector<XMFLOAT2> _uvs;            //vec2 for uv

		//references to the vertices/normals & uvs in previous vectors
		std::vector<unsigned> _indices;

	protected:

	    std::string _id;

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

};

#endif // MESH_H
