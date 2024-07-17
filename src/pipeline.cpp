#include <map>
#include <set>
#include <span>
//
#include <spirv_reflect.h>
#include <cmrc/cmrc.hpp>
#include <fmt/base.h>
#undef VULKAN_HPP_NO_TO_STRING
#include <vulkan/vulkan.hpp>
//
#include "pipeline.hpp"
CMRC_DECLARE(shaders);

static auto FormatSize(VkFormat format) 
    -> uint32_t
{
    uint32_t result = 0;
    switch (format) {
        case VK_FORMAT_UNDEFINED:
            result = 0;
            break;
        case VK_FORMAT_R4G4_UNORM_PACK8:
            result = 1;
            break;
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
            result = 2;
            break;
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
            result = 2;
            break;
        case VK_FORMAT_R5G6B5_UNORM_PACK16:
            result = 2;
            break;
        case VK_FORMAT_B5G6R5_UNORM_PACK16:
            result = 2;
            break;
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
            result = 2;
            break;
        case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
            result = 2;
            break;
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
            result = 2;
            break;
        case VK_FORMAT_R8_UNORM:
            result = 1;
            break;
        case VK_FORMAT_R8_SNORM:
            result = 1;
            break;
        case VK_FORMAT_R8_USCALED:
            result = 1;
            break;
        case VK_FORMAT_R8_SSCALED:
            result = 1;
            break;
        case VK_FORMAT_R8_UINT:
            result = 1;
            break;
        case VK_FORMAT_R8_SINT:
            result = 1;
            break;
        case VK_FORMAT_R8_SRGB:
            result = 1;
            break;
        case VK_FORMAT_R8G8_UNORM:
            result = 2;
            break;
        case VK_FORMAT_R8G8_SNORM:
            result = 2;
            break;
        case VK_FORMAT_R8G8_USCALED:
            result = 2;
            break;
        case VK_FORMAT_R8G8_SSCALED:
            result = 2;
            break;
        case VK_FORMAT_R8G8_UINT:
            result = 2;
            break;
        case VK_FORMAT_R8G8_SINT:
            result = 2;
            break;
        case VK_FORMAT_R8G8_SRGB:
            result = 2;
            break;
        case VK_FORMAT_R8G8B8_UNORM:
            result = 3;
            break;
        case VK_FORMAT_R8G8B8_SNORM:
            result = 3;
            break;
        case VK_FORMAT_R8G8B8_USCALED:
            result = 3;
            break;
        case VK_FORMAT_R8G8B8_SSCALED:
            result = 3;
            break;
        case VK_FORMAT_R8G8B8_UINT:
            result = 3;
            break;
        case VK_FORMAT_R8G8B8_SINT:
            result = 3;
            break;
        case VK_FORMAT_R8G8B8_SRGB:
            result = 3;
            break;
        case VK_FORMAT_B8G8R8_UNORM:
            result = 3;
            break;
        case VK_FORMAT_B8G8R8_SNORM:
            result = 3;
            break;
        case VK_FORMAT_B8G8R8_USCALED:
            result = 3;
            break;
        case VK_FORMAT_B8G8R8_SSCALED:
            result = 3;
            break;
        case VK_FORMAT_B8G8R8_UINT:
            result = 3;
            break;
        case VK_FORMAT_B8G8R8_SINT:
            result = 3;
            break;
        case VK_FORMAT_B8G8R8_SRGB:
            result = 3;
            break;
        case VK_FORMAT_R8G8B8A8_UNORM:
            result = 4;
            break;
        case VK_FORMAT_R8G8B8A8_SNORM:
            result = 4;
            break;
        case VK_FORMAT_R8G8B8A8_USCALED:
            result = 4;
            break;
        case VK_FORMAT_R8G8B8A8_SSCALED:
            result = 4;
            break;
        case VK_FORMAT_R8G8B8A8_UINT:
            result = 4;
            break;
        case VK_FORMAT_R8G8B8A8_SINT:
            result = 4;
            break;
        case VK_FORMAT_R8G8B8A8_SRGB:
            result = 4;
            break;
        case VK_FORMAT_B8G8R8A8_UNORM:
            result = 4;
            break;
        case VK_FORMAT_B8G8R8A8_SNORM:
            result = 4;
            break;
        case VK_FORMAT_B8G8R8A8_USCALED:
            result = 4;
            break;
        case VK_FORMAT_B8G8R8A8_SSCALED:
            result = 4;
            break;
        case VK_FORMAT_B8G8R8A8_UINT:
            result = 4;
            break;
        case VK_FORMAT_B8G8R8A8_SINT:
            result = 4;
            break;
        case VK_FORMAT_B8G8R8A8_SRGB:
            result = 4;
            break;
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A8B8G8R8_UINT_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A8B8G8R8_SINT_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A2R10G10B10_UINT_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A2R10G10B10_SINT_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A2B10G10R10_UINT_PACK32:
            result = 4;
            break;
        case VK_FORMAT_A2B10G10R10_SINT_PACK32:
            result = 4;
            break;
        case VK_FORMAT_R16_UNORM:
            result = 2;
            break;
        case VK_FORMAT_R16_SNORM:
            result = 2;
            break;
        case VK_FORMAT_R16_USCALED:
            result = 2;
            break;
        case VK_FORMAT_R16_SSCALED:
            result = 2;
            break;
        case VK_FORMAT_R16_UINT:
            result = 2;
            break;
        case VK_FORMAT_R16_SINT:
            result = 2;
            break;
        case VK_FORMAT_R16_SFLOAT:
            result = 2;
            break;
        case VK_FORMAT_R16G16_UNORM:
            result = 4;
            break;
        case VK_FORMAT_R16G16_SNORM:
            result = 4;
            break;
        case VK_FORMAT_R16G16_USCALED:
            result = 4;
            break;
        case VK_FORMAT_R16G16_SSCALED:
            result = 4;
            break;
        case VK_FORMAT_R16G16_UINT:
            result = 4;
            break;
        case VK_FORMAT_R16G16_SINT:
            result = 4;
            break;
        case VK_FORMAT_R16G16_SFLOAT:
            result = 4;
            break;
        case VK_FORMAT_R16G16B16_UNORM:
            result = 6;
            break;
        case VK_FORMAT_R16G16B16_SNORM:
            result = 6;
            break;
        case VK_FORMAT_R16G16B16_USCALED:
            result = 6;
            break;
        case VK_FORMAT_R16G16B16_SSCALED:
            result = 6;
            break;
        case VK_FORMAT_R16G16B16_UINT:
            result = 6;
            break;
        case VK_FORMAT_R16G16B16_SINT:
            result = 6;
            break;
        case VK_FORMAT_R16G16B16_SFLOAT:
            result = 6;
            break;
        case VK_FORMAT_R16G16B16A16_UNORM:
            result = 8;
            break;
        case VK_FORMAT_R16G16B16A16_SNORM:
            result = 8;
            break;
        case VK_FORMAT_R16G16B16A16_USCALED:
            result = 8;
            break;
        case VK_FORMAT_R16G16B16A16_SSCALED:
            result = 8;
            break;
        case VK_FORMAT_R16G16B16A16_UINT:
            result = 8;
            break;
        case VK_FORMAT_R16G16B16A16_SINT:
            result = 8;
            break;
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            result = 8;
            break;
        case VK_FORMAT_R32_UINT:
            result = 4;
            break;
        case VK_FORMAT_R32_SINT:
            result = 4;
            break;
        case VK_FORMAT_R32_SFLOAT:
            result = 4;
            break;
        case VK_FORMAT_R32G32_UINT:
            result = 8;
            break;
        case VK_FORMAT_R32G32_SINT:
            result = 8;
            break;
        case VK_FORMAT_R32G32_SFLOAT:
            result = 8;
            break;
        case VK_FORMAT_R32G32B32_UINT:
            result = 12;
            break;
        case VK_FORMAT_R32G32B32_SINT:
            result = 12;
            break;
        case VK_FORMAT_R32G32B32_SFLOAT:
            result = 12;
            break;
        case VK_FORMAT_R32G32B32A32_UINT:
            result = 16;
            break;
        case VK_FORMAT_R32G32B32A32_SINT:
            result = 16;
            break;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            result = 16;
            break;
        case VK_FORMAT_R64_UINT:
            result = 8;
            break;
        case VK_FORMAT_R64_SINT:
            result = 8;
            break;
        case VK_FORMAT_R64_SFLOAT:
            result = 8;
            break;
        case VK_FORMAT_R64G64_UINT:
            result = 16;
            break;
        case VK_FORMAT_R64G64_SINT:
            result = 16;
            break;
        case VK_FORMAT_R64G64_SFLOAT:
            result = 16;
            break;
        case VK_FORMAT_R64G64B64_UINT:
            result = 24;
            break;
        case VK_FORMAT_R64G64B64_SINT:
            result = 24;
            break;
        case VK_FORMAT_R64G64B64_SFLOAT:
            result = 24;
            break;
        case VK_FORMAT_R64G64B64A64_UINT:
            result = 32;
            break;
        case VK_FORMAT_R64G64B64A64_SINT:
            result = 32;
            break;
        case VK_FORMAT_R64G64B64A64_SFLOAT:
            result = 32;
            break;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
            result = 4;
            break;
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
            result = 4;
            break;

        default:
            break;
    }
    return result;
}
auto get_refl_desc_sets(spv_reflect::ShaderModule& reflection)
    -> std::vector<SpvReflectDescriptorSet*>
{
	// enumerate desc sets
	uint32_t n_desc_sets;
	SpvReflectResult result;
	std::vector<SpvReflectDescriptorSet*> refl_desc_sets;
	result = reflection.EnumerateEntryPointDescriptorSets("main", &n_desc_sets, nullptr);
	if (result != SPV_REFLECT_RESULT_SUCCESS) fmt::println("shader reflection error: {}", (uint32_t)result);
	refl_desc_sets.resize(n_desc_sets);
	result = reflection.EnumerateEntryPointDescriptorSets("main", &n_desc_sets, refl_desc_sets.data());
	if (result != SPV_REFLECT_RESULT_SUCCESS) fmt::println("shader reflection error: {}", (uint32_t)result);
	return refl_desc_sets;
}
auto Pipeline::Base::compile(vk::Device device, std::string& path)
    -> vk::UniqueShaderModule
{
	cmrc::embedded_filesystem fs = cmrc::shaders::get_filesystem();
	if (!fs.exists(path)) {
		fmt::println("could not find shader: {}", path);
		exit(-1);
	}
	cmrc::file file = fs.open(path);
    vk::ShaderModuleCreateInfo info_shader {
        .codeSize = file.size(),
        .pCode = reinterpret_cast<const uint32_t*>(file.cbegin()),
    };
	return device.createShaderModuleUnique(info_shader);
}
auto Pipeline::Base::reflect(vk::Device device, std::span<std::string> shaderPaths) 
    -> std::pair< vk::VertexInputBindingDescription, std::vector<vk::VertexInputAttributeDescription>>
{
	// read raw shader sources
	std::vector<std::pair<size_t, const uint32_t*>> shaders;
	for (auto& path : shaderPaths) {
		cmrc::embedded_filesystem fs = cmrc::shaders::get_filesystem();
		if (!fs.exists(path)) {
			fmt::println("could not find shader: {}", path);
			exit(-1);
		}
		cmrc::file file = fs.open(path);
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

		uint32_t n_inputs = 0;
		auto result = reflection.EnumerateEntryPointInputVariables("main", &n_inputs, nullptr);
		if (result != SPV_REFLECT_RESULT_SUCCESS) fmt::println("shader reflection error: {}", (uint32_t)result);
		std::vector<SpvReflectInterfaceVariable*> vars(n_inputs);
		result = reflection.EnumerateEntryPointInputVariables("main", &n_inputs, vars.data());
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
			uint32_t format_size = FormatSize((VkFormat)attribute.format);
			attribute.offset = vertex_input_desc.stride;
			vertex_input_desc.stride += format_size;
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
			SpvReflectDescriptorBinding* pBinding = set->bindings[i];
			auto [iter, bUnique] = unique_bindings.emplace(pBinding->set, pBinding->binding);
			if (!bUnique) continue;

            // tally descriptor types for descriptor pool
            auto [iNode, bEmplaced] = binding_tally.emplace((vk::DescriptorType)pBinding->descriptor_type, 1);
            if (!bEmplaced) iNode->second++;

            // // reflect binding
            // fmt::println("\tset {} | binding {}: {} {}", 
            //    pBinding->set,
            //    pBinding->binding,
            //    vk::to_string((vk::DescriptorType)pBinding->descriptor_type), 
            //    pBinding->name);
            bindings.emplace_back().setBinding(pBinding->binding)
                .setDescriptorCount(pBinding->count)
                .setStageFlags(stage_flags) // todo: combine stages flags if its present in both
                .setDescriptorType((vk::DescriptorType)pBinding->descriptor_type);
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