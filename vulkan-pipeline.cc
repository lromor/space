#include <unordered_map>
#include <algorithm>
#include <set>

#include "vulkan-core.h"

using namespace space::core;

class GraphicsPipelineBuilder::Impl {
public:
  Impl(
    const vk::UniqueDevice *device,
    const vk::UniquePipelineLayout *pipeline_layout,
    const vk::UniqueRenderPass *render_pass);

  void SetPrimitiveTopology(vk::PrimitiveTopology topology);
  void SetPolygonMode(vk::PolygonMode mode);
  void SetFrontFace(vk::FrontFace front_face);
  void DepthBuffered(const bool value) { depth_buffered_ = value; }
  void AddShader(const vk::ShaderModule &shader,
                 const vk::ShaderStageFlagBits &stage,
                 const vk::SpecializationInfo *specialization_info);

  void AddVertexInputBindingDescription(
    const vk::VertexInputBindingDescription &input_binding);
  bool AddVertexInputAttributeDescription(
    const vk::VertexInputAttributeDescription &input_attribute);

  void EnableDynamicState(const vk::DynamicState &state);

  vk::UniquePipeline Create(vk::UniquePipelineCache *pipeline_cache);
private:
  const vk::UniqueDevice *device_;
  const vk::UniquePipelineLayout *pipeline_layout_;
  const vk::UniqueRenderPass *render_pass_;

  // Depth buffered?
  bool depth_buffered_;

  vk::PipelineInputAssemblyStateCreateInfo input_assembly_state_;
  vk::PipelineRasterizationStateCreateInfo rasterization_state_;

  // A unique shader per type.
  std::unordered_map<enum vk::ShaderStageFlagBits,
                     vk::PipelineShaderStageCreateInfo> stages_;

  std::unordered_map<uint32_t, vk::VertexInputBindingDescription> input_bindings_;
  std::vector<vk::VertexInputAttributeDescription> input_attributes_;

  // Enabled dynamic states
  std::set<vk::DynamicState> dynamic_states_;
};

GraphicsPipelineBuilder::Impl::Impl(
  const vk::UniqueDevice *device,
  const vk::UniquePipelineLayout *pipeline_layout,
  const vk::UniqueRenderPass *render_pass)
  : device_(device), pipeline_layout_(pipeline_layout),
    render_pass_(render_pass),
    depth_buffered_(false),
    input_assembly_state_(
      vk::PipelineInputAssemblyStateCreateFlags(),
      vk::PrimitiveTopology::eTriangleList),
    rasterization_state_(
      vk::PipelineRasterizationStateCreateFlags(), false, false,
      vk::PolygonMode::eLine, vk::CullModeFlagBits::eNone,
      vk::FrontFace::eClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f) {}

void GraphicsPipelineBuilder::Impl::AddShader(
  const vk::ShaderModule &shader,
  const vk::ShaderStageFlagBits &stage,
  const vk::SpecializationInfo *specialization_info) {
  stages_.insert({stage, vk::PipelineShaderStageCreateInfo(
      vk::PipelineShaderStageCreateFlags(), stage,
      shader, "main", specialization_info)});
}

void GraphicsPipelineBuilder::Impl::SetPrimitiveTopology(vk::PrimitiveTopology topology) {
  input_assembly_state_.topology = topology;
}

void GraphicsPipelineBuilder::Impl::SetPolygonMode(vk::PolygonMode mode) {
  rasterization_state_.polygonMode = mode;
}

void GraphicsPipelineBuilder::Impl::SetFrontFace(vk::FrontFace front_face) {
  rasterization_state_.frontFace = front_face;
}

void GraphicsPipelineBuilder::Impl::AddVertexInputBindingDescription(
  const vk::VertexInputBindingDescription &input_binding) {
  input_bindings_[input_binding.binding] = input_binding;
}

bool GraphicsPipelineBuilder::Impl::AddVertexInputAttributeDescription(
  const vk::VertexInputAttributeDescription &input_attribute) {
  // Check that the binding is already added.
  if (input_bindings_.find(input_attribute.binding) == input_bindings_.end())
    return false;
  input_attributes_.push_back(input_attribute);
  return true;
}

void GraphicsPipelineBuilder::Impl::EnableDynamicState(const vk::DynamicState &state) {
  dynamic_states_.insert(state);
}

vk::UniquePipeline GraphicsPipelineBuilder::Impl::Create(vk::UniquePipelineCache *pipeline_cache) {
  // Shaders
  std::vector<vk::PipelineShaderStageCreateInfo> stages;
  for (const auto &value : stages_) {
    stages.push_back(value.second);
  }

  // Vertex input state
  std::vector<vk::VertexInputBindingDescription> input_bindings;
  for (const auto &value : input_bindings_) {
    input_bindings.push_back(value.second);
  }

  vk::PipelineVertexInputStateCreateInfo vertex_input_state;
  vertex_input_state.vertexBindingDescriptionCount = input_bindings_.size();
  vertex_input_state.pVertexBindingDescriptions = input_bindings.data();
  vertex_input_state.vertexAttributeDescriptionCount = input_attributes_.size();
  vertex_input_state.pVertexAttributeDescriptions = input_attributes_.data();

  // Viewport state
  vk::PipelineViewportStateCreateInfo viewport_state(
    vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

  vk::PipelineMultisampleStateCreateInfo multisample_state(
    {}, vk::SampleCountFlagBits::e1);

  vk::StencilOpState stencil_op_state(
    vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep,
    vk::CompareOp::eAlways);
  vk::PipelineDepthStencilStateCreateInfo depth_stencil_state(
    vk::PipelineDepthStencilStateCreateFlags(), depth_buffered_, depth_buffered_,
    vk::CompareOp::eLessOrEqual, false, false, stencil_op_state, stencil_op_state);

    vk::ColorComponentFlags color_component_flags(
      vk::ColorComponentFlagBits::eR
      | vk::ColorComponentFlagBits::eG
      | vk::ColorComponentFlagBits::eB
      | vk::ColorComponentFlagBits::eA);

  vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state(
    false, vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
    vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd, color_component_flags);
  vk::PipelineColorBlendStateCreateInfo color_blend_state(
    vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eNoOp,
    1, &pipeline_color_blend_attachment_state, { { 1.0f, 1.0f, 1.0f, 1.0f } });

  // Dynamic states
  vk::PipelineDynamicStateCreateInfo dynamic_state;
  std::vector<vk::DynamicState> enabled_states;
  std::copy(dynamic_states_.begin(), dynamic_states_.end(),
            std::back_inserter(enabled_states));
  dynamic_state.pDynamicStates = enabled_states.data();
  dynamic_state.dynamicStateCount = enabled_states.size();

  // Build the stages array. Convert the unordered map in a vector.
  vk::GraphicsPipelineCreateInfo graphics_pipeline_create_info(
    vk::PipelineCreateFlags(), stages.size(), stages.data(),
    &vertex_input_state, &input_assembly_state_,
    nullptr, &viewport_state, &rasterization_state_,
    &multisample_state, &depth_stencil_state,
    &color_blend_state, &dynamic_state,
    pipeline_layout_->get(), render_pass_->get(), 0, nullptr, 0);

  auto &device = *device_;
  return device->createGraphicsPipelineUnique(
    pipeline_cache->get(), graphics_pipeline_create_info).value;
}

// PIMPL forwards.
GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetPrimitiveTopology(
  vk::PrimitiveTopology topology) { impl_->SetPrimitiveTopology(topology); return *this; }

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetPolygoneMode(
  vk::PolygonMode mode) { impl_->SetPolygonMode(mode); return *this; }

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetFrontFace(
  vk::FrontFace front_face) { impl_->SetFrontFace(front_face); return *this; }

GraphicsPipelineBuilder& GraphicsPipelineBuilder::DepthBuffered(const bool value){
  impl_->DepthBuffered(value); return *this; }

GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddFragmentShader(
  const vk::ShaderModule &shader, const vk::SpecializationInfo *specialization_info) {
  impl_->AddShader(shader, vk::ShaderStageFlagBits::eFragment, specialization_info);
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddVertexShader(
  const vk::ShaderModule &shader, const vk::SpecializationInfo *specialization_info) {
  impl_->AddShader(shader, vk::ShaderStageFlagBits::eVertex, specialization_info);
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddVertexInputBindingDescription(
  uint32_t binding, uint32_t stride, vk::VertexInputRate input_rate) {
  impl_->AddVertexInputBindingDescription({ binding, stride, input_rate });
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddVertexInputAttributeDescription(
  uint32_t location, uint32_t binding, vk::Format format, uint32_t offset) {
  impl_->AddVertexInputAttributeDescription({location, binding, format, offset});
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::EnableDynamicState(
  const vk::DynamicState &state) {
  impl_->EnableDynamicState(state);
  return *this;
}

vk::UniquePipeline GraphicsPipelineBuilder::Create(
  vk::UniquePipelineCache *pipeline_cache) {
  return impl_->Create(pipeline_cache);
}

GraphicsPipelineBuilder::GraphicsPipelineBuilder(
  const vk::UniqueDevice *device,
  const vk::UniquePipelineLayout *pipeline_layout,
  const vk::UniqueRenderPass *render_pass)
  : impl_(new Impl(device, pipeline_layout, render_pass)) {}

GraphicsPipelineBuilder::~GraphicsPipelineBuilder() = default;
