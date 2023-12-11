#include "camera.hpp"
#include "math.hpp"

Camera::Camera() { }

Camera::Camera(
    float yaw,
    float pitch,
    glm::vec3 world_up,
    float fovy,
    float aspect_ratio,
    float near_plane,
    float far_plane
) : projection(perspective(fovy, aspect_ratio, near_plane, far_plane)),
    yaw(yaw),
    pitch(pitch),
    world_up(world_up),
    position(glm::vec3(0.0f, 0.0f, 0.0f)),
    g(1.0f / tan(fovy / 2.0f)),
    s(aspect_ratio),
    near(near_plane),
    far(far_plane)
{
}

void Camera::set_perspective_projection(float fovy, float aspect_ratio, float near_plane, float far_plane) {
    g = 1.0f / tan(fovy / 2.0f);
    s = aspect_ratio;
    near = near_plane;
    far = far_plane;
    projection = perspective(fovy, aspect_ratio, near_plane, far_plane);
}

glm::mat4 Camera::get_view() {
    calculate_view_if_needed();
    return view;
}

glm::mat4 Camera::get_transform() {
    calculate_view_if_needed();
    return transform;
}

void Camera::set_position(glm::vec3 position) {
    this->position = position;
    is_view_dirty = true;
}

void Camera::set_yaw(float degrees) {
    yaw = degrees;
    is_view_dirty = true;
}

void Camera::set_pitch(float degrees) {
    pitch = degrees;
    is_view_dirty = true;
}

void Camera::add_yaw(float degrees_delta) {
    yaw += degrees_delta;
    is_view_dirty = true;
}

void Camera::add_pitch(float degrees_delta) {
    pitch += degrees_delta;
    pitch = pitch < -89.0f ? -89.0f : (pitch > 89.0f ? 89.0f : pitch);
    is_view_dirty = true;
}

void Camera::calculate_view_if_needed() {
    if (!is_view_dirty) {
        return;
    }
    is_view_dirty = false;
    forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward.y = sin(glm::radians(pitch));
    forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward = glm::normalize(forward);
    right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), forward));
    up = glm::normalize(glm::cross(forward, right));
    glm::vec4 point = glm::vec4(0.0f, 1.0f, 1.0f, 1.0);
    glm::mat4 to_view_space_axis_configuration = glm::mat4(
        1.0f,  0.0f,  0.0f, 0.0f,
        0.0f, -1.0f,  0.0f, 0.0f,
        0.0f,  0.0f, -1.0f, 0.0f,
        0.0f,  0.0f,  0.0f, 1.0f
    );
    glm::mat4 from_view_space_axis_configuration = glm::mat4(
        1.0f,  0.0f,  0.0f, 0.0f,
        0.0f, -1.0f,  0.0f, 0.0f,
        0.0f,  0.0f, -1.0f, 0.0f,
        0.0f,  0.0f,  0.0f, 1.0f
    );
    transform = glm::mat4(
         right.x,     right.y,     right.z,    0.0f,
        -up.x,       -up.y,       -up.z,       0.0f,
        -forward.x,  -forward.y,  -forward.z,  0.0f,
         position.x,  position.y,  position.z, 1.0f
    );
    view = glm::mat4(
        right.x, -up.x, -forward.x, 0.0f,
        right.y, -up.y, -forward.y, 0.0f,
        right.z, -up.z, -forward.z, 0.0f,
        -dot(right, position), -dot(-up, position), -dot(-forward, position), 1.0f
    );
    auto test = transform * view;
    bool is_I = test == glm::mat4(1.0f);
}

glm::vec3 Camera::get_right() {
    calculate_view_if_needed();
    return right;
}

glm::vec3 Camera::get_up() {
    calculate_view_if_needed();
    return up;
}

glm::vec3 Camera::get_forward() {
    calculate_view_if_needed();
    return -forward;
}

glm::mat4 Camera::get_projection() {
    return projection;
}

glm::vec3 Camera::get_position() {
    return position;
}

Frustum Camera::get_frustum() {
    return Frustum::from_projection_plane(g, s, near, far);
}

Frustum Camera::get_frustum(float a, float b) {
    return Frustum::from_projection_plane(g, s, a, b);
}

float Camera::get_near() const {
    return near;
}

float Camera::get_far() const {
    return far;
}
