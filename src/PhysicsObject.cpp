#include "PhysicsObject.h"
#include <glm/gtc/random.hpp>
#include <algorithm>
#include <vector>
#include <iostream>
#include <limits>

void ProjectVertices(const std::vector<glm::vec3>& vertices, const glm::vec3& axis, float& min, float& max) {
    min = std::numeric_limits<float>::max();
    max = std::numeric_limits<float>::lowest();
    for (const auto& v : vertices) {
        float proj = glm::dot(v, axis);
        if (proj < min) min = proj;
        if (proj > max) max = proj;
    }
}

bool FindOverlap(const std::vector<glm::vec3>& vertsA, const std::vector<glm::vec3>& vertsB, const glm::vec3& axis, float& overlap) {
    float minA, maxA, minB, maxB;
    ProjectVertices(vertsA, axis, minA, maxA);
    ProjectVertices(vertsB, axis, minB, maxB);

    if (minA >= maxB || minB >= maxA) return false;

    float overlap1 = maxB - minA;
    float overlap2 = maxA - minB;
    overlap = std::min(overlap1, overlap2);
    return true;
}

PhysicsObject::PhysicsObject(glm::vec3 startPos, unsigned int texID, int modelID, float mass)
    : Position(startPos), Velocity(0.0f), AngularVelocity(0.0f),
    TextureID(texID), ModelID(modelID), Mass(mass),
    Orientation(glm::mat4(1.0f)), IsDragging(false), IsSleeping(false),
    Friction(0.4f), Restitution(0.2f), SleepTimer(0.0f)
{
    Dimensions = glm::vec3(0.5f);

    if (Mass > 0.0f) {
        InvMass = 1.0f / Mass;
        IsStatic = false;

        glm::vec3 size = Dimensions * 2.0f;
        float x2 = size.x * size.x;
        float y2 = size.y * size.y;
        float z2 = size.z * size.z;

        InertiaTensor = glm::mat3(0.0f);
        InertiaTensor[0][0] = (1.0f / 12.0f) * Mass * (y2 + z2);
        InertiaTensor[1][1] = (1.0f / 12.0f) * Mass * (x2 + z2);
        InertiaTensor[2][2] = (1.0f / 12.0f) * Mass * (x2 + y2);

        InverseInertiaTensor = glm::inverse(InertiaTensor);
    }
    else {
        InvMass = 0.0f;
        IsStatic = true;
        InverseInertiaTensor = glm::mat3(0.0f);
        InertiaTensor = glm::mat3(0.0f);
    }

    UpdateInertia();
}

void PhysicsObject::UpdateInertia() {
    if (IsStatic) {
        WorldInvInertia = glm::mat3(0.0f);
    }
    else {
        glm::mat3 rot = glm::mat3(Orientation);
        WorldInvInertia = rot * InverseInertiaTensor * glm::transpose(rot);
    }
}

void PhysicsObject::Update(float deltaTime, glm::vec3 gravity) {
    if (IsStatic || IsDragging) {
        IsSleeping = false;
        SleepTimer = 0.0f;
        UpdateInertia();
        return;
    }

    float speed = glm::length(Velocity);
    float angSpeed = glm::length(AngularVelocity);

    if (speed < 0.15f && angSpeed < 0.2f) {
        SleepTimer += deltaTime;
        if (SleepTimer > 0.5f) {
            IsSleeping = true;
            Velocity = glm::vec3(0.0f);
            AngularVelocity = glm::vec3(0.0f);
            return;
        }
    }
    else {
        SleepTimer = 0.0f;
        IsSleeping = false;
    }

    Velocity += gravity * deltaTime;

    float damping = std::pow(0.9f, deltaTime);
    Velocity *= damping;
    AngularVelocity *= damping;

    Position += Velocity * deltaTime;

    if (glm::length(AngularVelocity) > 0.0001f) {
        glm::vec3 axis = glm::normalize(AngularVelocity);
        float angle = glm::length(AngularVelocity) * deltaTime;
        Orientation = glm::rotate(glm::mat4(1.0f), angle, axis) * Orientation;

        glm::vec3 r = glm::normalize(glm::vec3(Orientation[0]));
        glm::vec3 u = glm::normalize(glm::vec3(Orientation[1]));
        glm::vec3 f = glm::normalize(glm::cross(r, u));
        u = glm::cross(f, r);
        Orientation[0] = glm::vec4(r, 0.0f);
        Orientation[1] = glm::vec4(u, 0.0f);
        Orientation[2] = glm::vec4(f, 0.0f);
    }

    UpdateInertia();
}

std::vector<glm::vec3> PhysicsObject::GetWorldCorners() {
    std::vector<glm::vec3> corners;
    glm::mat4 model = GetModelMatrix();
    glm::vec3 h = Dimensions;

    corners.push_back(glm::vec3(model * glm::vec4(h.x, h.y, h.z, 1.0f)));
    corners.push_back(glm::vec3(model * glm::vec4(h.x, h.y, -h.z, 1.0f)));
    corners.push_back(glm::vec3(model * glm::vec4(h.x, -h.y, h.z, 1.0f)));
    corners.push_back(glm::vec3(model * glm::vec4(h.x, -h.y, -h.z, 1.0f)));
    corners.push_back(glm::vec3(model * glm::vec4(-h.x, h.y, h.z, 1.0f)));
    corners.push_back(glm::vec3(model * glm::vec4(-h.x, h.y, -h.z, 1.0f)));
    corners.push_back(glm::vec3(model * glm::vec4(-h.x, -h.y, h.z, 1.0f)));
    corners.push_back(glm::vec3(model * glm::vec4(-h.x, -h.y, -h.z, 1.0f)));
    return corners;
}

std::vector<glm::vec3> GetIncidentFace(PhysicsObject* incBody, const glm::vec3& normal) {
    glm::vec3 incNormal = glm::vec3(incBody->Orientation * glm::transpose(glm::mat4(incBody->Orientation)) * glm::vec4(normal, 0.0f));

    glm::mat3 rot = glm::mat3(incBody->Orientation);
    glm::vec3 axes[] = { rot[0], rot[1], rot[2] };

    int bestFaceIndex = -1;
    float minDot = std::numeric_limits<float>::max();

    glm::vec3 faceNormals[] = {
         axes[0], -axes[0],
         axes[1], -axes[1],
         axes[2], -axes[2]
    };

    for (int i = 0; i < 6; i++) {
        float d = glm::dot(normal, faceNormals[i]);
        if (d < minDot) {
            minDot = d;
            bestFaceIndex = i;
        }
    }

    glm::vec3 h = incBody->Dimensions;
    std::vector<glm::vec3> faceVerts;

    switch (bestFaceIndex) {
    case 0: faceVerts = { {h.x, h.y, h.z}, {h.x,-h.y, h.z}, {h.x,-h.y,-h.z}, {h.x, h.y,-h.z} }; break;
    case 1: faceVerts = { {-h.x, h.y,-h.z}, {-h.x,-h.y,-h.z}, {-h.x,-h.y, h.z}, {-h.x, h.y, h.z} }; break;
    case 2: faceVerts = { {-h.x, h.y, h.z}, {-h.x, h.y,-h.z}, { h.x, h.y,-h.z}, { h.x, h.y, h.z} }; break;
    case 3: faceVerts = { { h.x,-h.y, h.z}, { h.x,-h.y,-h.z}, {-h.x,-h.y,-h.z}, {-h.x,-h.y, h.z} }; break;
    case 4: faceVerts = { {-h.x, h.y, h.z}, { h.x, h.y, h.z}, { h.x,-h.y, h.z}, {-h.x,-h.y, h.z} }; break;
    case 5: faceVerts = { { h.x, h.y,-h.z}, {-h.x, h.y,-h.z}, {-h.x,-h.y,-h.z}, { h.x,-h.y,-h.z} }; break;
    }

    for (auto& v : faceVerts) v = glm::vec3(incBody->GetModelMatrix() * glm::vec4(v, 1.0f));
    return faceVerts;
}

std::vector<glm::vec3> Clip(const std::vector<glm::vec3>& subjectPoly, const glm::vec3& clipNormal, float clipOffset) {
    std::vector<glm::vec3> outputPoly;
    if (subjectPoly.empty()) return outputPoly;

    glm::vec3 v1 = subjectPoly.back();
    for (const auto& v2 : subjectPoly) {
        float d1 = glm::dot(clipNormal, v1) - clipOffset;
        float d2 = glm::dot(clipNormal, v2) - clipOffset;

        if (d1 <= 0 && d2 <= 0) {
            outputPoly.push_back(v2);
        }
        else if (d1 <= 0 && d2 > 0) {
            float t = d1 / (d1 - d2);
            outputPoly.push_back(v1 + t * (v2 - v1));
        }
        else if (d1 > 0 && d2 <= 0) {
            float t = d1 / (d1 - d2);
            outputPoly.push_back(v1 + t * (v2 - v1));
            outputPoly.push_back(v2);
        }
        v1 = v2;
    }
    return outputPoly;
}

Manifold PhysicsObject::GenerateManifold(PhysicsObject& other) {
    Manifold m;
    m.BodyA = this;
    m.BodyB = &other;
    m.IsColliding = false;

    std::vector<glm::vec3> vertsA = this->GetWorldCorners();
    std::vector<glm::vec3> vertsB = other.GetWorldCorners();

    glm::mat3 rotA = glm::mat3(this->Orientation);
    glm::mat3 rotB = glm::mat3(other.Orientation);

    glm::vec3 axes[] = {
        rotA[0], rotA[1], rotA[2],
        rotB[0], rotB[1], rotB[2],
        glm::cross(rotA[0], rotB[0]), glm::cross(rotA[0], rotB[1]), glm::cross(rotA[0], rotB[2]),
        glm::cross(rotA[1], rotB[0]), glm::cross(rotA[1], rotB[1]), glm::cross(rotA[1], rotB[2]),
        glm::cross(rotA[2], rotB[0]), glm::cross(rotA[2], rotB[1]), glm::cross(rotA[2], rotB[2])
    };

    float minOverlap = std::numeric_limits<float>::max();
    glm::vec3 bestAxis;

    for (const auto& axis : axes) {
        if (glm::length(axis) < 0.001f) continue;
        glm::vec3 n = glm::normalize(axis);
        float overlap;

        if (!FindOverlap(vertsA, vertsB, n, overlap)) {
            return m;
        }

        if (overlap < minOverlap) {
            minOverlap = overlap;
            bestAxis = n;
        }
    }

    m.IsColliding = true;

    glm::vec3 d = other.Position - this->Position;
    if (glm::dot(bestAxis, d) < 0) bestAxis = -bestAxis;
    m.Normal = bestAxis;

    PhysicsObject* refBody = this;
    PhysicsObject* incBody = &other;

    std::vector<glm::vec3> incidentPoly = GetIncidentFace(incBody, -m.Normal);

    glm::mat3 refRot = glm::mat3(refBody->Orientation);
    glm::vec3 refAxes[] = { refRot[0], refRot[1], refRot[2] };

    int refAxisIdx = 0;
    float maxDot = 0.0f;
    for (int i = 0; i < 3; i++) {
        float d = std::abs(glm::dot(m.Normal, refAxes[i]));
        if (d > maxDot) { maxDot = d; refAxisIdx = i; }
    }

    glm::vec3 refNormal = refAxes[refAxisIdx];
    if (glm::dot(refNormal, m.Normal) < 0) refNormal = -refNormal;

    glm::vec3 refPos = refBody->Position + refNormal * refBody->Dimensions[refAxisIdx];

    std::vector<glm::vec3> clipAxes;
    for (int i = 0; i < 3; i++) if (i != refAxisIdx) clipAxes.push_back(refAxes[i]);

    std::vector<glm::vec3> clippedPoly = incidentPoly;

    for (const auto& ax : clipAxes) {
        float dist = refBody->Dimensions[(ax == refAxes[0]) ? 0 : (ax == refAxes[1] ? 1 : 2)];
        clippedPoly = Clip(clippedPoly, ax, glm::dot(refBody->Position, ax) + dist);
        clippedPoly = Clip(clippedPoly, -ax, -glm::dot(refBody->Position, ax) + dist);
    }

    float refOffset = glm::dot(refNormal, refBody->Position) + refBody->Dimensions[refAxisIdx];

    for (const auto& v : clippedPoly) {
        ContactPoint cp;
        cp.Position = v;
        cp.Penetration = minOverlap;
        m.Contacts.push_back(cp);
    }

    if (m.Contacts.empty()) {
        ContactPoint cp;
        cp.Position = this->Position + m.Normal * minOverlap;
        cp.Penetration = minOverlap;
        m.Contacts.push_back(cp);
    }

    return m;
}

glm::mat4 PhysicsObject::GetModelMatrix() {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, Position);
    model = model * Orientation;
    return model;
}

void PhysicsObject::ManualRotate(float amount, bool axisMode) {
    IsSleeping = false; SleepTimer = 0.0f;
    float angle = glm::radians(5.0f) * amount;
    glm::vec3 axis = axisMode ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
    if (axisMode) Orientation = Orientation * glm::rotate(glm::mat4(1.0f), angle, axis);
    else Orientation = glm::rotate(glm::mat4(1.0f), angle, axis) * Orientation;
    UpdateInertia();
}

bool PhysicsObject::TryGrab(glm::vec3 cameraPos, glm::vec3 cameraFront) {
    float dist = glm::distance(cameraPos, Position);
    glm::vec3 dirToObj = glm::normalize(Position - cameraPos);
    float dotProd = glm::dot(cameraFront, dirToObj);
    if (dotProd > 0.95f && dist < 10.0f) {
        IsDragging = true;
        IsSleeping = false;
        HoldDistance = dist;
        Velocity = glm::vec3(0.0f);
        AngularVelocity = glm::vec3(0.0f);
        return true;
    }
    return false;
}

void PhysicsObject::UpdateDragPosition(glm::vec3 cameraPos, glm::vec3 cameraFront) {
    if (IsDragging) {
        IsSleeping = false;
        glm::vec3 targetPos = cameraPos + (cameraFront * HoldDistance);
        glm::vec3 force = (targetPos - Position) * 10.0f;
        Velocity = force;
        Position += Velocity * 0.016f;
        AngularVelocity *= 0.9f;
    }
}

void PhysicsObject::Release() {
    IsDragging = false;
    IsSleeping = false;
}

void PhysicsObject::ApplyImpulse(glm::vec3 impulse, glm::vec3 contactPoint) {
    if (IsStatic || IsDragging) return;
    IsSleeping = false; SleepTimer = 0.0f;

    Velocity += impulse * InvMass;

    glm::vec3 r = contactPoint - Position;
    AngularVelocity += WorldInvInertia * glm::cross(r, impulse);
}

void PhysicsObject::ApplyLinearImpulse(glm::vec3 impulse) {
    if (IsStatic || IsDragging) return;
    IsSleeping = false; SleepTimer = 0.0f;
    Velocity += impulse * InvMass;
}