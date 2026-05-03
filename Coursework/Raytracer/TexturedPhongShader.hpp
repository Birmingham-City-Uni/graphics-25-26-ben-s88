#pragma once
#include "Shader.hpp"

/// <summary>
/// Lambertian reflectance shader that samples albedo values from a texture.
/// The texture should be stored as an image file (TGAImage instance).
/// </summary>
class TexturedPhongShader : public Shader
{
private:
	const std::vector<uint8_t>* albedoTexture_;
	const int texWidth_, texHeight_;
	Eigen::Vector3f specular_;
	float shininess_;
	bool shadowTest_;
public:
	TexturedPhongShader(const std::vector<uint8_t>* albedoTexture, int texWidth, int texHeight, const Eigen::Vector3f& specular={1.0f, 1.0f, 1.0f}, float shininess=100.f, bool shadowTest = true)
		:shadowTest_(shadowTest), albedoTexture_(albedoTexture),
		texWidth_(texWidth), texHeight_(texHeight), specular_(specular), shininess_(shininess)
	{
	}

	virtual Eigen::Vector3f getColor(const HitInfo& hitInfo,
		const Renderable* scene,
		const std::vector<std::unique_ptr<Light>>& lights,
		const Eigen::Vector3f& ambientLight,
		int currBounceCount,
		const int maxBounces) const
	{
		Eigen::Vector3f albedo(0.f, 0.f, 0.f);

		Eigen::Vector2f tex = hitInfo.texCoords;
		int pixX = static_cast<int>(tex.x() * texWidth_);
		int pixY = static_cast<int>((1.f - tex.y()) * texHeight_);
		pixX = std::max(pixX, 0);
		pixY = std::max(pixY, 0);
		pixX = std::min(pixX, texWidth_ - 1);
		pixY = std::min(pixY, texHeight_ - 1);

		albedo.x() = static_cast<float>((*albedoTexture_)[(pixX + texWidth_ * pixY) * 4 + 0]) / 255.f;
		albedo.y() = static_cast<float>((*albedoTexture_)[(pixX + texWidth_ * pixY) * 4 + 1]) / 255.f;
		albedo.z() = static_cast<float>((*albedoTexture_)[(pixX + texWidth_ * pixY) * 4 + 2]) / 255.f;

		Eigen::Vector3f color = coefftWiseMul(albedo, ambientLight);

		for (auto& light : lights) {
			float influenceWeighted = 1.f;
			if (shadowTest_ && light->type != light->POINT) {
				if (!light->visibilityCheck(hitInfo.location, scene))
					continue;
			}
			else if (shadowTest_ && light->type == light->POINT)
			{
				int totalInfluence = 0;
				int gridSize = 4;
				float gridOffset = 0.4f;
				const PointLight* pl = dynamic_cast<PointLight*>(light.get());
				if (!pl) { continue; }
				for (int y = 0; y < gridSize; y++)
					for (int x = 0; x < gridSize; x++)
					{ // jitter the rays and whatever
						float randX = ((std::rand() & 11) - 5) / 10.f;
						float randY = ((std::rand() & 11) - 5) / 10.f;
						if (pl->visibilityCheckWithOffset(hitInfo.location, scene, Eigen::Vector3f((x - (gridSize / 2.f)) * gridOffset + (randX * gridOffset), 0.f, (y - (gridSize / 2.f)) * gridOffset + (randY * gridOffset))))
						{
							totalInfluence++;
						}
							
					}
				influenceWeighted = totalInfluence / pow(gridSize, 2);
				
			}
			Eigen::Vector3f lightVec = light->getVecToLight(hitInfo.location);
			float dotProd = std::max(lightVec.dot(hitInfo.normal), 0.f);
			color += (dotProd * coefftWiseMul(light->getIntensity(hitInfo.location) * influenceWeighted, albedo));

			Eigen::Vector3f reflectVec = reflect(hitInfo.inDirection, hitInfo.normal);
			float dotSpec = std::max(lightVec.dot(reflectVec), 0.f);
			dotSpec = powf(dotSpec, shininess_);
			color += dotSpec * coefftWiseMul(light->getIntensity(hitInfo.location), specular_);
		}

		return color;
	}
};

