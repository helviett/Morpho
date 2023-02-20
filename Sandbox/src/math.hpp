#include <glm/glm.hpp>

glm::mat4 perspective(float fovy, float aspect, float zNear, float zFar);

glm::mat4 look_at(const glm::vec3& camera_position, const glm::vec3& look_at_position, const glm::vec3& up);