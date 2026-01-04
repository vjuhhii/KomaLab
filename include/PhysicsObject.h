#ifndef PHYSICS_OBJECT_H
#define PHYSICS_OBJECT_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cmath>

class PhysicsObject;

struct ContactPoint {
    glm::vec3 Position;
    float Penetration;
    float NormalImpulse;
	float TangentImpulse1; 
	float TangentImpulse2;
};

struct Manifold {
    PhysicsObject* BodyA;
    PhysicsObject* BodyB;
    glm::vec3 Normal;
    std::vector<ContactPoint> Contacts;
    bool IsColliding;
};

class PhysicsObject {
public:
    glm::vec3 Position;
    glm::vec3 Velocity;

    glm::mat4 Orientation;
    glm::vec3 AngularVelocity;

	// physics properties
    float Mass;
    float InvMass;

    glm::mat3 InertiaTensor;
    glm::mat3 InverseInertiaTensor;
    glm::mat3 WorldInvInertia;

    float Friction;
    float Restitution;

    glm::vec3 Dimensions;

	// graphics properties
    unsigned int TextureID;
    int ModelID;

    bool IsDragging;
    float HoldDistance;
    bool IsStatic;

    float SleepTimer;
    bool IsSleeping;

    PhysicsObject(glm::vec3 startPos, unsigned int texID, int modelID = -1, float mass = 1.0f);

    void Update(float deltaTime, glm::vec3 gravity);

    void UpdateInertia();

    Manifold GenerateManifold(PhysicsObject& other);

    bool TryGrab(glm::vec3 cameraPos, glm::vec3 cameraFront);
    void UpdateDragPosition(glm::vec3 cameraPos, glm::vec3 cameraFront);
    void Release();
    void ApplyImpulse(glm::vec3 force, glm::vec3 contactPoint);
    void ApplyLinearImpulse(glm::vec3 impulse);

    glm::mat4 GetModelMatrix();
    void ManualRotate(float amount, bool axisMode);
    std::vector<glm::vec3> GetWorldCorners();
};

#endif