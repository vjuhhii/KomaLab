# ⚛️ KomaLab: Physics Sandbox & Rendering Engine

![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B)
![OpenGL](https://img.shields.io/badge/OpenGL-Core_Profile-5586A4?logo=opengl)
![Inspiration](https://img.shields.io/badge/Inspiration-Valve_Source-orange)

## 📖 Overview
**KomaLab** is a physics simulation environment engineered from the ground up. Unlike general-purpose game engines, the primary focus of this project is to create a "digital playground" for realistic rigid body dynamics and object interaction.
Drawing inspiration from the mechanical depth of the **Source Engine**, KomaLab implements a custom physics solver that handles collisions, gravity, and friction, allowing for complex stacking and manipulation of objects in real-time.

### 📐 Physics & Simulation (Prototype Stage)
Currently, the engine implements the core foundation of a physics system, with plans to expand into a full-scale simulation comparable to Source Engine mechanics.

* **Current Implementation (Active):**
    * **Newtonian Integration:** Semi-implicit Euler integration for handling velocity, acceleration, and gravity ($F = ma$).
    * **Basic Collision:** AABB (Axis-Aligned Bounding Box) checks for ground detection and simple object overlaps.
    * **Kinematics:** Object movement, force application, and transform hierarchies.

![Engine Demo](media/demo.gif)
