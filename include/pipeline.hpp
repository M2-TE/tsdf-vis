#pragma once
#include <span>
//
#include <vulkan/vulkan.hpp>
#include <fmt/core.h>
//
#include "image.hpp"

namespace Pipeline
{
    struct Base {
		void destroy(vk::Device device) {
			device.destroyPipeline(_pipeline);
			device.destroyPipelineLayout(_pipeline_layout);

			for (auto& layout: _desc_set_layouts) device.destroyDescriptorSetLayout(layout);
			device.destroyDescriptorPool(_pool);
			_desc_sets.resize(0);
			_desc_set_layouts.resize(0);
		}
		void write_descriptor(vk::Device device, uint32_t set, uint32_t binding, Image& image) {
			vk::DescriptorImageInfo info_image {
				.imageView = image._view,
				.imageLayout = vk::ImageLayout::eGeneral,
			};
			vk::WriteDescriptorSet write_image {
				.dstSet = _desc_sets[set],
				.dstBinding = binding,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageImage,
				.pImageInfo = &info_image,
			};
			device.updateDescriptorSets(write_image, {});
		}
		void write_descriptor(vk::Device device, uint32_t set, uint32_t binding, vk::Buffer buffer, vk::DeviceSize size) {
			if (_desc_sets.size() <= set) {
				fmt::println("Attempted to bind invalid set"); 
				return;
			}
			vk::DescriptorBufferInfo info_buffer {
				.buffer = buffer,
				.offset = 0,
				.range = size
			};
			vk::WriteDescriptorSet write_buffer {
				.dstSet = _desc_sets[set],
				.dstBinding = binding,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &info_buffer
			};
			device.updateDescriptorSets(write_buffer, {});
		}
        
	protected:
		// TODO
		auto reflect(vk::Device device, std::span<std::string> shaderPaths)
            -> std::pair<vk::VertexInputBindingDescription, std::vector<vk::VertexInputAttributeDescription>>;
		auto compile(vk::Device device, std::string& path)
            -> vk::UniqueShaderModule;

	protected:
		vk::Pipeline _pipeline;
		vk::PipelineLayout _pipeline_layout;
		vk::DescriptorPool _pool;
		std::vector<vk::DescriptorSet> _desc_sets;
		std::vector<vk::DescriptorSetLayout> _desc_set_layouts;
    };
	struct Compute: Base {
		void init(vk::Device device, std::string cs_path) {
			// reflect shader contents
			std::array<std::string, 1> paths = { cs_path.append(".spv") };
			reflect(device, paths);

			// create pipeline layout
			vk::PipelineLayoutCreateInfo info_layout {
				.setLayoutCount = (uint32_t)_desc_set_layouts.size(),
				.pSetLayouts = _desc_set_layouts.data(),
			};
			_pipeline_layout = device.createPipelineLayout(info_layout);

			// create pipeline
			vk::UniqueShaderModule cs_module = compile(device, cs_path);
			vk::PipelineShaderStageCreateInfo info_shader_stage {
				.stage = vk::ShaderStageFlagBits::eCompute,
				.module = cs_module.get(),
				.pName = "main",
			};
			vk::ComputePipelineCreateInfo info_compute_pipe {
				.stage = info_shader_stage,	
				.layout = _pipeline_layout,
			};
			auto [result, pipeline] = device.createComputePipeline(nullptr, info_compute_pipe);
			if (result != vk::Result::eSuccess) fmt::println("error creating compute pipeline");
			_pipeline = pipeline;
		}
		void execute(vk::CommandBuffer cmd, uint32_t x, uint32_t y, uint32_t z) {
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, _pipeline);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, _pipeline_layout, 0, _desc_sets, {});
			cmd.dispatch(x, y, z);
		}
	};
	struct Graphics: Base {
		void init(vk::Device device, vk::Extent2D extent, std::string vs_path, std::string fs_path) {
			// reflect shader contents
			std::array<std::string, 2> paths = { vs_path.append(".spv"), fs_path.append(".spv") };
			auto [bind_desc, attr_descs] = reflect(device, paths);

			// create pipeline layout
			vk::PushConstantRange pcr {
				.stageFlags = vk::ShaderStageFlagBits::eVertex,
				.offset = 0,
				.size = sizeof(vk::DeviceAddress),
			};
			vk::PipelineLayoutCreateInfo layoutInfo {
				.setLayoutCount = (uint32_t)_desc_set_layouts.size(),
				.pSetLayouts = _desc_set_layouts.data(),
				.pushConstantRangeCount = 1,
				.pPushConstantRanges = &pcr,
			};
			_pipeline_layout = device.createPipelineLayout(layoutInfo);

			// create shader stages
			vk::UniqueShaderModule vs_module = compile(device, vs_path);
			vk::UniqueShaderModule fs_module = compile(device, fs_path);
			std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages {{
				{
					.stage = vk::ShaderStageFlagBits::eVertex,
					.module = vs_module.get(),
					.pName = "main",
				},
				{
					.stage = vk::ShaderStageFlagBits::eFragment,
					.module = fs_module.get(),
					.pName = "main",
				}
			}};

			// create graphics pipeline parts
			vk::PipelineVertexInputStateCreateInfo info_vertex_input;
			if (attr_descs.size() > 0) {
				info_vertex_input = {
					.vertexBindingDescriptionCount = 1,
					.pVertexBindingDescriptions = &bind_desc,
					.vertexAttributeDescriptionCount = (uint32_t)attr_descs.size(),
					.pVertexAttributeDescriptions = attr_descs.data(),
				};
			}
			vk::PipelineInputAssemblyStateCreateInfo info_input_assembly {
				.topology = vk::PrimitiveTopology::eTriangleList,
				.primitiveRestartEnable = false,
			};
			vk::Viewport viewport {
				.x = 0, .y = 0,
				.width = (float)extent.width,
				.height = (float)extent.height,
				.minDepth = 0.0,	
				.maxDepth = 1.0,
			};
			vk::Rect2D scissor({0, 0}, extent);
			vk::PipelineViewportStateCreateInfo info_viewport {
				.viewportCount = 1,
				.pViewports = &viewport,
				.scissorCount = 1,
				.pScissors = & scissor,	
			};
			vk::PipelineRasterizationStateCreateInfo info_rasterization {
				.depthClampEnable = false,
				.rasterizerDiscardEnable = false,
				.polygonMode = vk::PolygonMode::eLine, // line renderer
				// .polygonMode = vk::PolygonMode::eFill, // primitive renderer
				.cullMode = vk::CullModeFlagBits::eBack,
				.frontFace = vk::FrontFace::eClockwise,
				.depthBiasEnable = false,
				.lineWidth = 1.0,
			};
			vk::PipelineMultisampleStateCreateInfo info_multisampling {
				.sampleShadingEnable = false,
				.alphaToCoverageEnable = false,
				.alphaToOneEnable = false,
			};
			vk::PipelineDepthStencilStateCreateInfo info_depth_stencil {
				.depthTestEnable = false,
				.depthWriteEnable = false,
				.depthCompareOp = vk::CompareOp::eLessOrEqual,
				.depthBoundsTestEnable = false,
				.stencilTestEnable = false,
			};
			vk::PipelineColorBlendAttachmentState info_blend_attach {
				.blendEnable = false,
				.colorWriteMask = 
					vk::ColorComponentFlagBits::eR |
					vk::ColorComponentFlagBits::eG |
					vk::ColorComponentFlagBits::eB,
			};
			vk::PipelineColorBlendStateCreateInfo info_blend_state {
				.logicOpEnable = false,
				.attachmentCount = 1,
				.pAttachments = &info_blend_attach,	
				.blendConstants = std::array<float, 4>{ 1.0, 1.0, 1.0, 1.0 },
			};

			// create pipeline
			vk::Format outputFormat = vk::Format::eR16G16B16A16Sfloat;
			vk::PipelineRenderingCreateInfo renderInfo {
				.colorAttachmentCount = 1,
				.pColorAttachmentFormats = &outputFormat,
				.depthAttachmentFormat = vk::Format::eUndefined,
				.stencilAttachmentFormat = vk::Format::eUndefined,
			};
			vk::GraphicsPipelineCreateInfo pipeInfo {
				.pNext = &renderInfo,
				.stageCount = shader_stages.size(), .pStages = shader_stages.data(),
				.pVertexInputState = &info_vertex_input,
				.pInputAssemblyState = &info_input_assembly,
				.pTessellationState = nullptr,
				.pViewportState = &info_viewport,
				.pRasterizationState = &info_rasterization,
				.pMultisampleState = &info_multisampling,
				.pDepthStencilState = &info_depth_stencil,
				.pColorBlendState = &info_blend_state,
				.pDynamicState = nullptr,
				.layout = _pipeline_layout,
			};
			auto [result, pipeline] = device.createGraphicsPipeline(nullptr, pipeInfo);
			if (result != vk::Result::eSuccess) fmt::println("error creating graphics pipeline");
			_pipeline = pipeline;
			_render_area = vk::Rect2D({ 0,0 }, extent);
		}
		void execute(vk::CommandBuffer cmd, Image& dst_image) {
			// vk::RenderingAttachmentInfo info_depth_attach {
			// 	// .imageView = nullptr,
			// 	.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			// 	.resolveMode = 	vk::ResolveModeFlagBits::eNone,
			// 	.loadOp = vk::AttachmentLoadOp::eDontCare,
			// 	.storeOp = vk::AttachmentStoreOp::eStore,
			// 	.clearValue = { .depthStencil = 0.0f },
			// };
			vk::RenderingAttachmentInfo info_color_attach {
				.imageView = dst_image._view,
				.imageLayout = dst_image._last_layout,
				.resolveMode = 	vk::ResolveModeFlagBits::eNone,
				.loadOp = vk::AttachmentLoadOp::eLoad,
				.storeOp = vk::AttachmentStoreOp::eStore,
			};
			vk::RenderingInfo info_render {
				.renderArea = _render_area,
				.layerCount = 1,
				.colorAttachmentCount = 1,
				.pColorAttachments = &info_color_attach,
				.pDepthAttachment = nullptr,
				.pStencilAttachment = nullptr,
			};
			cmd.beginRendering(info_render);
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline);
			if (_desc_sets.size() > 0) {
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipeline_layout, 0, _desc_sets, {});
			}
			//
			// TODO: draw stuff
			//
			cmd.endRendering();
		}
	private:
		vk::Rect2D _render_area;
	};
}