#include "camera.hpp"

Camera::Camera() { }

Camera::Camera(
    float yaw,
    float pitch,
    glm::vec3 world_up,
    float fovy,
    float aspect_ratio,
    float near_plane,
    float far_plane
) : projection(glm::perspective(fovy, aspect_ratio, near_plane, far_plane)),
    yaw(yaw),
    pitch(pitch),
    world_up(world_up),
    position(glm::vec3(0.0f, 0.0f, 0.0f))
{
    projection[1][1] *= -1;
}

void Camera::set_perspective_projection(float fovy, float aspect_ratio, float near_plane, float far_plane) {
    projection = glm::perspective(fovy, aspect_ratio, near_plane, far_plane);
    projection[1][1] *= -1;
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
    right = glm::normalize(glm::cross(forward, world_up));
    up = glm::normalize(glm::cross(right, forward));
    view = glm::mat4(
        right.x, right.y, right.z, -glm::dot(right, position),
        up.x, up.y, up.z, -glm::dot(up, position),
        -forward.x, -forward.y, -forward.z, -glm::dot(-forward, position),
        0.0f, 0.0f, 0.0f, 1.0f
    );
    view = glm::mat4(
        right.x, up.x, -forward.x, 0.0f,
        right.y, up.y, -forward.y, 0.0f,
        right.z, up.z, -forward.z, 0.0f,
        -glm::dot(right, position), -glm::dot(up, position), -glm::dot(-forward, position), 1.0f
    );
    auto kekview = glm::mat4(1.0);
    auto right = glm::normalize(glm::cross(forward, world_up));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
    auto up = glm::normalize(glm::cross(right, forward));
    auto rotation = glm::transpose(glm::mat3(right, up, -forward));
    kekview[0] = glm::vec4(rotation[0], 0.0f);
    kekview[1] = glm::vec4(rotation[1], 0.0f);
    kekview[2] = glm::vec4(rotation[2], 0.0f);
    kekview = kekview * glm::translate(glm::mat4(1.0), -position);
    //wow;
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
    return forward;
}

glm::mat4 Camera::get_projection() {
    return projection;
}

glm::vec3 Camera::get_position() {
    return position;
}
