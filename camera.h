// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
#ifndef __CAMERA_H_
#define __CAMERA_H_

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

class Camera {
public:
  Camera()
    : camera_vectors_{glm::vec3(0.0f, 2.0f, 5.0f),
    glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)} {}

  void FirstPersonControl(float dx, float dy, float dtheta, float dphy);

  void TrackballControlRotate(float dside, float dup);
  void TrackballControlPan(float dside, float dup);
  void TrackballControlZoom(float zoom);

  struct ProjectionMatrices {
    float fov;
    glm::mat4x4 model;
    glm::mat4x4 view;
    glm::mat4x4 projection;
    glm::mat4x4 clip;
  };

  ProjectionMatrices GetProjectionMatrices(float aspect_ratio, float fov = glm::radians(60.0f));

private:

  struct CameraVectors {
    glm::vec3 eye; // Camera position.
    glm::vec3 center; // Looking point.
    glm::vec3 up; // Camera pointing-top vector.
  };
  struct CameraVectors camera_vectors_;
};

#endif // __CAMERA_H_
