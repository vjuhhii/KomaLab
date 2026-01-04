#include "Model.h"
#include "stb_image.h"
#include <filesystem>
#include <algorithm> 
#include <sstream>
#include <set>

namespace fs = std::filesystem;

string ToLower(string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

vector<string> Tokenize(const string& str) {
    vector<string> tokens;
    string temp = ToLower(str);
    for (char& c : temp) {
        if (c == '_' || c == '-' || c == '.') c = ' ';
    }
    stringstream ss(temp);
    string token;
    while (ss >> token) {
        if (token.length() >= 2 && token != "geo" && token != "mesh" && token != "poly" && token != "shape" && token != "model" && token != "material") {
            tokens.push_back(token);
        }
    }
    return tokens;
}

int CalculateScore(const string& filename, const vector<string>& meshTokens, const vector<string>& materialTokens, const vector<string>& modelTokens) {
    int score = 0;
    string lowerFilename = ToLower(filename);

    for (const string& token : meshTokens) {
        if (lowerFilename.find(token) != string::npos) score += 100;
    }

    for (const string& token : materialTokens) {
        if (lowerFilename.find(token) != string::npos) score += 50;
    }

    for (const string& token : modelTokens) {
        if (lowerFilename.find(token) != string::npos) score += 20;
    }

    return score;
}

void Model::Draw(Shader& shader) {
    for (unsigned int i = 0; i < meshes.size(); i++)
        meshes[i].Draw(shader);
}

void Model::loadModel(string const& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
        return;
    }

    directory = path.substr(0, path.find_last_of('/'));
    if (directory == path) directory = path.substr(0, path.find_last_of('\\'));

    fs::path p(path);
    this->name = p.stem().string();

    processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode* node, const aiScene* scene) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene);
    }
}

unsigned int CreateFallbackTexture() {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    unsigned char data[] = { 128, 128, 128 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return textureID;
}

vector<MeshTexture> ScanBestTextures(const string& dir, const string& meshName, const string& matName, const string& modelName, vector<MeshTexture>& loaded_cache, bool forceLoad) {
    vector<MeshTexture> results;
    if (!fs::exists(dir)) return results;

    vector<string> meshTokens = Tokenize(meshName);
    vector<string> matTokens = Tokenize(matName);
    vector<string> modelTokens = Tokenize(modelName);

    map<string, string> bestFileForType;
    map<string, int> bestScoreForType;

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        string filename = entry.path().filename().string();
        string lowerFilename = ToLower(filename);
        string ext = entry.path().extension().string();
        if (ToLower(ext) != ".jpg" && ToLower(ext) != ".png" && ToLower(ext) != ".tga") continue;

        string type = "";
        if (lowerFilename.find("basecolor") != string::npos || lowerFilename.find("diffuse") != string::npos || lowerFilename.find("albedo") != string::npos || lowerFilename.find("color") != string::npos) {
            type = "texture_diffuse";
        }
        else if (lowerFilename.find("normal") != string::npos) {
            type = "texture_normal";
        }
        else if (lowerFilename.find("metallic") != string::npos || lowerFilename.find("roughness") != string::npos || lowerFilename.find("specular") != string::npos) {
            type = "texture_specular";
        }

        if (type.empty()) continue;

        int score = CalculateScore(filename, meshTokens, matTokens, modelTokens);

        if (forceLoad && score == 0) score = 1;

        if (score > 0) score += 1;

        if (score > 0 && score > bestScoreForType[type]) {
            bestScoreForType[type] = score;
            bestFileForType[type] = filename;
        }
    }

    for (auto const& [type, filename] : bestFileForType) {
        bool loaded = false;
        for (const auto& t : loaded_cache) {
            if (std::strcmp(t.path.c_str(), filename.c_str()) == 0) {
                results.push_back(t);
                loaded = true;
                break;
            }
        }

        if (!loaded) {
            cout << "[AUTO-MATCH] Best fit for " << meshName << " (" << type << "): " << filename << " (Score: " << bestScoreForType[type] << ")" << endl;
            MeshTexture tex;
            tex.id = TextureFromFile(filename.c_str(), dir, false);
            tex.type = type;
            tex.path = filename;
            results.push_back(tex);
            loaded_cache.push_back(tex);
        }
    }

    return results;
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene) {
    vector<Vertex> vertices;
    vector<unsigned int> indices;
    vector<MeshTexture> textures;

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        glm::vec3 vector;
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.Position = vector;
        if (mesh->HasNormals()) {
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.Normal = vector;
        }
        if (mesh->mTextureCoords[0]) {
            glm::vec2 vec;
            vec.x = mesh->mTextureCoords[0][i].x;
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex.TexCoords = vec;
            if (mesh->HasTangentsAndBitangents()) {
                vector.x = mesh->mTangents[i].x;
                vector.y = mesh->mTangents[i].y;
                vector.z = mesh->mTangents[i].z;
                vertex.Tangent = vector;
                vector.x = mesh->mBitangents[i].x;
                vector.y = mesh->mBitangents[i].y;
                vector.z = mesh->mBitangents[i].z;
                vertex.Bitangent = vector;
            }
        }
        else {
            vertex.TexCoords = glm::vec2(0.0f, 0.0f);
        }
        vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    aiString matName;
    material->Get(AI_MATKEY_NAME, matName);

    vector<MeshTexture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    if (diffuseMaps.empty()) {
        vector<MeshTexture> baseColor = loadMaterialTextures(material, aiTextureType_BASE_COLOR, "texture_diffuse");
        textures.insert(textures.end(), baseColor.begin(), baseColor.end());
    }
    vector<MeshTexture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    vector<MeshTexture> normalMaps = loadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal");
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

    if (textures.empty()) {
        string currentMeshName = mesh->mName.C_Str();
        string currentMatName = matName.C_Str();

        if (fs::path(this->directory).has_parent_path()) {
            string texDir = (fs::path(this->directory).parent_path() / "textures").string();
            vector<MeshTexture> found = ScanBestTextures(texDir, currentMeshName, currentMatName, this->name, this->textures_loaded, true);
            textures.insert(textures.end(), found.begin(), found.end());
        }

        if (textures.empty()) {
            vector<MeshTexture> found = ScanBestTextures(this->directory, currentMeshName, currentMatName, this->name, this->textures_loaded, false);
            textures.insert(textures.end(), found.begin(), found.end());
        }
    }

    if (textures.empty()) {
        MeshTexture fallback;
        fallback.id = CreateFallbackTexture();
        fallback.type = "texture_diffuse";
        fallback.path = "fallback_grey";
        textures.push_back(fallback);
    }

    return Mesh(vertices, indices, textures);
}

string FindTexturePath(const string& rawPath, const string& modelDir) {
    fs::path target(rawPath);
    string filename = target.filename().string();
    fs::path p1 = fs::path(modelDir) / rawPath;
    if (fs::exists(p1)) return p1.string();
    fs::path p2 = fs::path(modelDir) / filename;
    if (fs::exists(p2)) return p2.string();
    if (fs::path(modelDir).has_parent_path()) {
        fs::path p3 = fs::path(modelDir).parent_path() / "textures" / filename;
        if (fs::exists(p3)) return p3.string();
    }
    if (fs::exists("assets/" + filename)) return "assets/" + filename;
    return (fs::path(modelDir) / rawPath).string();
}

vector<MeshTexture> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName) {
    vector<MeshTexture> textures;
    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);

        bool skip = false;
        for (unsigned int j = 0; j < textures_loaded.size(); j++) {
            if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
                textures.push_back(textures_loaded[j]);
                skip = true;
                break;
            }
        }

        if (!skip) {
            MeshTexture texture;
            string smartPath = FindTexturePath(str.C_Str(), this->directory);
            texture.id = TextureFromFile(smartPath.c_str(), "", false);
            texture.type = typeName;
            texture.path = str.C_Str();
            textures.push_back(texture);
            textures_loaded.push_back(texture);
        }
    }
    return textures;
}

unsigned int TextureFromFile(const char* path, const string& directory, bool gamma) {
    string filename = string(path);
    if (!directory.empty())
        filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);

    if (data) {
        GLenum format;
        if (nrComponents == 1) format = GL_RED;
        else if (nrComponents == 3) format = GL_RGB;
        else if (nrComponents == 4) format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else {
        stbi_image_free(data);
    }

    return textureID;
}