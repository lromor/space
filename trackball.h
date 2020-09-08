#ifndef _TRACKBALL_H
#define _TRACKBALL_H

#include <glm/glm.hpp>

class TrackBall {
public:
  TrackBall(float radius = 1) : radius_(radius) {}
  // Get starting poistion of the pointer
  void Start(float x, float y);

  // Update the position of the start vector.
  glm::quat Update(float dx, float dy);

private:
  glm::vec3 start_;
  glm::vec3 end_;

  float radius_;
};


#endif // _TRACKBALL_H
