#include "camera.h"
#include <glm/fwd.hpp>
#include <glm/gtx/norm.hpp>
#include <math.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/quaternion.hpp>

// dx: left and right.
// dy: forward and backward.
void Camera::FirstPersonControlMove(float dx, float dy) {
  const glm::vec3 eyecenter = camera_vectors_.eye - camera_vectors_.center;
  const glm::vec3 neye = glm::normalize(eyecenter);
  const glm::vec3 nup = glm::normalize(camera_vectors_.up);
  const glm::vec3 nside = glm::normalize(glm::cross(nup, neye));

  // Forward
  glm::vec3 displacementx = nside * dx;
  glm::vec3 displacementy = neye * dy;
  glm::vec3 displacement = displacementx + displacementy;
  camera_vectors_.center += displacement;
  camera_vectors_.eye += displacement;
}

void Camera::FirstPersonControlCenter(float dtheta, float dphy) {
  float angle = sqrt(dtheta * dtheta + dphy * dphy);
  if (std::fabs(angle) < 1e-3) return;
  const glm::vec3 eyecenter = camera_vectors_.center - camera_vectors_.eye;
  const glm::vec3 neye = glm::normalize(eyecenter);
  const glm::vec3 nup = glm::normalize(camera_vectors_.up);
  const glm::vec3 nside = glm::normalize(glm::cross(nup, neye));

  // Rotation component
  glm::vec3 new_up = nup * dphy;
  glm::vec3 new_side = nside * dtheta;

  glm::vec3 new_direction = new_up + new_side;
  glm::vec3 new_rot = glm::normalize(
      glm::cross(new_direction, eyecenter));
  glm::mat4x4 transform = glm::toMat4(glm::angleAxis(angle, new_rot));
  camera_vectors_.center = transform * glm::vec4(eyecenter, 1.0f);
  camera_vectors_.center += camera_vectors_.eye;
  camera_vectors_.up = transform * glm::vec4(camera_vectors_.up, 1.0f);
}

// dtheta is "up" and "down", dphy is "left" and "right. The output
// When doing left/right the camera rotates around ths eye-center axis.
// When doing up/down it rotates around the side axis.
void Camera::FirstPersonControlRotate(float dtheta, float dphy) {
  dtheta /= 10;
  dphy /= 10;
  const glm::vec3 eyecenter = - camera_vectors_.eye + camera_vectors_.center;
  const glm::vec3 neye = glm::normalize(eyecenter);
  const glm::vec3 nup = glm::normalize(camera_vectors_.up);
  const glm::vec3 nside = glm::normalize(glm::cross(nup, neye));

  // Tilt the camera first.
  glm::mat4x4 transform_tilt = glm::toMat4(glm::angleAxis(dtheta, neye));
  camera_vectors_.up = transform_tilt * glm::vec4(nup, 1.0f);

  glm::mat4x4 transform_up = glm::toMat4(glm::angleAxis(dphy, glm::normalize(glm::cross(camera_vectors_.up, neye))));
  camera_vectors_.center = transform_up * glm::vec4(eyecenter, 1.0f);
  camera_vectors_.center += camera_vectors_.eye;
  camera_vectors_.up = transform_up * glm::vec4(camera_vectors_.up, 1.0f);
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
