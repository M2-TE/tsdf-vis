#include <map>
#include <set>
#include <span>
#include <spirv_reflect.h>
#include <cmrc/cmrc.hpp>
#include <fmt/base.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include "core/pipeline.hpp"
CMRC_DECLARE(shaders);

auto get_refl_desc_sets(spv_reflect::ShaderModule& reflection)
    -> std::vector<SpvReflectDescriptorSet*>
{
	SpvReflectResult result;
	// get number of descriptor sets
	uint32_t refl_desc_sets_n;
	result = reflection.EnumerateEntryPointDescriptorSets("main", &refl_desc_sets_n, nullptr);
	if (result != SPV_REFLECT_RESULT_SUCCESS) fmt::println("shader reflection error: {}", (uint32_t)result);
	std::vector<SpvReflectDescriptorSet*> refl_desc_sets(refl_desc_sets_n);
	// fill vector with reflected descriptor sets
	result = reflection.EnumerateEntryPointDescriptorSets("main", &refl_desc_sets_n, refl_desc_sets.data());
	if (result != SPV_REFLECT_RESULT_SUCCESS) fmt::println("shader reflection error: {}", (uint32_t)result);
	return refl_desc_sets;
}
auto Pipeline::Base::compile(vk::Device device, std::string_view path)
    -> vk::ShaderModule
{
	cmrc::embedded_filesystem fs = cmrc::shaders::get_filesystem();
	if (!fs.exists(path.data())) {
		fmt::println("could not find shader: {}", path.data());
		exit(-1);
	}
	cmrc::file file = fs.open(path.data());
    vk::ShaderModuleCreateInfo info_shader {
        .codeSize = file.size(),
        .pCode = reinterpret_cast<const uint32_t*>(file.cbegin()),
    };
	return device.createShaderModule(info_shader);
}
auto Pipeline::Base::reflect(vk::Device device, const vk::ArrayProxy<std::string_view>& shader_paths) 
    -> std::pair< vk::VertexInputBindingDescription, std::vector<vk::VertexInputAttributeDescription>>
{
	// read raw shader sources
	std::vector<std::pair<size_t, const uint32_t*>> shaders;
	for (std::string_view path: shader_paths) {
		cmrc::embedded_filesystem fs = cmrc::shaders::get_filesystem();
		if (!fs.exists(path.data())) {
			fmt::println("could not find shader: {}", path.data());
			exit(-1);
		}
		cmrc::file file = fs.open(path.data());
		shaders.emplace_back(file.size(), reinterpret_cast<const uint32_t*>(file.cbegin()));
	}

	// reflect shaders
	std::vector<spv_reflect::ShaderModule> reflections;
	for (auto& shader: shaders) {
		reflections.emplace_back(shader.first, shader.second);
	}

	// enumerate over input stages
	vk::ShaderStageFlags stage_flags;
    vk::VertexInputBindingDescription vertex_input_desc;
    std::vector<vk::VertexInputAttributeDescription> attr_descs;
	for (auto& reflection: reflections) {
		auto stage = (vk::ShaderStageFlags)reflection.GetShaderStage();
		stage_flags |= stage;

		// continue if not a vertex shader
		if (stage != vk::ShaderStageFlagBits::eVertex) continue;

		uint32_t inputs_n = 0;
		auto result = reflection.EnumerateEntryPointInputVariables("main", &inputs_n, nullptr);
		if (result != SPV_REFLECT_RESULT_SUCCESS) fmt::println("shader reflection error: {}", (uint32_t)result);
		std::vector<SpvReflectInterfaceVariable*> vars(inputs_n);
		result = reflection.EnumerateEntryPointInputVariables("main", &inputs_n, vars.data());
		if (result != SPV_REFLECT_RESULT_SUCCESS) fmt::println("shader reflection error: {}", (uint32_t)result);

        // build bind descriptions 
		vertex_input_desc = {
            .binding = 0,
            .stride = 0,
            .inputRate = vk::VertexInputRate::eVertex,
        };

        // gather attribute descriptions
		attr_descs.reserve(vars.size());
		for (auto* input: vars) {
			if (input->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) continue;
            vk::VertexInputAttributeDescription attrDesc {
                .location = input->location,
                .binding = vertex_input_desc.binding,
                .format = (vk::Format)input->format,
                .offset = 0, // computed later
            };
			attr_descs.push_back(attrDesc);
		}

		// sort attributes by location
        auto sorter = [](auto& a, auto& b) { return a.location < b.location; };
		std::sort(std::begin(attr_descs), std::end(attr_descs), sorter);
		// compute final offsets of each attribute and total vertex stride
		for (auto& attribute: attr_descs) {
			attribute.offset = vertex_input_desc.stride;
			vertex_input_desc.stride += vk::blockSize(attribute.format);;
		}
	}

	// combine descriptor sets
	std::vector<SpvReflectDescriptorSet*> refl_desc_sets;
	for (auto& reflection: reflections) {
		auto parts = get_refl_desc_sets(reflection);
		refl_desc_sets.insert(refl_desc_sets.end(), parts.cbegin(), parts.cend());
	}
	if (refl_desc_sets.size() == 0) return { vertex_input_desc, attr_descs };

	// enumerate descriptor sets and bindings
    std::map<vk::DescriptorType, uint32_t> binding_tally;
	std::set<std::pair<uint32_t, uint32_t>> unique_bindings;
    _desc_set_layouts.reserve(refl_desc_sets.size());
    for (const auto& set: refl_desc_sets) {
        // enumerate all bindings for current set
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
		bindings.reserve(set->binding_count);
        for (uint32_t i = 0; i < set->binding_count; i++) {

			// check if binding is unique
			SpvReflectDescriptorBinding* binding_p = set->bindings[i];
			auto [_, unique] = unique_bindings.emplace(binding_p->set, binding_p->binding);
			if (!unique) continue;

            // tally descriptor types for descriptor pool
            auto [it_node, emplaced] = binding_tally.emplace((vk::DescriptorType)binding_p->descriptor_type, 1);
            if (!emplaced) it_node->second++;

            // reflect binding
            fmt::println("\tset {} | binding {}: {} {}", 
               binding_p->set,
               binding_p->binding,
               vk::to_string((vk::DescriptorType)binding_p->descriptor_type), 
               binding_p->name);
            vk::DescriptorSetLayoutBinding binding {
                .binding = binding_p->binding,
                .descriptorType = (vk::DescriptorType)binding_p->descriptor_type,
                .descriptorCount = binding_p->count,
                .stageFlags = stage_flags, // todo: combine stages flags if its present in both
				.pImmutableSamplers = nullptr
            };
			// optionally create immutable sampler
			if (binding_p->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
				vk::SamplerCreateInfo info_sampler = {
					.magFilter = vk::Filter::eLinear,
					.minFilter = vk::Filter::eLinear,
					.mipmapMode = vk::SamplerMipmapMode::eLinear,
					.addressModeU = vk::SamplerAddressMode::eClampToEdge,
					.addressModeV = vk::SamplerAddressMode::eClampToEdge,
					.addressModeW = vk::SamplerAddressMode::eClampToEdge,
					.mipLodBias = 0.0f,
					.anisotropyEnable = false,
					.maxAnisotropy = 1.0f,
					.compareEnable = false,
					.compareOp = vk::CompareOp::eAlways,
					.minLod = 0.0f,
					.maxLod = 0.0f,
					.borderColor = vk::BorderColor::eFloatOpaqueWhite,
					.unnormalizedCoordinates = false,
				};
				_immutable_samplers.push_back(device.createSampler(info_sampler));
				binding.pImmutableSamplers = &_immutable_samplers.back();
			}
            bindings.push_back(binding);
        }

        // create set layout from all bindings
		if (bindings.size() == 0) continue;
        vk::DescriptorSetLayoutCreateInfo descLayoutInfo { 
            .bindingCount = (uint32_t)bindings.size(), 
            .pBindings = bindings.data() 
        };
        _desc_set_layouts.emplace_back(device.createDescriptorSetLayout(descLayoutInfo));
    }

    // create descriptor pool
    std::vector<vk::DescriptorPoolSize> poolSizes;
    poolSizes.reserve(binding_tally.size());
    for (const auto& pair : binding_tally) poolSizes.emplace_back(pair.first, pair.second);
    auto poolCreateInfo = vk::DescriptorPoolCreateInfo {
        .maxSets = (uint32_t)unique_bindings.size(),
        .poolSizeCount = (uint32_t)poolSizes.size(), 
        .pPoolSizes = poolSizes.data(),
    };
    _pool = device.createDescriptorPool(poolCreateInfo);

    // allocate desc sets
    vk::DescriptorSetAllocateInfo allocInfo {
        .descriptorPool = _pool,
        .descriptorSetCount = (uint32_t)_desc_set_layouts.size(), 
        .pSetLayouts = _desc_set_layouts.data(),
    };
    _desc_sets = device.allocateDescriptorSets(allocInfo);
    return { vertex_input_desc, attr_descs };
}