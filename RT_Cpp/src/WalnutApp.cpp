#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "Walnut/Timer.h"
#include "src/Renderer.h"
#include "src/Camera.h"
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
using namespace Walnut;

class ExampleLayer : public Walnut::Layer
{
public:
	ExampleLayer()
		: m_Camera(45.0f, 0.1f, 100.0f) {
		// Initialize Materials
		Material &pinkSphere = m_Scene.Materials.emplace_back();
		pinkSphere.Albedo = glm::vec3(1.0f, 0.0f, 1.0f);
		pinkSphere.Roughness = 0.0f;

		Material &blueSphere = m_Scene.Materials.emplace_back();
		blueSphere.Albedo = glm::vec3(0.2f, 0.3f, 1.0f);
		blueSphere.Roughness = 0.1f;

		// Initialize Sphere Details
		{
			Sphere sphere;
			sphere.Position = glm::vec3(0, 0, 0);
			sphere.Radius = 1.0f;
			sphere.MaterialIndex = 0;
			m_Scene.Spheres.push_back(sphere);
		}
		{
			Sphere sphere;
			sphere.Position = glm::vec3(0,-101, -5);
			sphere.Radius = 100.0f;
			sphere.MaterialIndex = 1;
			m_Scene.Spheres.push_back(sphere);
		}
	}

	virtual void OnUpdate(float ts) override {
		// Update Camera	
		if (m_Camera.OnUpdate(ts))
			m_Renderer.ResetFrameIndex();
	}
	virtual void OnUIRender() override {
		ImGui::Begin("Settings");
		ImGui::Text("Last Render: %.3fms", m_LastRenderTime);

		if (ImGui::Button("Render")) {
			Render();
		}
		ImGui::Checkbox("Accumulate", &m_Renderer.GetSettings().Accumulate);
		if (ImGui::Button("Reset")) {
			m_Renderer.ResetFrameIndex();
		}
		ImGui::End();
		ImGui::Begin("Scene");
		for (size_t i = 0; i < m_Scene.Spheres.size(); i++) {
			ImGui::PushID(i); // Unique ID For each Sphere
			Sphere &sphere = m_Scene.Spheres[i];
			ImGui::DragFloat3("Sphere Position", glm::value_ptr(sphere.Position), 0.1f);
			ImGui::DragFloat("Sphere Radius", &sphere.Radius, 0.1f);
			ImGui::DragInt("Material", &sphere.MaterialIndex, 1, 0, (int)m_Scene.Materials.size()-1);

			ImGui::NewLine();
			ImGui::Separator();
			ImGui::PopID();
		}
		for (size_t i = 0; i < m_Scene.Materials.size(); i++) {
			ImGui::PushID(i); // Unique ID For each Sphere
			Material& material = m_Scene.Materials[i];

			ImGui::ColorEdit3("Sphere Color (Albedo)", glm::value_ptr(material.Albedo));
			ImGui::DragFloat("Sphere Rougness", &material.Roughness, 0.05f, 0.0f, 1.0f);
			ImGui::DragFloat("Sphere Metallic", &material.Metallic, 0.05f, 0.0f, 1.0f);

			ImGui::NewLine();
			ImGui::Separator();
			ImGui::PopID();
		}
		ImGui::End();

		// Remove Borders From Window
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		ImGui::Begin("Viewport");
		m_ViewportWidth = ImGui::GetContentRegionAvail().x;
		m_ViewportHeight = ImGui::GetContentRegionAvail().y;
		auto image = m_Renderer.GetFinalImage();
		if (image)
			ImGui::Image(image->GetDescriptorSet(), { (float)image->GetWidth(), (float)image->GetHeight() }, 
			ImVec2(0,1), ImVec2(1,0));
			// ^ Flips Y Axis, (Top Is Max Y)
		ImGui::End();
		ImGui::PopStyleVar();

		Render();
	}

	void Render() {
		Timer timer;				
		m_Renderer.OnResize(m_ViewportWidth, m_ViewportHeight); // Renderer Resize
		m_Camera.OnResize(m_ViewportWidth, m_ViewportHeight); // Camera Resize
		m_Renderer.Render(m_Scene, m_Camera); // Renderer Render
		m_LastRenderTime = timer.Elapsed(); // Set Elapsed Time
	}

private:	
	Renderer m_Renderer;
	Camera m_Camera;
	Scene m_Scene;
	uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;	
	float m_LastRenderTime = 0.0f;	
};

Walnut::Application* Walnut::CreateApplication(int argc, char** argv) {
	Walnut::ApplicationSpecification spec;
	spec.Name = "Ray Tracing C++";

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<ExampleLayer>();
	app->SetMenubarCallback([app]() {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Exit")) {
				app->Close();
			}
			ImGui::EndMenu();
		}
	});
	return app;
}