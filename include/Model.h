#pragma once

#include <glad/glad.h> 
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h"
#include "Shader.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>

using namespace std;

unsigned int TextureFromFile(const char* path, const string& directory, bool gamma = false);

class Model {
public:
    vector<MeshTexture> textures_loaded;
    vector<Mesh>    meshes;
    string directory;
    string name;
    bool gammaCorrection;

    Model(string const& path, bool gamma = false) : gammaCorrection(gamma) {
        loadModel(path);
    }

    void Draw(Shader& shader);

private:
    void loadModel(string const& path);
    void processNode(aiNode* node, const aiScene* scene);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
    vector<MeshTexture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName);
};