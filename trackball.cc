#include <cmath>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include "trackball.h"

#include <iostream>
#include <math.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/ext.hpp>

static inline float euclid(float x, float y) {
  return x * x + y * y;
}

static inline float sphere(float x, float y, float r) {
  return sqrt(r * r - (x * x + y * y));
}

static inline float hyper(float x, float y, float r) {
  return r * r * 0.5f / sqrt(x * x + y * y);
}

static float tracksurface(float x, float y, float r) {
  return (euclid(x, y) <= r * r / 2) ? sphere(x, y, r) : hyper(x, y, r);
}

void TrackBall::Start(float x, float y) {
  x = 0, y = 0;
  const float z = tracksurface(x, y, radius_);
  //start_ = glm::vec3{x, y, z};
  start_ = glm::vec3{x, y, z};
  end_ = start_;
}

// We are in furstum coodinates. Front vector is z, up vector is y
// right vector is x. We need to the rotation axis in the original coordinates.
glm::quat TrackBall::Update(float dx, float dy) {
  start_ = end_;
  end_.x += dx;
  end_.y += dy;
  end_.z = tracksurface(end_.x, end_.y, radius_);

  glm::vec3 v1 = glm::normalize(start_);
  glm::vec3 v2 = glm::normalize(end_);

  // Angle between the two vectors
  float costheta = glm::dot(v1, v2);

  // Perpendicular angle
  glm::vec3 rotation_axis;

  rotation_axis = glm::normalize(cross(v1, v2));

  float s = sqrt((1 + costheta) / 2);
  float invs = sqrt((1 - costheta) / 2);
  std::cout << end_.x << " " << end_.y << " " << end_.z << std::endl;
  std::cout << "UNIT" << glm::l2Norm(rotation_axis) << std::endl;
  glm::quat result = glm::quat(
    s * 0.5f, rotation_axis.x * invs,
    rotation_axis.y * invs, rotation_axis.z * invs);
  return result;
}
