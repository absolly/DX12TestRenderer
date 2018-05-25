#include "Mesh.h"
#include <iostream>
#include <map>
#include <string>
#include <fstream>

using namespace std;


Mesh::Mesh(string pId, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
	: _id(pId), _indexBufferId(0), _vertexBufferId(0), _normalBufferId(0), _uvBufferId(0), _tangentBufferId(0), _bitangentBufferId(0),
	_vertices(), _indices(), _vertexData(), device(pDevice), commandList(pCommandList){
	//ctor
}

Mesh::~Mesh() {
	//dtor
}

/**
 * Load reads the obj data into a new mesh using C++ combined with c style coding.
 * The result is an indexed mesh for use with glDrawElements.
 * Expects a obj file with following layout v/vt/vn/f eg
 *
 * For example the obj file for a simple plane describes two triangles, based on
 * four vertices, with 4 uv's all having the same vertex normals (NOT FACE NORMALS!)
 *
 * v 10.000000 0.000000 10.000000              //vertex 1
 * v -10.000000 0.000000 10.000000             //vertex 2
 * v 10.000000 0.000000 -10.000000             //vertex 3
 * v -10.000000 0.000000 -10.000000            //vertex 4
 * vt 0.000000 0.000000                        //uv 1
 * vt 1.000000 0.000000                        //uv 2
 * vt 1.000000 1.000000                        //uv 3
 * vt 0.000000 1.000000                        //uv 4
 * vn 0.000000 1.000000 -0.000000              //normal 1 (normal for each vertex is same)
 * s off
 *
 * Using these vertices, uvs and normals we can construct faces, made up of 3 triplets (vertex, uv, normal)
 * f 2/1/1 1/2/1 3/3/1                         //face 1 (triangle 1)
 * f 4/4/1 2/1/1 3/3/1                         //face 2 (triangle 2)
 *
 * So although this is a good format for blender and other tools reading .obj files, this is
 * not an index mechanism that OpenGL supports out of the box.
 * The reason is that OpenGL supports only one indexbuffer, and the value at a certain point in the indexbuffer, eg 3
 * refers to all three other buffers (v, vt, vn) at once,
 * eg if index[0] = 5, opengl will stream vertexBuffer[5], uvBuffer[5], normalBuffer[5] into the shader.
 *
 * So what we have to do after reading the file with all vertices, is construct unique indexes for
 * all pairs that are described by the faces in the object file, eg if you have
 * f 2/1/1 1/2/1 3/3/1                         //face 1 (triangle 1)
 * f 4/4/1 2/1/1 3/3/1                         //face 2 (triangle 2)
 *
 * v/vt/vn[0] will represent 2/1/1
 * v/vt/vn[1] will represent 1/2/1
 * v/vt/vn[2] will represent 3/3/1
 * v/vt/vn[3] will represent 4/4/1
 *
 * and that are all unique pairs, after which our index buffer can contain:
 *
 * 0,1,2,3,0,2
 *
 * So the basic process is, read ALL data into separate arrays, then use the faces to
 * create unique entries in a new set of arrays and create the indexbuffer to go along with it.
 *
 * Note that loading this mesh isn't cached like we do with texturing, this is an exercise left for the students.
 */
Mesh* Mesh::load(string pFileName, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, bool pDoBuffer) {
	//cout << "Loading " << pFileName << "...";

	Mesh* mesh = new Mesh(pFileName, pDevice, pCommandList);

	ifstream file(pFileName, ios::in);

	if (file.is_open()) {
		//these three vectors will contains data as taken from the obj file
		//in the order it is encountered in the object file
		vector<XMFLOAT3> vertices;
		vector<XMFLOAT3> fs;
		vector<XMFLOAT2> uvs;
		vector<XMFLOAT3> normals;

		//in addition we create a map to store the triplets found under the f(aces) section in the
		//object file and map them to an index for our index buffer (just number them sequentially
		//as we encounter them and store references to the pack
		map <FaceIndexTriplet, unsigned int> mappedTriplets;

		string line; // to store each line in
		while (getline(file, line)) {

			// c-type string to store cmd read from obj file (cmd is v, vt, vn, f)
			char cmd[10];
			cmd[0] = 0;

			//get the first string in the line of max 10 chars (c-style)
			sscanf(line.c_str(), "%10s", cmd);

			//note that although the if statements below seem to imply that we can
			//read these different line types (eg vertex, normal, uv) in any order,
			//this is just convenience coding for us (instead of multiple while loops)
			//we assume the obj file to list ALL v lines first, then ALL vt lines,
			//then ALL vn lines and last but not least ALL f lines last

			//so... start processing lines
			//are we reading a vertex line? straightforward copy into local vertices vector
			if (strcmp(cmd, "v") == 0) {
				XMFLOAT3 vertex;
				sscanf(line.c_str(), "%10s %f %f %f ", cmd, &vertex.x, &vertex.y, &vertex.z);
				vertices.push_back(vertex);

				//or are we reading a normal line? straightforward copy into local normal vector
			}
			else if (strcmp(cmd, "vn") == 0) {
				XMFLOAT3 normal;
				sscanf(line.c_str(), "%10s %f %f %f ", cmd, &normal.x, &normal.y, &normal.z);
				normals.push_back(normal);

				//or are we reading a uv line? straightforward copy into local uv vector
			}
			else if (strcmp(cmd, "vt") == 0) {
				XMFLOAT2 uv;
				sscanf(line.c_str(), "%10s %f %f ", cmd, &uv.x, &uv.y);

				//TODO this is a fix for the convertion from opengl to directX, might be a better solution for this
				uv.y = 1 - uv.y;
				uvs.push_back(uv);

				//this is where it gets nasty. After having read all vertices, normals and uvs into
				//their own buffer
			}
			else if (strcmp(cmd, "f") == 0) {

				//an f lines looks like
				//f 2/1/1 1/2/1 3/3/1
				//in other words
				//f v1/u1/n1 v2/u2/n2 v3/u3/n3
				//for each triplet like that we need to check whether we already encountered it
				//and update our administration based on that
				XMINT4 vertexIndex;
				XMINT4 normalIndex;
				XMINT4 uvIndex;

				int count = sscanf(line.c_str(), "%10s %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d", cmd, &vertexIndex.x, &uvIndex.x, &normalIndex.x, &vertexIndex.y, &uvIndex.y, &normalIndex.y, &vertexIndex.z, &uvIndex.z, &normalIndex.z, &vertexIndex.w, &uvIndex.w, &normalIndex.w);

				//Have we read exactly 10 elements?
				if (count == 10 || count == 13) {
					//process 3 triplets, one for each vertex (which is first element of the triplet)
					//cout << count << endl;
					int vertCount = (vertexIndex.w) ? 6 : 3;

					XMFLOAT3 edge1;
					XMStoreFloat3(&edge1, XMLoadFloat3(&vertices[vertexIndex.y - 1]) - XMLoadFloat3(&vertices[vertexIndex.x - 1]));
					XMFLOAT3 edge2;
					XMStoreFloat3(&edge2, XMLoadFloat3(&vertices[vertexIndex.z - 1]) - XMLoadFloat3(&vertices[vertexIndex.x - 1]));
					XMFLOAT2 deltaUV1;
					XMStoreFloat2(&deltaUV1, XMLoadFloat2(&uvs[uvIndex.y - 1]) - XMLoadFloat2(&uvs[uvIndex.x - 1]));
					XMFLOAT2 deltaUV2;
					XMStoreFloat2(&deltaUV2, XMLoadFloat2(&uvs[uvIndex.z - 1]) - XMLoadFloat2(&uvs[uvIndex.x - 1]));

					float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

					XMFLOAT3 tangent;
					XMFLOAT3 bitangent;
					tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
					tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
					tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
					XMStoreFloat3(&tangent, XMVector3Normalize(XMLoadFloat3(&tangent)));

					bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
					bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
					bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
					XMStoreFloat3(&bitangent, XMVector3Normalize(XMLoadFloat3(&bitangent)));

					for (int i = 0; i < vertCount; ++i) {
						int vindex;
						int uIndex;
						int nIndex;
						switch (i) {
						case 0:
						case 3:
							vindex = vertexIndex.x;
							uIndex = uvIndex.x;
							nIndex = normalIndex.x;
							break;
						case 1:
							vindex = vertexIndex.y;
							uIndex = uvIndex.y;
							nIndex = normalIndex.y;
							break;
						case 2:
						case 4:
							vindex = vertexIndex.z;
							uIndex = uvIndex.z;
							nIndex = normalIndex.z;
							break;
						case 5:
							vindex = vertexIndex.w;
							uIndex = uvIndex.w;
							nIndex = normalIndex.w;
							break;
						}

						/*vertexIndex[newIndex] = (vertexIndex[newIndex] < 0) ? vertices.size() + vertexIndex[newIndex] + 1 : vertexIndex[newIndex];
						uvIndex[newIndex] = (uvIndex[newIndex] < 0) ? uvs.size() + uvIndex[newIndex] + 1 : uvIndex[newIndex];
						normalIndex[newIndex] = (normalIndex[newIndex] < 0) ? normals.size() + normalIndex[newIndex] + 1 : normalIndex[newIndex];*/

						if (vindex > vertices.size() || uIndex > uvs.size() || nIndex > normals.size()) {
							//If we read a different amount, something is wrong
							cout << "Error reading obj: cannot work with negative indices" << endl;
							delete mesh;
							return NULL;

						}
						//create key out of the triplet and check if we already encountered this before
						FaceIndexTriplet triplet(vindex, vindex, nIndex);
						map<FaceIndexTriplet, unsigned int>::iterator found = mappedTriplets.find(triplet);

						//if iterator points at the end, we haven't found it
						if (found == mappedTriplets.end()) {
							//so create a new index value, and map our triplet to it
							unsigned int index = mappedTriplets.size();
							mappedTriplets[triplet] = index;

							//now record this index
							mesh->_indices.push_back(index);
							//and store the corresponding vertex/normal/uv values into our own buffers
							//note the -1 is required since all values in the f triplets in the .obj file
							//are 1 based, but our vectors are 0 based
							mesh->_vertexData.push_back(Vertex(vertices[vindex - 1], uvs[uIndex - 1], normals[nIndex - 1], tangent, bitangent));
							mesh->_vertices.push_back(vertices[vindex - 1]);
							//mesh->_normals.push_back(normals[nIndex - 1]);
							//mesh->_tangents.push_back(tangent);
							//mesh->_bitangents.push_back(bitangent);
							//mesh->_uvs.push_back(uvs[uIndex - 1]);
						}
						else {
							//if the key was already present, get the index value for it
							unsigned int index = found->second;
							//and update our index buffer with it
							mesh->_indices.push_back(index);
						}
					}
				}
				else {
					//If we read a different amount, something is wrong
					cout << "Error reading obj, needing v,vn,vt" << endl;
					delete mesh;
					return NULL;
				}
			}

		}

		file.close();

		mesh->_buffer();

		//cout << "Mesh loaded and buffered:" << (mesh->_indices.size() / 3.0f) << " triangles." << endl;
		return mesh;
	}
	else {
		cout << "Could not read " << pFileName << endl;
		delete mesh;
		return NULL;
	}
}

void Mesh::_buffer() {
	//std::vector<Vertex> vList = _vertexData;

	int vBufferSize = _vertexData.size() * sizeof(Vertex);
	ID3D12Resource* vBufferUploadHeap;

	//create the default buffer for the vertex data and upload the data using an upload buffer.
	vertexBuffer = CreateDefaultBuffer(device, commandList, &_vertexData[0], vBufferSize, vBufferUploadHeap);

	////transition the vertex buffer data from copy destination state to vertex buffer state
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	//create index buffer
	//std::vector <DWORD> iList = _indices;

	int iBufferSize = sizeof(DWORD) * _indices.size();

	numIndices = _indices.size(); //the number of indeces we want to draw (size of the (iList)/(size of one float3) i think)

	ID3D12Resource* iBufferUploadHeap;
	indexBuffer = CreateDefaultBuffer(device, commandList, &_indices[0], iBufferSize, iBufferUploadHeap);

	//transition index buffer data from copy to index buffer state
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));

	//create a vertex buffer view for the triangle. we get the gpu memory address to the vertex pointer using the GetGPUVirtualAddress() method
	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizeof(Vertex);
	vertexBufferView.SizeInBytes = vBufferSize;

	//create a index buffer view for the triangle. gets the gpu memory address to the pointer.
	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	indexBufferView.SizeInBytes = iBufferSize;
}

void Mesh::streamToOpenGL(int pVerticesAttrib, int pNormalsAttrib, int pUVsAttrib, int pTangentAttrib, int pBitangentAttrib) {
	
	//if (pVerticesAttrib != -1) {
	//	glBindBuffer(GL_ARRAY_BUFFER, _vertexBufferId);
	//	glEnableVertexAttribArray(pVerticesAttrib);
	//	glVertexAttribPointer(pVerticesAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
	//}

	//if (pNormalsAttrib != -1) {
	//	glBindBuffer(GL_ARRAY_BUFFER, _normalBufferId);
	//	glEnableVertexAttribArray(pNormalsAttrib);
	//	glVertexAttribPointer(pNormalsAttrib, 3, GL_FLOAT, GL_TRUE, 0, 0);
	//}

	//if (pUVsAttrib != -1) {
	//	glBindBuffer(GL_ARRAY_BUFFER, _uvBufferId);
	//	glEnableVertexAttribArray(pUVsAttrib);
	//	glVertexAttribPointer(pUVsAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
	//}

	//if (pTangentAttrib != -1) {
	//	glBindBuffer(GL_ARRAY_BUFFER, _tangentBufferId);
	//	glEnableVertexAttribArray(pTangentAttrib);
	//	glVertexAttribPointer(pTangentAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
	//}

	//if (pBitangentAttrib != -1) {
	//	glBindBuffer(GL_ARRAY_BUFFER, _bitangentBufferId);
	//	glEnableVertexAttribArray(pBitangentAttrib);
	//	glVertexAttribPointer(pBitangentAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
	//}

	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexBufferId);

	//glDrawElements(GL_TRIANGLES, _indices.size(), GL_UNSIGNED_INT, (GLvoid*)0);

	//// no current buffer, to avoid mishaps, very important for performance

	//glBindBuffer(GL_ARRAY_BUFFER, 0);

	////fix for serious performance issue
	//if (pUVsAttrib != -1) glDisableVertexAttribArray(pUVsAttrib);
	//if (pNormalsAttrib != -1) glDisableVertexAttribArray(pNormalsAttrib);
	//if (pVerticesAttrib != -1) glDisableVertexAttribArray(pVerticesAttrib);
	//if (pTangentAttrib != -1) glDisableVertexAttribArray(pTangentAttrib);
	//if (pBitangentAttrib != -1) glDisableVertexAttribArray(pBitangentAttrib);
}

void Mesh::instanceToOpenGL(int pVerticesAttrib, int pNormalsAttrib, int pUVsAttrib, int pTangentAttrib, int pBitangentAttrib) {
	//if (pVerticesAttrib != -1) {
	//	glBindBuffer(GL_ARRAY_BUFFER, _vertexBufferId);
	//	glEnableVertexAttribArray(pVerticesAttrib);
	//	glVertexAttribPointer(pVerticesAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
	//}

	//if (pNormalsAttrib != -1) {
	//	glBindBuffer(GL_ARRAY_BUFFER, _normalBufferId);
	//	glEnableVertexAttribArray(pNormalsAttrib);
	//	glVertexAttribPointer(pNormalsAttrib, 3, GL_FLOAT, GL_TRUE, 0, 0);
	//}

	//if (pUVsAttrib != -1) {
	//	glBindBuffer(GL_ARRAY_BUFFER, _uvBufferId);
	//	glEnableVertexAttribArray(pUVsAttrib);
	//	glVertexAttribPointer(pUVsAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
	//}

	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexBufferId);

	////////if (pTangentAttrib != -1) {
	////////	glBindBuffer(GL_ARRAY_BUFFER, _tangentBufferId);
	////////	glEnableVertexAttribArray(pTangentAttrib);
	////////	glVertexAttribPointer(pTangentAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
	////////}

	////////if (pBitangentAttrib != -1) {
	////////	glBindBuffer(GL_ARRAY_BUFFER, _bitangentBufferId);
	////////	glEnableVertexAttribArray(pBitangentAttrib);
	////////	glVertexAttribPointer(pBitangentAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
	////////}

	////////glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexBufferId);

	////////glDrawElements(GL_TRIANGLES, _indices.size(), GL_UNSIGNED_INT, (GLvoid*)0);

	//////// no current buffer, to avoid mishaps, very important for performance

	////////glBindBuffer(GL_ARRAY_BUFFER, 0);

	////////fix for serious performance issue
	///////*if (pUVsAttrib != -1) glDisableVertexAttribArray(pUVsAttrib);
	//////if (pNormalsAttrib != -1) glDisableVertexAttribArray(pNormalsAttrib);
	//////if (pVerticesAttrib != -1) glDisableVertexAttribArray(pVerticesAttrib);*/

	//_uvattr = pUVsAttrib;
	//_normalattr = pNormalsAttrib;
	//_verticesattrb = pVerticesAttrib;
}

void Mesh::SetVertexIndexBuffers()
{
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	commandList->IASetIndexBuffer(&indexBufferView);
}

void Mesh::Draw()
{
	commandList->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);
}

void Mesh::DisableVertexAttribArrays()
{
	//glBindBuffer(GL_ARRAY_BUFFER, 0);

	//if (_uvattr != -1) glDisableVertexAttribArray(_uvattr);
	//if (_normalattr != -1) glDisableVertexAttribArray(_normalattr);
	//if (_verticesattrb != -1) glDisableVertexAttribArray(_verticesattrb);
}




void Mesh::drawDebugInfo(const XMFLOAT4X4& pModelMatrix, const XMFLOAT4X4& pViewMatrix, const XMFLOAT4X4& pProjectionMatrix) {
	//demo of how to render some debug info using the good ol' direct rendering mode...
	//glUseProgram(0);
	//glMatrixMode(GL_PROJECTION);
	//glLoadMatrixf(glm::value_ptr(pProjectionMatrix));
	//glMatrixMode(GL_MODELVIEW);
	//glLoadMatrixf(glm::value_ptr(pViewMatrix * pModelMatrix));

	//glBegin(GL_LINES);
	////for each index draw the normal starting at the corresponding vertex
	//for (size_t i = 0; i < _indices.size(); i++) {
	//	//draw normal for vertex
	//	if (true) {
	//		//now get normal end
	//		glm::vec3 normal = _normals[_indices[i]];
	//		glColor3fv(glm::value_ptr(normal));

	//		glm::vec3 normalStart = _vertices[_indices[i]];
	//		glVertex3fv(glm::value_ptr(normalStart));
	//		glm::vec3 normalEnd = normalStart + normal*0.2f;
	//		glVertex3fv(glm::value_ptr(normalEnd));
	//	}

	//}
	//glEnd();
}

std::vector<XMFLOAT3>* Mesh::getVerticies()
{
	return &_vertices;
}

std::vector<DWORD>* Mesh::getVertextIndices()
{
	return &_indices;
}


ID3D12Resource* Mesh::CreateDefaultBuffer(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	const void* initData,
	UINT64 byteSize,
	ID3D12Resource*& uploadBuffer)
{
	ID3D12Resource* defaultBuffer;

	// Create the actual default buffer resource.
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&defaultBuffer)));

	// In order to copy CPU memory data into our default buffer, we need to create
	// an intermediate upload heap. 
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&uploadBuffer)));


	// Describe the data we want to copy into the default buffer.
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = byteSize;
	subResourceData.SlicePitch = subResourceData.RowPitch;

	// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
	// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
	// the intermediate upload heap data will be copied to mBuffer.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer,
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources<1>(cmdList, defaultBuffer, uploadBuffer, 0, 0, 1, &subResourceData);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer,
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

	// Note: uploadBuffer has to be kept alive after the above function calls because
	// the command list has not been executed yet that performs the actual copy.
	// The caller can Release the uploadBuffer after it knows the copy has been executed.


	return defaultBuffer;
}