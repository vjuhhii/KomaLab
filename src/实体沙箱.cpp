#include <iostream>
#include <vector> 
#include <string>
#include <map>
#include <cstdint>
#if defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "Shader.h"
#include "Camera.h"
#include "Texture.h"
#include "PhysicsScene.h"
#include "Model.h" 

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

unsigned int SCR_WIDTH = 800;
unsigned int SCR_HEIGHT = 600;
const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

Camera camera(glm::vec3(0.0f, 2.0f, 8.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

glm::vec3 lightPos(-2.0f, 6.0f, -1.0f);
PhysicsScene scene;

bool isMenuOpen = false;
bool isTabPressed = false;

std::string selectedTexturePath = "assets/floor.jpg";
unsigned int selectedTextureID = 0;
std::map<std::string, Texture*> loadedTextures;

std::string selectedModelPath = "Cube (Default)";
int selectedModelIndex = -1;
std::vector<Model*> loadedModels;
std::map<std::string, int> modelCache;

bool isSpacePressed = false;
bool isMousePressed = false;
bool isBPressed = false;
bool isRPressed = false;

// returns texture ID
unsigned int GetTexture(const std::string& path) {
    if (loadedTextures.find(path) != loadedTextures.end()) {
        return loadedTextures[path]->ID;
    }
    Texture* newTex = new Texture(path.c_str());
    loadedTextures[path] = newTex;
    return newTex->ID;
}
// returns model ID
int GetModelID(const std::string& path) {
    if (path == "Cube (Default)") return -1;
    if (modelCache.find(path) != modelCache.end()) return modelCache[path];
    Model* newModel = new Model(path);
    loadedModels.push_back(newModel);
    int newIndex = (int)loadedModels.size() - 1;
    modelCache[path] = newIndex;
    return newIndex;
}
// scans a directory for files with given extensions
std::vector<std::string> ScanFiles(const std::string& directory, const std::vector<std::string>& extensions) {
    std::vector<std::string> files;
    if (fs::exists(directory)) {
        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                for (const auto& reqExt : extensions) {
                    if (ext == reqExt) {
                        files.push_back(entry.path().string());
                        break;
                    }
                }
            }
        }
    }
    return files;
}
// floor rendering function
void RenderFloor(Shader& shader, unsigned int planeVAO, unsigned int floorTexID) {
    shader.setInt("texture1", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, floorTexID);
    glm::mat4 model = glm::mat4(1.0f);
    shader.setMat4("model", model);
    glBindVertexArray(planeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    SCR_WIDTH = mode->width;
    SCR_HEIGHT = mode->height;
    lastX = SCR_WIDTH / 2.0f;
    lastY = SCR_HEIGHT / 2.0f;
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "k", primaryMonitor, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::StyleColorsDark();
    glEnable(GL_DEPTH_TEST);
    Shader lightingShader("shaders/lighting.vert", "shaders/lighting.frag");
    Shader simpleDepthShader("shaders/simpleDepthShader.vert", "shaders/simpleDepthShader.frag");
    Shader lampShader("shaders/lighting.vert", "shaders/lamp.frag");
    // load default floor texture
    unsigned int floorID = GetTexture("assets/floor.jpg");
    selectedTextureID = GetTexture(selectedTexturePath);
    // assets scanning
    std::vector<std::string> textureFiles = ScanFiles("assets", { ".jpg", ".png", ".jpeg" });
    std::vector<std::string> modelFiles = ScanFiles("assets", { ".obj", ".fbx", ".gltf" });
    modelFiles.insert(modelFiles.begin(), "Cube (Default)");
    // shadow map FBO setup
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // plane vertices
    float planeVertices[] = {
         25.0f, -2.0f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
        -25.0f, -2.0f,  25.0f,  0.0f, 1.0f, 0.0f,   0.0f,  0.0f,
        -25.0f, -2.0f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,
         25.0f, -2.0f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
        -25.0f, -2.0f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,
         25.0f, -2.0f, -25.0f,  0.0f, 1.0f, 0.0f,  25.0f, 25.0f
    };
    // cube vertices
    float cubeVertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f, 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f, -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f, 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f, -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f, -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f, -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f, -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f, 0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f, 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f, 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
        0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f, -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f, -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
        0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f, -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f, -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f
    };
    // plane setup
    unsigned int planeVBO, planeVAO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
    // cube setup
    unsigned int cubeVBO, cubeVAO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
    // light cube setup
    unsigned int lightVAO;
    glGenVertexArrays(1, &lightVAO);
    glBindVertexArray(lightVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    lightingShader.use();
    lightingShader.setInt("shadowMap", 10);
    // spawn initial object
    scene.SpawnObject(glm::vec3(0.0f, 3.0f, 0.0f), selectedTextureID, -1);
    // main loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        // input    
        processInput(window);
        // imGui frame start
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (isMenuOpen) {
            ImGui::Begin("Spawner Menu");
            ImGui::Text("Select Model:");
            if (ImGui::BeginCombo("##model", selectedModelPath.c_str())) {
                for (const auto& file : modelFiles) {
                    bool isSelected = (selectedModelPath == file);
                    if (ImGui::Selectable(file.c_str(), isSelected)) {
                        selectedModelPath = file;
                        selectedModelIndex = GetModelID(file);
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (selectedModelIndex == -1) {
                ImGui::Separator();
                ImGui::Text("Select Texture (for standard Cube):");
                if (ImGui::BeginCombo("##texture", selectedTexturePath.c_str())) {
                    for (const auto& file : textureFiles) {
                        bool isSelected = (selectedTexturePath == file);
                        if (ImGui::Selectable(file.c_str(), isSelected)) {
                            selectedTexturePath = file;
                            selectedTextureID = GetTexture(file);
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                // --
                ImTextureID imTexID = (ImTextureID)(intptr_t)selectedTextureID;
                ImGui::Image(imTexID, ImVec2(100.0f, 100.0f));
                // --
            }
            else {
                ImGui::Separator();
                ImGui::Text("Material: Embedded in Model");
                ImGui::TextDisabled("(Texture selection disabled for imported models)");
            }
            
            ImGui::Separator();
            if (ImGui::Button("SPAWN (B)", ImVec2(200, 50))) {
                unsigned int texIDToUse = (selectedModelIndex == -1) ? selectedTextureID : 0;
                scene.SpawnObject(camera.Position + camera.Front * 3.0f, texIDToUse, selectedModelIndex);
            }
            if (ImGui::Button("CLEAR ALL (R)")) {
                scene.Clear();
            }
            ImGui::End();
        }
        if (scene.HeldObject) scene.HeldObject->UpdateDragPosition(camera.Position, camera.Front);
        scene.Update(deltaTime);
        // shadow pass
        glm::mat4 lightProjection = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 1.0f, 50.0f);
        glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;
        simpleDepthShader.use();
        simpleDepthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glm::mat4 model = glm::mat4(1.0f);
        simpleDepthShader.setMat4("model", model);
        glBindVertexArray(planeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        for (auto& obj : scene.Objects) {
            model = obj.GetModelMatrix();
            if (obj.ModelID >= 0) model = glm::scale(model, glm::vec3(0.2f));
            simpleDepthShader.setMat4("model", model);
            if (obj.ModelID >= 0 && obj.ModelID < loadedModels.size()) {
                loadedModels[obj.ModelID]->Draw(simpleDepthShader);
            }
            else {
                glBindVertexArray(cubeVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // lightning pass
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        lightingShader.use();
        lightingShader.setVec3("lightColor", 1.0f, 1.0f, 1.0f);
        lightingShader.setVec3("lightPos", lightPos);
        lightingShader.setVec3("viewPos", camera.Position);
        lightingShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, depthMap);

        // floor render
        lightingShader.setBool("useMeshMaterial", false);
        RenderFloor(lightingShader, planeVAO, floorID);

        for (auto& obj : scene.Objects) {
            glm::mat4 model = obj.GetModelMatrix();

            if (obj.ModelID == -1) {
                // cube
                lightingShader.setBool("useMeshMaterial", false);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, obj.TextureID);

                lightingShader.setMat4("model", model);
                glBindVertexArray(cubeVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
            else {
                // model
                lightingShader.setBool("useMeshMaterial", true);
                model = glm::scale(model, glm::vec3(0.2f));
                lightingShader.setMat4("model", model);

                if (obj.ModelID >= 0 && obj.ModelID < loadedModels.size()) {
                    loadedModels[obj.ModelID]->Draw(lightingShader);
                }
            }
        }
        lampShader.use();
        lampShader.setMat4("projection", projection);
        lampShader.setMat4("view", view);
        model = glm::mat4(1.0f);
        model = glm::translate(model, lightPos);
        model = glm::scale(model, glm::vec3(0.2f));
        lampShader.setMat4("model", model);
        glBindVertexArray(lightVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
// input processing
void processInput(GLFWwindow* window) {
    // Escape: close window
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
        // Tab: toggle menu
        if (!isTabPressed) {
            isMenuOpen = !isMenuOpen;
            if (isMenuOpen) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
            isTabPressed = true;
        }
    }
    else {
        isTabPressed = false;
    }
    if (isMenuOpen) return;
    // camera movement (WASD)
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime);
    // B: spawn object
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) {
        if (!isBPressed) {
            unsigned int texIDToUse = (selectedModelIndex == -1) ? selectedTextureID : 0;
            scene.SpawnObject(camera.Position + camera.Front * 3.0f, texIDToUse, selectedModelIndex);
            isBPressed = true;
        }
    }
    else isBPressed = false;
    // R: clear all objects
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        if (!isRPressed) { scene.Clear(); isRPressed = true; }
    }
    else isRPressed = false;
    // Space: throw object
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (!isSpacePressed) { scene.ProcessThrow(camera.Front * 15.0f); isSpacePressed = true; }
    }
    else isSpacePressed = false;
    // LMB: grab/release object
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        if (!isMousePressed) { scene.ProcessGrab(camera.Position, camera.Front); isMousePressed = true; }
    }
    else { scene.ProcessRelease(); isMousePressed = false; }
}
// scroll callback for rotating held object
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (isMenuOpen) return;
    bool shiftPressed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
    scene.RotateHeldObject((float)yoffset, shiftPressed);
}
void framebuffer_size_callback(GLFWwindow* window, int width, int height) { glViewport(0, 0, width, height); }
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    if (isMenuOpen) return;
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}