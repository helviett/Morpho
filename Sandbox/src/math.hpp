#pragma once
#include <glm/glm.hpp>

glm::mat4 perspective(float fovy, float aspect, float zNear, float zFar);

glm::mat4 look_at(const glm::vec3& camera_position, const glm::vec3& look_at_position, const glm::vec3& up);

glm::mat4 ortho(float w, float h, float d);

inline float projection_plane_distance(float fovy) {
    return 1.0f / tan(fovy / 2);
}

inline float lerp(float a, float b, float t) {
    return a * (1 - t) + b * t;
}

class Frustum {
public:
    Frustum(std::initializer_list<glm::vec3> vertices);

    glm::vec3 operator[](uint32_t index);

    static Frustum from_projection_plane(float g, float s, float a, float b);
private:
    glm::vec3 vertices[8];
};
