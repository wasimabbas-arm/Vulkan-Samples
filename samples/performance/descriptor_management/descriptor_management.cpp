/* Copyright (c) 2019-2025, Arm Limited and Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "descriptor_management.h"

#include "common/vk_common.h"
#include "filesystem/legacy.h"
#include "gltf_loader.h"
#include "gui.h"

#include "rendering/subpasses/forward_subpass.h"
#include "stats/stats.h"

DescriptorManagement::DescriptorManagement()
{
	auto &config = get_configuration();

	config.insert<vkb::IntSetting>(0, descriptor_caching.value, 0);
	config.insert<vkb::IntSetting>(0, buffer_allocation.value, 0);

	config.insert<vkb::IntSetting>(1, descriptor_caching.value, 1);
	config.insert<vkb::IntSetting>(1, buffer_allocation.value, 1);
}

bool DescriptorManagement::prepare(const vkb::ApplicationOptions &options)
{
	if (!VulkanSample::prepare(options))
	{
		return false;
	}

	// Load a scene from the assets folder
	load_scene("scenes/bonza/Bonza4X.gltf");

	// Attach a move script to the camera component in the scene
	auto &camera_node = vkb::add_free_camera(get_scene(), "main_camera", get_render_context().get_surface_extent());
	camera            = dynamic_cast<vkb::sg::PerspectiveCamera *>(&camera_node.get_component<vkb::sg::Camera>());

	vkb::ShaderSource vert_shader("base.vert");
	vkb::ShaderSource frag_shader("base.frag");
	auto              scene_subpass   = std::make_unique<vkb::ForwardSubpass>(get_render_context(), std::move(vert_shader), std::move(frag_shader), get_scene(), *camera);
	auto              render_pipeline = std::make_unique<vkb::RenderPipeline>();
	render_pipeline->add_subpass(std::move(scene_subpass));
	set_render_pipeline(std::move(render_pipeline));

	// Add a GUI with the stats you want to monitor
	get_stats().request_stats({vkb::StatIndex::frame_times});
	create_gui(*window, &get_stats());

	return true;
}

void DescriptorManagement::update(float delta_time)
{
	// don't call the parent's update, because it's done differently here... but call the grandparent's update for fps logging
	vkb::Application::update(delta_time);

	update_scene(delta_time);

	update_gui(delta_time);

	auto &render_context = get_render_context();

	auto command_buffer = render_context.begin();

	update_stats(delta_time);

	// Process GUI input
	auto buffer_alloc_strategy = (buffer_allocation.value == 0) ?
	                                 vkb::rendering::BufferAllocationStrategy::OneAllocationPerBuffer :
	                                 vkb::rendering::BufferAllocationStrategy::MultipleAllocationsPerBuffer;

	render_context.get_active_frame().set_buffer_allocation_strategy(buffer_alloc_strategy);

	auto descriptor_management_strategy = (descriptor_caching.value == 0) ?
	                                          vkb::rendering::DescriptorManagementStrategy::CreateDirectly :
	                                          vkb::rendering::DescriptorManagementStrategy::StoreInCache;

	render_context.get_active_frame().set_descriptor_management_strategy(descriptor_management_strategy);

	command_buffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	get_stats().begin_sampling(*command_buffer);

	draw(*command_buffer, render_context.get_active_frame().get_render_target());

	get_stats().end_sampling(*command_buffer);
	command_buffer->end();

	render_context.submit(command_buffer);
}

void DescriptorManagement::draw_gui()
{
	auto lines = radio_buttons.size();
	if (camera->get_aspect_ratio() < 1.0f)
	{
		// In portrait, show buttons below heading
		lines = lines * 2;
	}

	get_gui().show_options_window(
	    /* body = */ [this, lines]() {
		    // For every option set
		    for (size_t i = 0; i < radio_buttons.size(); ++i)
		    {
			    // Avoid conflicts between buttons with identical labels
			    ImGui::PushID(vkb::to_u32(i));

			    auto &radio_button = radio_buttons[i];

			    ImGui::Text("%s: ", radio_button->description);

			    if (camera->get_aspect_ratio() > 1.0f)
			    {
				    // In landscape, show all options following the heading
				    ImGui::SameLine();
			    }

			    // For every option
			    for (size_t j = 0; j < radio_button->options.size(); ++j)
			    {
				    ImGui::RadioButton(radio_button->options[j], &radio_button->value, vkb::to_u32(j));

				    if (j < radio_button->options.size() - 1)
				    {
					    ImGui::SameLine();
				    }
			    }

			    ImGui::PopID();
		    }
	    },
	    /* lines = */ vkb::to_u32(lines));
}

std::unique_ptr<vkb::VulkanSampleC> create_descriptor_management()
{
	return std::make_unique<DescriptorManagement>();
}
