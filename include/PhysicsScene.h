#ifndef PHYSICS_SCENE_H
#define PHYSICS_SCENE_H

#include <vector>
#include "PhysicsObject.h"

class PhysicsScene {
public:
    std::vector<PhysicsObject> Objects;
    PhysicsObject* HeldObject;

    PhysicsScene();

    void SpawnObject(glm::vec3 position, unsigned int textureID, int modelID = -1);
    void Clear();

    void Update(float deltaTime);

    void ProcessGrab(glm::vec3 cameraPos, glm::vec3 cameraFront);
    void ProcessRelease();
    void ProcessThrow(glm::vec3 force);
    void RotateHeldObject(float amount, bool axisMode);
    void ExplodeAll();

private:
    glm::vec3 Gravity;

    void ResolveManifold(Manifold& m);
};

#endif