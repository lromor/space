// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright(c) Leonardo Romor <leonardo.romor@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>
#include <bits/stdint-uintn.h>
#include <memory>
#include <numeric>
#include <cmath>
#include <set>
#include <iostream>

#include "vulkan-core.h"

#include "curve.h"
#include "shaders/curve.vert.h"
#include "shaders/curve.frag.h"


Point operator* (const Point& point, const float scalar) {
  return Point{point.x * scalar, point.y * scalar, point.z * scalar};
}

Point operator* (const float scalar, const Point& point) {
  return point * scalar;
}

Point operator+ (const Point& p1, const Point& p2) {
  return Point{p1.x + p2.x, p1.y + p2.y, p1.z + p2.z};
}


// https://pages.mtu.edu/~shene/COURSES/cs3621/NOTES/spline/B-spline/de-Boor.html
// Define a parametric NURB in the 3d space with pinned
// uniform knots
class NURBS {
public:
  typedef std::vector<Point> ControlPoints;
  NURBS(const ControlPoints &control_points, unsigned int p = 2);

  const Point operator()(float t) const;

private:
  const ControlPoints cps_;
  const unsigned int p_;
  std::vector<float> knots_;

};


Point DeBoor(const float t, const std::vector<Point> &points, const std::vector<float> &knots, const unsigned p) {
  assert(points.size() > 0);
  if (points.size() == 1) {
    return points.back();
  }

  // Remove the external knots.
  const std::vector<float> new_knots(knots.begin() + 1, knots.end() - 1);

  // Compute the parent node points.
  const unsigned int num_new_points = points.size() - 1;
  std::vector<Point> new_points(num_new_points);

  float a[num_new_points];
  for (size_t i = 0; i < num_new_points; ++i) {
    a[i] = (t - knots[i]) / (knots[i + p] - knots[i]);
  }

  for (size_t i = 0; i < num_new_points; ++i) {
    const auto new_point = (1 - a[i]) * points[i] + a[i] * points[i + 1];
    new_points[i] = new_point;
  }

  return DeBoor(t, new_points, new_knots, p - 1);
}

// cps vector of control points, p order of spline.
NURBS::NURBS(const ControlPoints &cps, unsigned int p)
  : cps_(cps), p_(p), knots_() {
  const unsigned int num_middle_knots = cps.size() - p;
  const float step = 1.0 / num_middle_knots;
  unsigned int i;

  // Create the knots.
  // At the end we should have, for instance, for degree 2 and #knots 6
  // 0.0, 0.0, 0.0, 0.3, 0.6, 1.0, 1.0, 1.0
  for (i = 0; i < p + 1; ++i)
    knots_.push_back(0);

  for (i = 1; i < num_middle_knots; ++i)
    knots_.push_back(step * i);

  for (i = 0; i < p + 1; ++i)
    knots_.push_back(1.0);
}

const Point NURBS::operator()(float t) const {
  // Compute the value using De-boor
  // Clamp t to stay within the interval definition [0, 1].
  t = (t > 1.0) ? 1.0 : (t < 0.0) ? 0.0 : t;
  unsigned int s = 0, k = 0;

  // Find the knot interval index (k) such that u_k < t < u_{k + 1}
  for (const float knot : knots_) {
    if (knot <= t) {
      k++;
      // Increase multiplicity if t is equal to the knot.
      s += (knot == t);
    }
    else break;
  }
  k--;
  s = (s > 0) ? s - 1 : 0;

  // Last control point
  if ((k - p_) == cps_.size()) {
    return cps_.back();
  }

  // Get the control points associated with that specific point.
  // Remember that every point in the NURB/B-spline is affected but only a neibhoring subset
  // of control points. This is because b-splines overlaps based on their order and knots
  // and have limited support (value t of the domain for which their value is not 0).
  std::vector<Point> points;
  std::copy(cps_.begin() + k - p_, cps_.begin() + k - s + 1, std::back_inserter(points));

  // We also want to collect te respective knots of the basis functions centered in our control points.
  std::vector<float> knots;
  std::copy(knots_.begin() + k - p_ + 1, knots_.begin() + k - s + p_ + 1, std::back_inserter(knots));

  return DeBoor(t, points, knots, p_);
}

void Curve::Update(const float t) {}

void Curve::Register(
    space::core::VkAppContext *context,
    vk::UniquePipelineLayout *pipeline_layout,
    vk::UniqueRenderPass *render_pass,
    vk::UniquePipelineCache *pipeline_cache) {

  points_.clear();

  // Instantiate the shaders
  vk::UniqueShaderModule vertex =
    context->device->createShaderModuleUnique(
      vk::ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(), sizeof(curve_vert), curve_vert));

  vk::UniqueShaderModule frag =
    context->device->createShaderModuleUnique(
      vk::ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(), sizeof(curve_frag), curve_frag));

  pipeline_ = space::core::GraphicsPipelineBuilder(&context->device, pipeline_layout, render_pass)
    .DepthBuffered(true)
    .SetPrimitiveTopology(vk::PrimitiveTopology::eLineList)
    .SetPolygoneMode(vk::PolygonMode::eLine)
    .AddVertexShader(*vertex)
    .AddFragmentShader(*frag)
    .AddVertexInputBindingDescription(0, sizeof(Point), vk::VertexInputRate::eVertex)
    .AddVertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0)
    .EnableDynamicState(vk::DynamicState::eScissor)
    .EnableDynamicState(vk::DynamicState::eViewport)
    .EnableDynamicState(vk::DynamicState::eLineWidth)
    .Create(pipeline_cache);

  unsigned nsteps = 1000;

  std::vector<Point> points;
  points.push_back({ 4.0f, 1.0f, 8.3f });
  points.push_back({ 1.0f, 1.0f, 0.0f });
  points.push_back({ -3.0f, 1.0f, -2.1f });
  points.push_back({ 2.0f, 1.0f, 0.0f });
  points.push_back({ 4.0f, 1.0f, 5.0f });
  points.push_back({ 3.2f, -1.0f, 0.0f });
  points.push_back({ 6.0f, 1.0f, 0.3f });
  auto f = NURBS(points, 3);

  // Sample!
  for (unsigned i = 0; i <= nsteps; ++i) {
    const float t = 1.0f * i / nsteps;
    const Point p = f(t);
    points_.push_back(p);
  }

  vertex_buffer_data_ = std::make_unique<space::core::BufferData>(
    context->physical_device, context->device, points_.size() * sizeof(Point),
    vk::BufferUsageFlagBits::eVertexBuffer);
  // Submit them to the device
  space::core::CopyToDevice(
    context->device, vertex_buffer_data_->deviceMemory, points_.data(), points_.size());

  // Build the index buffer
  std::vector<uint16_t> indexes((points_.size() - 1) * 2);
  for (size_t i = 0; i < indexes.size(); ++i) {
    indexes[i] = i / 2 + i % 2;
  }
  index_buffer_data_ = std::make_unique<space::core::BufferData>(
    space::core::BufferData(
      context->physical_device, context->device, indexes.size() * sizeof(uint16_t),
      vk::BufferUsageFlagBits::eIndexBuffer));

  // Submit them to the device
  space::core::CopyToDevice(
    context->device, index_buffer_data_->deviceMemory, indexes.data(), indexes.size());
}

void Curve::Draw(const vk::UniqueCommandBuffer *command_buffer) {
  const vk::UniqueCommandBuffer &cb  = *command_buffer;

  // Tell vulkan the next commands are associated to this pipeline.
  cb->bindPipeline(
    vk::PipelineBindPoint::eGraphics, pipeline_.get());

  // Tell vulkan which buffer contains the vertices we want to draw.
  cb->bindVertexBuffers(0, *vertex_buffer_data_->buffer, {0});
  cb->bindIndexBuffer(*index_buffer_data_->buffer, 0, vk::IndexType::eUint16);
  cb->setLineWidth(2.0);

  cb->drawIndexed((points_.size() - 1) * 2, 1, 0, 0, 0);
}
