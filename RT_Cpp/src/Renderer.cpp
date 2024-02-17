#include "Renderer.h"
#include "Walnut/Random.h"
#include "Camera.h"
#include "execution"
#include <iostream>


namespace Utils {
	static uint32_t ConvertToRGBA(const glm::vec4& color) {
		uint8_t r = (uint8_t)(color.r * 255.0f);
		uint8_t g = (uint8_t)(color.g * 255.0f);
		uint8_t b = (uint8_t)(color.b * 255.0f);
		uint8_t a = (uint8_t)(color.a * 255.0f);
		uint32_t result = (a << 24) | (b << 16) | (g << 8) | r;
		return result;
	}
}

void Renderer::OnResize(uint32_t width, uint32_t height) {
	if (m_FinalImage) {		
		// No Resize Needed		
		if (m_FinalImage->GetWidth() == width && m_FinalImage->GetHeight() == height)
			return;
		m_FinalImage->Resize(width, height);
	}
	else {
		m_FinalImage = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);
	}
	delete[] m_ImageData;
	m_ImageData = new uint32_t[width * height];
	
	delete[] m_AccumalationData;
	m_AccumalationData = new glm::vec4[width * height];

	m_ImageHorizontalIter.resize(width);
	m_ImageVerticalIter.resize(height);
	for (uint32_t i = 0; i < width; i++)
		m_ImageHorizontalIter[i] = i;
	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIter[i] = i;
}

void Renderer::Render(const Scene& scene, const Camera& camera) {
	m_ActiveScene = &scene;	
	m_ActiveCamera = &camera;
	
	// Reset Accumalation Data If Needed
	if (m_FrameIndex == 1)
		memset(m_AccumalationData, 0, m_FinalImage->GetWidth() * m_FinalImage->GetHeight() * sizeof(glm::vec4));

// Set To 1 For Multi-Threading and 0 for Single Thread
#define MT 1
#if MT
	// Parallel For Each
	std::for_each(std::execution::par, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(), [this](uint32_t y) {
		std::for_each(std::execution::par, m_ImageHorizontalIter.begin(), m_ImageHorizontalIter.end(), [this, y](uint32_t x) {
			glm::vec4 color = PerPixel(x, y);
			m_AccumalationData[x + y * m_FinalImage->GetWidth()] += color;

			glm::vec4 accumulatedColor = m_AccumalationData[x + y * m_FinalImage->GetWidth()];
			accumulatedColor /= (float)m_FrameIndex;

			accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(accumulatedColor);
		});
	});
#else
	// Render Every Pixel
	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++) {
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++) {			
			glm::vec4 color = PerPixel(x, y);
			m_AccumalationData[x + y * m_FinalImage->GetWidth()] += color;

			glm::vec4 accumulatedColor = m_AccumalationData[x + y * m_FinalImage->GetWidth()];
			accumulatedColor /= (float)m_FrameIndex;

			accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y*m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(accumulatedColor);
		}
	}
#endif
	
	m_FinalImage->SetData(m_ImageData);
	m_FrameIndex = (m_Settings.Accumulate) ? m_FrameIndex + 1 : 1;
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y)
{
	Ray ray;
	ray.Origin = m_ActiveCamera->GetPosition();
	ray.Direction = m_ActiveCamera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];

	glm::vec3 color(0.0f);
	float multiplier = 1.0f;
	int bounces = 5;
	for (int i = 0; i < bounces; i++) {
		Renderer::HitPayload payload = TraceRay(ray);
		if (payload.hitDistance < 0.0f) {		
			glm::vec3 skyColor = glm::vec3(0.6f, 0.7f, 0.9f);
			color += skyColor * multiplier;
			break;
		}
		glm::vec3 lightDir = glm::normalize(glm::vec3(-1, -1, -1));
		float diffuse = glm::max(glm::dot(payload.WorldNormal, -lightDir), 0.0f); // == cos(angle)

		const Sphere& sphere = m_ActiveScene->Spheres[payload.ObjectIndex];
		const Material& material = m_ActiveScene->Materials[sphere.MaterialIndex];

		glm::vec3 sphereColor = material.Albedo;
		sphereColor *= diffuse;
		color += sphereColor * multiplier;

		multiplier *= 0.5f;

		// Move small distance outside the sphere to not collide with it
		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;
		ray.Direction = glm::reflect(ray.Direction, payload.WorldNormal + material.Roughness * Walnut::Random::Vec3(-0.5f, 0.5f));
	}	
	return glm::vec4(color, 1);
}

Renderer::HitPayload Renderer::TraceRay(const Ray& ray) {

	// (bx^2 + by^2  bz^2)t^2 + 2(axbx + ayby + azbz)t + (ax^2 + ay^2  + az^2- r^2) = 0	
	// a is ray origin
	// b is ray direction
	// r is sphere radius
	// t is distance along ray	
	int closestSphere = -1;
	float hitDistance = FLT_MAX;
	for (size_t i = 0; i < m_ActiveScene->Spheres.size(); i++) {
		const Sphere& sphere = m_ActiveScene->Spheres[i];
		glm::vec3 spherePos = ray.Origin - sphere.Position;
		float a = glm::dot(ray.Direction, ray.Direction);
		float b = 2.0f * glm::dot(spherePos, ray.Direction);
		float c = glm::dot(spherePos, spherePos) - (sphere.Radius*sphere.Radius);

		// Quadratic Formula discriminant
		// b^2 - 4ac
		float discriminant = b*b - 4*a*c;
		if (discriminant < 0.0f)
			continue;

		// (-b +- sqrt(discrimant)) / 2a
		// float t0 = (-b + glm::sqrt(discriminant)) / (2.0f * a);
		float closestT = (-b - glm::sqrt(discriminant)) / (2.0f * a);

		if (closestT > 0.0f && closestT < hitDistance) {
			hitDistance = closestT;
			closestSphere = (int)i;
		}
	}
	if (closestSphere < 0)
		return Miss(ray);

	return ClosestHit(ray, hitDistance, closestSphere);	
}

Renderer::HitPayload Renderer::ClosestHit(const Ray& ray, float HitDistance, int objectIndex)
{
	Renderer::HitPayload payload;
	payload.hitDistance = HitDistance;
	payload.ObjectIndex = objectIndex;

	const Sphere &closestSphere = m_ActiveScene->Spheres[objectIndex];

	glm::vec3 spherePos = ray.Origin - closestSphere.Position;
	payload.WorldPosition = spherePos + (HitDistance * ray.Direction);
	payload.WorldNormal = glm::normalize(payload.WorldPosition);
	
	payload.WorldPosition += closestSphere.Position;

	return payload;

}

Renderer::HitPayload Renderer::Miss(const Ray& ray)
{
	Renderer::HitPayload payload;
	payload.hitDistance = -1.0f;
	return payload;
}