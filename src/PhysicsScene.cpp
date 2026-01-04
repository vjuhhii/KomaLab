#include "PhysicsScene.h"
#include <algorithm>
#include <iostream>
#include <glm/gtc/random.hpp>

PhysicsScene::PhysicsScene() : HeldObject(nullptr), Gravity(glm::vec3(0.0f, -9.81f, 0.0f)) {
}
void PhysicsScene::SpawnObject(glm::vec3 position, unsigned int textureID, int modelID) {
    Objects.emplace_back(position, textureID, modelID, 1.0f);
}
void PhysicsScene::Clear() {
    Objects.clear();
    HeldObject = nullptr;
}
void PhysicsScene::Update(float deltaTime) {
    if (deltaTime > 0.05f) deltaTime = 0.05f;

    int subSteps = 8;
    float subDt = deltaTime / (float)subSteps;

    for (int step = 0; step < subSteps; ++step) {
        for (auto& obj : Objects) {
            obj.Update(subDt, Gravity);
        }

        std::vector<Manifold> manifolds;
        for (size_t i = 0; i < Objects.size(); ++i) {
            for (size_t j = i + 1; j < Objects.size(); ++j) {
                if ((Objects[i].IsStatic || Objects[i].IsSleeping) &&
                    (Objects[j].IsStatic || Objects[j].IsSleeping)) continue;

                Manifold m = Objects[i].GenerateManifold(Objects[j]);
                if (m.IsColliding) {
                    manifolds.push_back(m);
                }
            }
        }
        float floorLevel = -2.0f;
        for (auto& obj : Objects) {
            if (obj.Position.y < 0.0f) {
                Manifold m;
                m.BodyA = &obj;
                m.BodyB = nullptr;
                m.Normal = glm::vec3(0, -1, 0);

                std::vector<glm::vec3> corners = obj.GetWorldCorners();
                for (auto& p : corners) {
                    if (p.y < floorLevel) {
                        ContactPoint cp;
                        cp.Position = p;
                        cp.Position.y = floorLevel;
                        cp.Penetration = floorLevel - p.y;
                        m.Contacts.push_back(cp);
                    }
                }
                if (!m.Contacts.empty()) {
                    m.IsColliding = true;
                    static PhysicsObject floorObj(glm::vec3(0, -1000, 0), 0, -1, 0.0f);
                    floorObj.IsStatic = true;
                    m.BodyB = &floorObj;
                    manifolds.push_back(m);
                }
            }
        }
        int iterations = 20;
        for (int i = 0; i < iterations; ++i) {
            for (auto& m : manifolds) {
                ResolveManifold(m);
            }
        }
        for (auto& m : manifolds) {
            PhysicsObject* A = m.BodyA;
            PhysicsObject* B = m.BodyB;
            float invMassA = A->InvMass;
            float invMassB = B->InvMass;
            float totalInvMass = invMassA + invMassB;
            if (totalInvMass == 0.0f) continue;

            float maxPen = 0.0f;
            for (auto& c : m.Contacts) maxPen = std::max(maxPen, c.Penetration);

            const float percent = 0.4f;
            const float slop = 0.01f;
            float correction = std::max(maxPen - slop, 0.0f) / totalInvMass * percent;
            glm::vec3 vec = m.Normal * correction;

            if (!A->IsStatic && !A->IsDragging) A->Position -= vec * invMassA;
            if (!B->IsStatic && !B->IsDragging) B->Position += vec * invMassB;
        }
    }
}

void PhysicsScene::ResolveManifold(Manifold& m) {
    PhysicsObject* A = m.BodyA;
    PhysicsObject* B = m.BodyB;

    for (auto& contact : m.Contacts) {
        glm::vec3 rA = contact.Position - A->Position;
        glm::vec3 rB = contact.Position - B->Position;

        glm::vec3 relVel = (B->Velocity + glm::cross(B->AngularVelocity, rB)) -
            (A->Velocity + glm::cross(A->AngularVelocity, rA));

        float velAlongNormal = glm::dot(relVel, m.Normal);

        if (velAlongNormal > 0) continue;

        glm::vec3 raxn = glm::cross(rA, m.Normal);
        glm::vec3 rbxn = glm::cross(rB, m.Normal);

        float invMassSum = A->InvMass + B->InvMass +
            glm::dot(raxn, A->WorldInvInertia * raxn) +
            glm::dot(rbxn, B->WorldInvInertia * rbxn);

        if (invMassSum == 0.0f) continue;

        float e = std::min(A->Restitution, B->Restitution);

        if (std::abs(velAlongNormal) < 1.0f) e = 0.0f;

        float j = -(1.0f + e) * velAlongNormal;
        j /= invMassSum;
        j /= (float)m.Contacts.size();

        glm::vec3 impulse = m.Normal * j;
        A->ApplyImpulse(-impulse, contact.Position);
        B->ApplyImpulse(impulse, contact.Position);

        relVel = (B->Velocity + glm::cross(B->AngularVelocity, rB)) -
            (A->Velocity + glm::cross(A->AngularVelocity, rA));

        glm::vec3 tangent = relVel - (m.Normal * glm::dot(relVel, m.Normal));
        float tangentLen = glm::length(tangent);

        if (tangentLen > 0.001f) {
            tangent /= tangentLen;

            float jt = -glm::dot(relVel, tangent);

            glm::vec3 raxt = glm::cross(rA, tangent);
            glm::vec3 rbxt = glm::cross(rB, tangent);

            float invMassSumT = A->InvMass + B->InvMass +
                glm::dot(raxt, A->WorldInvInertia * raxt) +
                glm::dot(rbxt, B->WorldInvInertia * rbxt);

            jt /= invMassSumT;
            jt /= (float)m.Contacts.size();

            float mu = std::sqrt(A->Friction * B->Friction);
            glm::vec3 frictionImpulse;

            if (std::abs(jt) < j * mu) {
                frictionImpulse = tangent * jt;
            }
            else {
                frictionImpulse = tangent * -j * mu;
            }

            A->ApplyImpulse(-frictionImpulse, contact.Position);
            B->ApplyImpulse(frictionImpulse, contact.Position);
        }
    }
}

void PhysicsScene::ProcessGrab(glm::vec3 cameraPos, glm::vec3 cameraFront) {
    if (HeldObject) return;
    float minDistance = 1000.0f;
    PhysicsObject* closestObj = nullptr;
    for (auto& obj : Objects) {
        if (obj.TryGrab(cameraPos, cameraFront)) {
            float dist = glm::distance(cameraPos, obj.Position);
            if (dist < minDistance) {
                minDistance = dist;
                closestObj = &obj;
            }
            obj.IsDragging = false;
        }
    }
    if (closestObj) {
        closestObj->IsDragging = true;
        closestObj->HoldDistance = minDistance;
        HeldObject = closestObj;
    }
}

void PhysicsScene::ProcessRelease() {
    if (HeldObject) {
        HeldObject->Release();
        HeldObject = nullptr;
    }
}

void PhysicsScene::ProcessThrow(glm::vec3 force) {
    if (HeldObject) {
        HeldObject->Release();
        glm::vec3 randomOffset = glm::linearRand(glm::vec3(-0.2f), glm::vec3(0.2f));
        HeldObject->ApplyImpulse(force, HeldObject->Position + randomOffset);

        HeldObject = nullptr;
    }
    else {
        ExplodeAll();
    }
}

void PhysicsScene::RotateHeldObject(float amount, bool axisMode) {
    if (HeldObject) {
        HeldObject->ManualRotate(amount, axisMode);
    }
}

void PhysicsScene::ExplodeAll() {
    for (auto& obj : Objects) {
        glm::vec3 randomOffset = glm::linearRand(glm::vec3(-0.2f), glm::vec3(0.2f));
        obj.ApplyImpulse(glm::vec3(0.0f, 10.0f, 0.0f), obj.Position + randomOffset);
    }
}