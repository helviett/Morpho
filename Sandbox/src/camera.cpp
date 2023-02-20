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
    position(glm::vec3(0.0f, 0.0f, 0.0f))
{
}

void Camera::set_perspective_projection(float fovy, float aspect_ratio, float near_plane, float far_plane) {
    projection = perspective(fovy, aspect_ratio, near_plane, far_plane);
}

glm::mat4 Camera::get_view() {
    calculate_view_if_needed();
    return view;
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
    view = glm::mat4(
        right.x, -up.x, -forward.x, 0.0f,
        right.y, -up.y, -forward.y, 0.0f,
        right.z, -up.z, -forward.z, 0.0f,
        -dot(right, position), -dot(-up, position), -dot(-forward, position), 1.0f
    );
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
