#include "camera.h"
#include <glm/gtx/norm.hpp>
#include <math.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/quaternion.hpp>

void Camera::FirstPersonControl(float dx, float dy, float dtheta, float dphy) {

}

void Camera::TrackballControlRotate(float dside, float dup) {
  const glm::vec3 eyecenter = camera_vectors_.eye - camera_vectors_.center;
  const glm::vec3 neye = glm::normalize(eyecenter);
  const glm::vec3 nup = glm::normalize(camera_vectors_.up);
  const glm::vec3 nside = glm::normalize(glm::cross(nup, neye));

  // Rotation component
  glm::vec3 new_up = nup * dup;
  glm::vec3 new_side = nside * dside;

  glm::vec3 new_direction = new_up + new_side;
  glm::vec3 new_rot = glm::normalize(
    glm::cross(new_direction, eyecenter));
  float angle = sqrt(dside * dside + dup * dup);
  glm::angleAxis(angle, new_rot);

  glm::mat4x4 transform = glm::toMat4(glm::angleAxis(angle, new_rot));
  camera_vectors_.eye = transform * glm::vec4(eyecenter, 1.0f);
  camera_vectors_.eye += camera_vectors_.center;
  camera_vectors_.up = transform * glm::vec4(camera_vectors_.up, 1.0f);
}

void Camera::TrackballControlPan(float dside, float dup) {
  const glm::vec3 eyecenter = camera_vectors_.eye - camera_vectors_.center;
  const glm::vec3 neye = glm::normalize(eyecenter);
  const glm::vec3 nup = glm::normalize(camera_vectors_.up);
  const glm::vec3 nside = glm::normalize(glm::cross(nup, neye));

  glm::vec3 pan_direction = nup * dup + nside * dside;
  camera_vectors_.eye += pan_direction;
  camera_vectors_.center += pan_direction;
}

void Camera::TrackballControlZoom(float zoom) {
  const glm::vec3 eyecenter = camera_vectors_.eye - camera_vectors_.center;
  const glm::vec3 neye = glm::normalize(eyecenter);
  const float length = glm::l2Norm(eyecenter);
  float logstep = log(length + 1) * zoom / 2.0f;

  // Shift also the center.
  if (logstep + length < 1e-3f) {
    logstep = 0.1f * zoom;
    camera_vectors_.center += neye * logstep;
  }

  camera_vectors_.eye += neye * logstep;
}

Camera::ProjectionMatrices Camera::GetProjectionMatrices(float aspect_ratio, float fov) {
  glm::mat4x4 model = glm::mat4x4(1.0f);
  glm::mat4x4 view = glm::lookAt(
    camera_vectors_.eye, camera_vectors_.center, camera_vectors_.up);
  glm::mat4x4 projection = glm::perspective(fov, aspect_ratio, 0.1f, 100.0f);
  glm::mat4x4 clip =
    glm::mat4x4(1.0f,  0.0f, 0.0f, 0.0f,
                0.0f, -1.0f, 0.0f, 0.0f,
                0.0f,  0.0f, 0.5f, 0.0f,
                0.0f,  0.0f, 0.5f, 1.0f);   // vulkan clip space has inverted y and half z !
  return ProjectionMatrices{ fov, model, view, projection, clip };
}
