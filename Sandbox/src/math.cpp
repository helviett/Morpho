#include "math.hpp"

glm::mat4 perspective(float fovy, float aspect, float near, float far) {
    float const tan_half_fov = tan(fovy / 2.0f);
    glm::mat4 p(0.0f);
    p[0][0] = 1.0f / (aspect * tan_half_fov);
    p[1][1] = 1.0f / tan_half_fov;
    p[2][2] = far / (far - near);
    p[2][3] = 1.0f;
    p[3][2] = -(near * far) / (far - near);
    return p;
}

glm::mat4 look_at(
    const glm::vec3& camera_position,
    const glm::vec3& look_at_position,
    const glm::vec3& world_up
) {
    glm::vec3 f = glm::normalize(camera_position - look_at_position);
    glm::vec3 r = glm::normalize(glm::cross(world_up, f));
    glm::vec3 u = glm::cross(f, r);
    return glm::mat4(
        r.x, -u.x, -f.x, 0.0f,
        r.y, -u.y, -f.y, 0.0f,
        r.z, -u.z, -f.z, 0.0f,
        -dot(r, camera_position), -dot(-u, camera_position), -dot(-f, camera_position), 1.0f
    );
}

glm::mat4 ortho(float w, float h, float d) {
    return glm::mat4(
        2.0f / w, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / h, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f / d, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

Frustum::Frustum(std::initializer_list<glm::vec3> vertices) {
    assert(vertices.size() == 8);
    uint32_t i = 0;
    for (auto& vertex : vertices) {
        this->vertices[i++] = vertex;
    }
}

glm::vec3 Frustum::operator[](uint32_t index) {
    assert(index < 8);
    return vertices[index];
}


Frustum Frustum::from_projection_plane(float g, float s, float a, float b) {
    return {
        glm::vec3(a * s / g, a / g, a),
        glm::vec3(-a * s / g, a / g, a),
        glm::vec3(-a * s / g, -a / g, a),
        glm::vec3(a * s / g, -a / g, a),
        glm::vec3(b * s / g, b / g, b),
        glm::vec3(-b * s / g, b / g, b),
        glm::vec3(-b * s / g, -b / g, b),
        glm::vec3(b * s / g, -b / g, b),
    };
}
