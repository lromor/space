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
#ifndef __CURVE_H_
#define __CURVE_H_

#include <vulkan/vulkan.hpp>

#include "vulkan-core.h"
#include "entity.h"

class Curve : public space::Entity {
public:
  struct Point {
    float x, y, z;
  };

  Curve() {}
  virtual void Register(
    space::core::VkAppContext *context,
    vk::UniquePipelineLayout *pipeline_layout,
    vk::UniqueRenderPass *render_pass,
    vk::UniquePipelineCache *pipeline_cache) final;
  virtual ~Curve() {}

  // Draw in the command buffer
  virtual void Draw(const vk::UniqueCommandBuffer *command_buffer) final;

  // Update the curve to render from 0 up to t.
  void Update(const float t);

private:
  vk::UniquePipeline pipeline_;
  std::vector<Point> points_;

  space::core::VkAppContext *vk_ctx_;

  std::unique_ptr<space::core::BufferData> vertex_buffer_data_;
};

#endif // __CURVE_H_
