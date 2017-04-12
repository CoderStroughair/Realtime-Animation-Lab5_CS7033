#pragma once
// Std. Includes
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <map>
using namespace std;

// Assimp includes
#include <assimp/cimport.h> // C importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations

// GL Includes
#include <GL/glew.h> // Contains all the necessery OpenGL includes
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Antons_maths_funcs.h"

struct Vertex {
	// Position
	glm::vec3 Position;
	// Normal
	glm::vec3 Normal;
	// TexCoords
	glm::vec2 TexCoords;
};

struct Texture {
	GLuint id;
	string type;
	aiString path;
};

struct vertexBoneData
{
	GLuint IDs[4];
	float Weights[4];

	void AddBoneData(GLuint BoneID, float Weight);
};

void vertexBoneData::AddBoneData(GLuint BoneID, float Weight)
{
	for (GLuint i = 0; i < 4; i++) {
		if (Weights[i] == 0.0) {
			IDs[i] = BoneID;
			Weights[i] = Weight;
			return;
		}
	}
	// should never get here - more bones than we have space for
	assert(0);
}


struct Bone
{
	string name;
	mat4 offsetMatrix;
	mat4 FinalTransformation;
};

class Node
{
public:
	string name;
	string parentName;
	vector<string> childnames;
	mat4 rotMatrix;
	mat4 parentOffsetMatrix;


	mat4 getTransformMatrix(map<string, GLuint> nodemap, vector<Node> nodes)
	{
		mat4 global = identity_mat4();
		if (this->parentName != "")
			global = nodes[nodemap.at(this->parentName)].getTransformMatrix(nodemap, nodes);
		return global * this->rotMatrix;
	}

	mat4 getGlobalTransform()
	{
		return parentOffsetMatrix * rotMatrix;
	}

	void updateLocation(map<string, GLuint> nodemap, vector<Node> nodes)
	{
		if (this->parentName != "")
			parentOffsetMatrix = nodes[nodemap.at(this->parentName)].getGlobalTransform();
		for (int i = 0; i <childnames.size(); i++)
		{
			if (childnames[i] == "Ganfaul")
				cout << endl;
			nodes[nodemap.at(childnames[i])].updateLocation(nodemap, nodes);
		}
	}

	void updateLocation(map<string, GLuint> nodemap, vector<Node> nodes, vector<mat4> &transforms)
	{
		if (this->parentName != "")
			parentOffsetMatrix = nodes[nodemap.at(this->parentName)].getGlobalTransform();
		else
			parentOffsetMatrix = identity_mat4();
		transforms.push_back(parentOffsetMatrix * rotMatrix);
		for each(string c in childnames)
		{
			nodes[nodemap.at(c)].updateLocation(nodemap, nodes, transforms);
		}
	}

	Node()
	{
		rotMatrix = identity_mat4();
		parentName = "";
		name = "";
		parentOffsetMatrix = identity_mat4();
	}
};

class Mesh {
public:
	/*  Mesh Data  */
	vector<Vertex> vertices;
	vector<Vertex> vertices_curr;
	vector<GLuint> indices;
	vector<Texture> textures;
	vector<Bone> bones;
	vector<vertexBoneData> vertexData;
	map<string, GLuint> boneMap;
	vector<Node> nodes;
	map<string, GLuint> nodeMap;


	/*  Functions  */
	// Constructor
	Mesh(vector<Vertex> vertices, vector<GLuint> indices, vector<Texture> textures)
	{
		this->vertices = vertices;
		this->vertices_curr = vertices;
		this->indices = indices;
		this->textures = textures;

		// Now that we have all the required data, set the vertex buffers and its attribute pointers.
		this->setupMesh();
	}

	Mesh(vector<Vertex> vertices, vector<GLuint> indices, vector<Texture> textures, vector<Bone> bones, vector<vertexBoneData> vertexData, vector<Node> nodes)
	{
		this->vertices = vertices;
		this->vertices_curr = vertices;
		this->indices = indices;
		this->textures = textures;
		this->bones = bones;
		this->vertexData = vertexData;
		this->nodes = nodes;

		for each(Node node in this->nodes)
		{
			if (nodeMap.find(node.name) == nodeMap.end())
			{
				GLuint index = nodeMap.size();
				nodeMap.insert(std::pair<string, GLuint>(node.name, index));
			}
		}

		// Now that we have all the required data, set the vertex buffers and its attribute pointers.
		this->setupMesh();
	}

	// Render the mesh
	void Draw(GLuint shaderID, const aiScene* scene)
	{
		// Bind appropriate textures
		GLuint diffuseNr = 1;
		GLuint specularNr = 1;
		for (GLuint i = 0; i < this->textures.size(); i++)
		{
			glActiveTexture(GL_TEXTURE0 + i); // Active proper texture unit before binding
											  // Retrieve texture number (the N in diffuse_textureN)
			stringstream ss;
			string number;
			string name = this->textures[i].type;
			//cout << textures[i].path.C_Str() << "\n";
			if (name == "texture_diffuse")
				ss << diffuseNr++; // Transfer GLuint to stream
			else if (name == "texture_specular")
				ss << specularNr++; // Transfer GLuint to stream
			number = ss.str();
			// Now set the sampler to the correct texture unit
			glUniform1i(glGetUniformLocation(shaderID, (name + number).c_str()), i);
			// And finally bind the texture
			glBindTexture(GL_TEXTURE_2D, this->textures[i].id);
		}
		vector<mat4> boneTransforms = GetBoneTransforms(0.03, scene);
		for (int i = 0; i < boneTransforms.size(); i++)
		{
			string name = "BoneTransforms[" + to_string(i) + "]";
			glUniformMatrix4fv(glGetUniformLocation(shaderID, name.c_str()), 1, GL_FALSE, boneTransforms[i].m);
		}
		//glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, model.m);
		glBindVertexArray(this->VAO);
		glDrawArrays(GL_TRIANGLES, 0, this->indices.size());
	}

	void SetAnimation(float AnimationTime, const aiNode* pNode, mat4 ParentTransform, const aiScene* scene)
	{
		string NodeName(pNode->mName.data);

		const aiAnimation* pAnimation = scene->mAnimations[0];

		mat4 NodeTransformation(convertMatrix(pNode->mTransformation));

		//const aiNodeAnim* pNodeAnim = FindNodeAnim(pAnimation, NodeName);

		//if (pNodeAnim) {
		//	// Interpolate scaling and generate scaling transformation matrix
		//	aiVector3D Scaling;
		//	CalcInterpolatedScaling(Scaling, AnimationTime, pNodeAnim);
		//	Matrix4f ScalingM;
		//	ScalingM.InitScaleTransform(Scaling.x, Scaling.y, Scaling.z);

		//	// Interpolate rotation and generate rotation transformation matrix
		//	aiQuaternion RotationQ;
		//	CalcInterpolatedRotation(RotationQ, AnimationTime, pNodeAnim);
		//	Matrix4f RotationM = Matrix4f(RotationQ.GetMatrix());

		//	// Interpolate translation and generate translation transformation matrix
		//	aiVector3D Translation;
		//	CalcInterpolatedPosition(Translation, AnimationTime, pNodeAnim);
		//	Matrix4f TranslationM;
		//	TranslationM.InitTranslationTransform(Translation.x, Translation.y, Translation.z);

		//	// Combine the above transformations
		//	NodeTransformation = TranslationM * RotationM * ScalingM;
		//}

		mat4 GlobalTransformation = ParentTransform * NodeTransformation;

		if (boneMap.find(NodeName) != boneMap.end()) {
			GLuint BoneIndex = boneMap[NodeName];
			bones[BoneIndex].FinalTransformation = convertMatrix(scene->mRootNode->mTransformation) * GlobalTransformation * bones[BoneIndex].offsetMatrix;
		}

		for (GLuint i = 0; i < pNode->mNumChildren; i++) {
			SetAnimation(AnimationTime, pNode->mChildren[i], GlobalTransformation, scene);
		}
	}

	vector<mat4>GetBoneTransforms(float TimeInSeconds, const aiScene* scene)
	{
		vector<mat4> transforms = vector<mat4>();
		transforms.resize(nodes.size());
		float TicksPerSecond = (float)(scene->mAnimations[0]->mTicksPerSecond != 0 ? scene->mAnimations[0]->mTicksPerSecond : 25.0f);
		float TimeInTicks = TimeInSeconds * TicksPerSecond;
		float AnimationTime = fmod(TimeInTicks, (float)scene->mAnimations[0]->mDuration);

		SetAnimation(AnimationTime, scene->mRootNode, identity_mat4(), scene);

		transforms.resize(bones.size());

		for (int i = 0; i < bones.size(); i++) {
			transforms[i] = bones[i].FinalTransformation;
		}
		return transforms;
	}

	/*  Render data  */
	GLuint VAO = NULL;

	/*  Functions    */
	// Initializes all the buffer objects/arrays
	void setupMesh()
	{
		// Create buffers/arrays
		GLuint VBO = NULL, EBO = NULL, BONE_VBO = NULL;
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);
		glGenBuffers(1, &BONE_VBO);
		glGenVertexArrays(1, &VAO);

		glBindVertexArray(this->VAO);
		// Load data into vertex buffers
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		// A great thing about structs is that their memory layout is sequential for all its items.
		// The effect is that we can simply pass a pointer to the struct and it translates perfectly to a glm::vec3/2 array which
		// again translates to 3/2 floats which translates to a byte array.
		glBufferData(GL_ARRAY_BUFFER, this->vertices_curr.size() * sizeof(Vertex), &this->vertices_curr[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indices.size() * sizeof(GLuint), &this->indices[0], GL_STATIC_DRAW);

		// Set the vertex attribute pointers
		// Vertex Positions
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
		// Vertex Normals
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, Normal));
		// Vertex Texture Coords
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, TexCoords));

		glBindBuffer(GL_ARRAY_BUFFER, BONE_VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData[0]) * vertexData.size(), &vertexData[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(3);
		glVertexAttribIPointer(3, 4, GL_INT, sizeof(vertexBoneData), (const GLvoid*)0);
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(vertexBoneData), (const GLvoid*)16);

		glBindVertexArray(0);
	}
};





