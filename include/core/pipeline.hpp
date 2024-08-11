#pragma once
#include <string_view>
#include <vulkan/vulkan.hpp>
#include <fmt/core.h>
#include "components/mesh/mesh.hpp"
#include "components/image.hpp"

namespace Pipeline
{
    struct Base {
		void destroy(vk::Device device) {
			device.destroyPipeline(_pipeline);
			device.destroyPipelineLayout(_pipeline_layout);

			for (auto& layout: _desc_set_layouts) device.destroyDescriptorSetLayout(layout);
			for (auto& sampler: _immutable_samplers) device.destroySampler(sampler);
			device.destroyDescriptorPool(_pool);
			_desc_sets.clear();
			_desc_set_layouts.clear();
			_immutable_samplers.clear();
		}
		void write_descriptor(vk::Device device, uint32_t set, uint32_t binding, Image& image) {
			// vk::DescriptorImageInfo info_image {
			// 	.imageView = image._view,
			// 	.imageLayout = vk::ImageLayout::eGeneral,
			// };
			// vk::WriteDescriptorSet write_image {
			// 	.dstSet = _desc_sets[set],
			// 	.dstBinding = binding,
			// 	.descriptorCount = 1,
			// 	.descriptorType = vk::DescriptorType::eStorageImage,
			// 	.pImageInfo = &info_image,
			// };

			// set should have an immutable sampler
			vk::DescriptorImageInfo info_image {
				.imageView = image._view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			};
			vk::WriteDescriptorSet write_image {
				.dstSet = _desc_sets[set],
				.dstBinding = binding,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
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
		auto reflect(vk::Device device, const vk::ArrayProxy<std::string_view>& shaderPaths)
            -> std::pair<vk::VertexInputBindingDescription, std::vector<vk::VertexInputAttributeDescription>>;
		auto compile(vk::Device device, std::string_view path)
            -> vk::ShaderModule;

	protected:
		vk::Pipeline _pipeline;
		vk::PipelineLayout _pipeline_layout;
		vk::DescriptorPool _pool;
		std::vector<vk::DescriptorSet> _desc_sets;
		std::vector<vk::DescriptorSetLayout> _desc_set_layouts;
		std::vector<vk::Sampler> _immutable_samplers;
    };
	struct Compute: Base {
		void init(vk::Device device, std::string_view cs_path) {
			// reflect shader contents
			reflect(device, cs_path);

			// create pipeline layout
			vk::PipelineLayoutCreateInfo info_layout {
				.setLayoutCount = (uint32_t)_desc_set_layouts.size(),
				.pSetLayouts = _desc_set_layouts.data(),
			};
			_pipeline_layout = device.createPipelineLayout(info_layout);

			// create pipeline
			vk::ShaderModule cs_module = compile(device, cs_path);
			vk::PipelineShaderStageCreateInfo info_shader_stage {
				.stage = vk::ShaderStageFlagBits::eCompute,
				.module = cs_module,
				.pName = "main",
			};
			vk::ComputePipelineCreateInfo info_compute_pipe {
				.stage = info_shader_stage,	
				.layout = _pipeline_layout,
			};
			auto [result, pipeline] = device.createComputePipeline(nullptr, info_compute_pipe);
			if (result != vk::Result::eSuccess) fmt::println("error creating compute pipeline");
			_pipeline = pipeline;
			// clean up shader module
			device.destroyShaderModule(cs_module);
		}
		void execute(vk::CommandBuffer cmd, uint32_t x, uint32_t y, uint32_t z) {
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, _pipeline);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, _pipeline_layout, 0, _desc_sets, {});
			cmd.dispatch(x, y, z);
		}
	};
	struct Graphics: Base {
		struct CreateInfo {
			vk::Device device;
			vk::Extent2D extent;
			std::vector<vk::Format> color_formats;
			vk::Format depth_format = vk::Format::eUndefined;
			vk::PolygonMode poly_mode = vk::PolygonMode::eFill;
			vk::PrimitiveTopology primitive_topology = vk::PrimitiveTopology::eTriangleList;
			vk::Bool32 primitive_restart = false;
			vk::CullModeFlags cull_mode = vk::CullModeFlagBits::eBack;
			vk::Bool32 blend_enabled = false;
			vk::Bool32 depth_write = false;
			vk::Bool32 depth_test = false;
			std::string_view vs_path;
			std::string_view fs_path;
		};
		void init(CreateInfo& info) {
			// reflect shader contents
			auto [bind_desc, attr_descs] = reflect(info.device, { info.vs_path, info.fs_path });

			// create pipeline layout (TODO: make configurable in CreateInfo)
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
			_pipeline_layout = info.device.createPipelineLayout(layoutInfo);

			// create shader stages
			vk::ShaderModule vs_module = compile(info.device, info.vs_path);
			vk::ShaderModule fs_module = compile(info.device, info.fs_path);
			std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages {{
				vk::PipelineShaderStageCreateInfo {
					.stage = vk::ShaderStageFlagBits::eVertex,
					.module = vs_module,
					.pName = "main",
				},
				vk::PipelineShaderStageCreateInfo {
					.stage = vk::ShaderStageFlagBits::eFragment,
					.module = fs_module,
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
				.topology = info.primitive_topology,			
				.primitiveRestartEnable = info.primitive_restart,
			};
			vk::Viewport viewport {
				.x = 0, .y = 0,
				.width = (float)info.extent.width,
				.height = (float)info.extent.height,
				.minDepth = 0.0,	
				.maxDepth = 1.0,
			};
			vk::Rect2D scissor({0, 0}, info.extent);
			vk::PipelineViewportStateCreateInfo info_viewport {
				.viewportCount = 1,
				.pViewports = &viewport,
				.scissorCount = 1,
				.pScissors = & scissor,	
			};
			vk::PipelineRasterizationStateCreateInfo info_rasterization {
				.depthClampEnable = false,
				.rasterizerDiscardEnable = false,
				.polygonMode = info.poly_mode,
				.cullMode = info.cull_mode,
				.frontFace = vk::FrontFace::eClockwise,
				.depthBiasEnable = false,
				.lineWidth = info.poly_mode == vk::PolygonMode::eLine ? 3.0f : 1.0f,
			};
			vk::PipelineMultisampleStateCreateInfo info_multisampling {
				.sampleShadingEnable = false,
				.alphaToCoverageEnable = false,
				.alphaToOneEnable = false,
			};
			vk::PipelineDepthStencilStateCreateInfo info_depth_stencil {
				.depthTestEnable = info.depth_test,
				.depthWriteEnable = info.depth_write,
				.depthCompareOp = vk::CompareOp::eLessOrEqual,
				.depthBoundsTestEnable = false,
				.stencilTestEnable = false,
			};
			vk::PipelineColorBlendAttachmentState info_blend_attach {
				.blendEnable = info.blend_enabled,
				.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
				.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
				.colorBlendOp = vk::BlendOp::eAdd,
				.srcAlphaBlendFactor = vk::BlendFactor::eOne,
				.dstAlphaBlendFactor = vk::BlendFactor::eZero,
				.alphaBlendOp = vk::BlendOp::eAdd,
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
			vk::PipelineRenderingCreateInfo renderInfo {
				.colorAttachmentCount = (uint32_t)info.color_formats.size(),
				.pColorAttachmentFormats = info.color_formats.data(),
				.depthAttachmentFormat = info.depth_format,
				.stencilAttachmentFormat = info.depth_format,
			};

			vk::GraphicsPipelineCreateInfo pipeInfo {
				.pNext = &renderInfo,
				.stageCount = shader_stages.size(), 
				.pStages = shader_stages.data(),
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
			auto [result, pipeline] = info.device.createGraphicsPipeline(nullptr, pipeInfo);
			if (result != vk::Result::eSuccess) fmt::println("error creating graphics pipeline");
			_pipeline = pipeline;
			_render_area = vk::Rect2D({ 0,0 }, info.extent);
			// clean up shader modules
			info.device.destroyShaderModule(vs_module);
			info.device.destroyShaderModule(fs_module);
		}
		
		// draw mesh with color and depth attachments
		template<typename Vertex, typename Index>
		void execute(vk::CommandBuffer cmd, Mesh<Vertex, Index>& mesh, 
			Image& color_dst, vk::AttachmentLoadOp color_load, 
			DepthStencil& depth_dst, vk::AttachmentLoadOp depth_load)
		{
			vk::RenderingAttachmentInfo info_color_attach {
				.imageView = color_dst._view,
				.imageLayout = color_dst._last_layout,
				.resolveMode = 	vk::ResolveModeFlagBits::eNone,
				.loadOp = color_load,
				.storeOp = vk::AttachmentStoreOp::eStore,
				.clearValue { .color = std::array<float, 4>{ 0, 0, 0, 1 } }
			};
			vk::RenderingAttachmentInfo info_depth_attach {
				.imageView = depth_dst._view,
				.imageLayout = depth_dst._last_layout,
				.resolveMode = 	vk::ResolveModeFlagBits::eNone,
				.loadOp = depth_load,
				.storeOp = vk::AttachmentStoreOp::eStore,
				.clearValue = { .depthStencil = 1.0f },
			};
			vk::RenderingInfo info_render {
				.renderArea = _render_area,
				.layerCount = 1,
				.colorAttachmentCount = 1,
				.pColorAttachments = &info_color_attach,
				.pDepthAttachment = &info_depth_attach,
				.pStencilAttachment = nullptr,
			};
			cmd.beginRendering(info_render);
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline);
			if (_desc_sets.size() > 0) {
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipeline_layout, 0, _desc_sets, {});
			}
			// draw beg //
			if (mesh._indices._index_n > 0) {
				cmd.bindVertexBuffers(0, mesh._vertices._buffer, { 0 });
				cmd.bindIndexBuffer(mesh._indices._buffer, 0, mesh._indices.get_type());
				cmd.drawIndexed(mesh._indices._index_n, 1, 0, 0, 0);
			}
			else {
				cmd.bindVertexBuffers(0, mesh._vertices._buffer, { 0 });
				cmd.draw(mesh._vertices._vertex_n, 1, 0, 0);
			}
			// draw end //
			cmd.endRendering();
		}
		
		// draw mesh with only color attachment
		template<typename Vertex, typename Index>
		void execute(vk::CommandBuffer cmd, Mesh<Vertex, Index>& mesh, 
			Image& color_dst,  vk::AttachmentLoadOp color_load)
		{
			vk::RenderingAttachmentInfo info_color_attach {
				.imageView = color_dst._view,
				.imageLayout = color_dst._last_layout,
				.resolveMode = 	vk::ResolveModeFlagBits::eNone,
				.loadOp = color_load,
				.storeOp = vk::AttachmentStoreOp::eStore,
				.clearValue { .color = std::array<float, 4>{ 0, 0, 0, 1 } }
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
			// draw beg //
			if (mesh._indices._index_n > 0) {
				cmd.bindVertexBuffers(0, mesh._vertices._buffer, { 0 });
				cmd.bindIndexBuffer(mesh._indices._buffer, 0, mesh._indices.get_type());
				cmd.drawIndexed(mesh._indices._index_n, 1, 0, 0, 0);
			}
			else {
				cmd.bindVertexBuffers(0, mesh._vertices._buffer, { 0 });
				cmd.draw(mesh._vertices._vertex_n, 1, 0, 0);
			}
			// draw end //
			cmd.endRendering();
		}
		
		// draw fullscreen mesh using oversized triangle
		void execute(vk::CommandBuffer cmd,
			Image& color_dst, vk::AttachmentLoadOp color_load)
		{
			vk::RenderingAttachmentInfo info_color_attach {
				.imageView = color_dst._view,
				.imageLayout = color_dst._last_layout,
				.resolveMode = 	vk::ResolveModeFlagBits::eNone,
				.loadOp = color_load,
				.storeOp = vk::AttachmentStoreOp::eStore,
				.clearValue { .color = std::array<float, 4>{ 0, 0, 0, 1 } }
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
			cmd.draw(3, 1, 0, 0);
			cmd.endRendering();
		}
	
	private:
		vk::Rect2D _render_area;
	};
}