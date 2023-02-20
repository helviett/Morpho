#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

// Free fly camera.
// View righ-handed space:
// X -- right
// Y -- down
// Z -- into the screen
class Camera {
public:
    Camera();
    Camera(float yaw, float pitch, glm::vec3 world_up, float fovy, float aspect_ratio, float near_plane, float far_plane);
    void set_perspective_projection(float fovy, float aspect_ratio, float near_plane, float far_plane);
    void set_position(glm::vec3 position);
    void set_yaw(float degrees);
    void set_pitch(float degrees);
    void add_yaw(float degrees_delta);
    void add_pitch(float degrees_delta);
    glm::vec3 get_right();
    glm::vec3 get_up();
    glm::vec3 get_forward();
    glm::vec3 get_position();
    glm::mat4 get_view();
    glm::mat4 get_projection();
private:
    bool is_view_dirty = true;
    glm::vec3 forward;
    glm::vec3 right;
    glm::vec3 up;
    glm::vec3 position;
    float yaw;
    float pitch;
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 world_up;

    void calculate_view_if_needed();
};
