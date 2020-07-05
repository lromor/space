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
#ifndef __REFERENCE_GRID_H_
#define __REFERENCE_GRID_H_

#include <vulkan/vulkan.hpp>

#include "vulkan-core.h"
#include "entity.h"

class ReferenceGrid : public space::Entity {
public:
  ReferenceGrid() {}
  virtual void Register(
    space::core::VkAppContext *context,
    vk::UniquePipelineLayout *pipeline_layout,
    vk::UniqueRenderPass *render_pass,
    vk::UniquePipelineCache *pipeline_cache);
  virtual ~ReferenceGrid() {}

  // Draw in the command buffer
  virtual void Draw(const vk::UniqueCommandBuffer *command_buffer);

private:
  vk::UniquePipeline pipeline_;
};

#endif // __REFERENCE_GRID_H_
