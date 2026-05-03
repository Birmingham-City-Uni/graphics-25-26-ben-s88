#include <Eigen/Dense>
#include <lodepng.h>
#include <json/json.hpp>
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include "BVHNode.hpp"
#include "Triangle.hpp"
#include "Scene.hpp"
#include "Camera.hpp"
#include "PointLight.hpp"
#include "DirectionalLight.hpp"
#include "LambertianShader.hpp"
#include "TexturedLambertianShader.hpp"
#include "PhongShader.hpp"
#include "MirrorShader.hpp"
#include "TexCoordTestShader.hpp"
#include "Model.hpp"
#include "GlassShader.hpp"
#include "TexturedPhongShader.hpp"
#include <fstream>

/// <summary>
/// Load a JSON config file using the nlohmann library.
/// </summary>
nlohmann::json loadConfig(const std::string& filename)
{
	std::ifstream configStream(filename);
	nlohmann::json config = nlohmann::json::parse(configStream);
	return config;
}

/// <summary>
/// Load an Eigen Vector3f from a config file.
/// Call as for example loadVec3FromConfig(config["myVector3"]);
/// </summary>
Eigen::Vector3f loadVec3FromConfig(const nlohmann::json& config)
{
	return Eigen::Vector3f(config[0], config[1], config[2]);
}

struct Color {
	uint8_t r, g, b, a;
};


void setPixel(std::vector<uint8_t>& image, int x, int y, int width, int height, const Color& color)
{
	int pixelIdx = x + y * width;
	image[pixelIdx * 4 + 0] = color.r;
	image[pixelIdx * 4 + 1] = color.g;
	image[pixelIdx * 4 + 2] = color.b;
	image[pixelIdx * 4 + 3] = color.a;
}

Color getPixel(const std::vector<uint8_t>& image, int x, int y, int width, int height)
{
	int pixelIdx = x + y * width;
	Color color{
		image[pixelIdx * 4 + 0],
		image[pixelIdx * 4 + 1],
		image[pixelIdx * 4 + 2],
		image[pixelIdx * 4 + 3]
	};
	return color;
}

void doSSAA(std::vector<uint8_t>& imgBuffer, int height, int width, std::vector<uint8_t>& outputBuffer)
{
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			if (x % 2 == 0 && y % 2 == 0)
			{
				int pixelIdx = x + y * width;

				int r = getPixel(imgBuffer, x, y, width, height).r + getPixel(imgBuffer, x + 1, y, width, height).r + getPixel(imgBuffer, x, y + 1, width, height).r + getPixel(imgBuffer, x + 1, y + 1, width, height).r;
				r /= 4;
				int g = getPixel(imgBuffer, x, y, width, height).g + getPixel(imgBuffer, x + 1, y, width, height).g + getPixel(imgBuffer, x, y + 1, width, height).g + getPixel(imgBuffer, x + 1, y + 1, width, height).g;
				g /= 4;
				int b = getPixel(imgBuffer, x, y, width, height).b + getPixel(imgBuffer, x + 1, y, width, height).b + getPixel(imgBuffer, x, y + 1, width, height).b + getPixel(imgBuffer, x + 1, y + 1, width, height).b;
				b /= 4;
				Color color{ r, g, b, 255 };

				setPixel(outputBuffer, x / 2, y / 2, width / 2, height / 2, color);
			}
		}
	}
}

int main(int argc, char* argv[]) {

	// *** Load the config file ***
	auto config = loadConfig("../config/config.json");
	bool SSAA = false;
	bool multiSample = false;
	int multiSampleGridSize = 2; // size^2 samples per pixel

	int pixHeight = config["pixHeight"], pixWidth = config["pixWidth"];
	int outputHeight = 1080, outputWidth = 1920;
	const int nChannels = 4;

	if (SSAA)
	{
		pixHeight *= 2;
		pixWidth *= 2;
	}

	// *** Set up camera and output image ***
	Camera cam(
		loadVec3FromConfig(config["cameraPos"]),
		loadVec3FromConfig(config["cameraForward"]),
		loadVec3FromConfig(config["cameraUp"]),
		pixWidth, pixHeight,
		config["cameraFov"]);


	std::vector<uint8_t> outImage(pixHeight * pixWidth * nChannels);

	Eigen::Vector3f
		red(1.f, 0.f, 0.f),
		blue(0.f, 0.f, 1.f),
		aqua(0.f, .8f, .8f),
		lavender(178.f / 255.f, 164.f / 255.f, 212.f / 255.f);

	// *** Load shaders and textures ***
	std::vector<uint8_t> jamesTexture;
	unsigned int jamesWidth, jamesHeight;
	lodepng::decode(jamesTexture, jamesWidth, jamesHeight, "../../Models/james-sunderland/textures/JamesTextureAtlas.png");

	std::vector<uint8_t> groundTexture;
	unsigned int groundTexWidth, groundTexHeight;
	lodepng::decode(groundTexture, groundTexWidth, groundTexHeight, "../../textures/Pavement/worn_tile_floor_diff_4k.png");

	std::vector<uint8_t> BGTexture;
	unsigned int BGTexWidth, BGTexHeight;
	lodepng::decode(BGTexture, BGTexWidth, BGTexHeight, "../../textures/SkyboxFlip.png");

	std::vector<uint8_t> BDTexture;
	unsigned int BDTexWidth, BDTexHeight;
	lodepng::decode(BDTexture, BDTexWidth, BDTexHeight, "../../textures/Wall/grey_plaster_03_diff_1k.png");

	std::vector<uint8_t> carTexture;
	unsigned int carTexWidth, carTexHeight;
	lodepng::decode(carTexture, carTexWidth, carTexHeight, "../../Models/pontiac-ventura/textures/ventura_d.png");

	LambertianShader redLambertianShader(red);
	PhongShader bluePlasticShader(blue, Eigen::Vector3f(1.f, 1.f, 1.f), 100.f);
	LambertianShader aquaLambertianShader(aqua);
	LambertianShader lavenderLambertianShader(lavender);
	TexturedPhongShader jamesShader(&jamesTexture, jamesWidth, jamesHeight);
	TexturedPhongShader groundShader(&groundTexture, groundTexWidth, groundTexHeight);
	TexturedPhongShader BGShader(&BGTexture, BGTexWidth, BGTexHeight);
	TexturedPhongShader BDShader(&BDTexture, BDTexWidth, BDTexHeight);
	TexturedPhongShader carShader(&carTexture, carTexWidth, carTexHeight);
	MirrorShader mirrorShader;
	TexCoordTestShader texCoordTestShader;
	GlassShader glassShader(1.01f, Eigen::Vector3f(1.f, 1.f, 1.f) * 0.7f, 100);
	// make transparent shader

	// *** Set up scene ***
	Scene scene;

	// Optional code: here's how to add the spot mesh to the scene, using a BVH
	// Try enabling this and comparing it to the non-BVH version below!

	Model groundModel("../../Models/CarPark.obj");
	scene.renderables.push_back(std::make_shared<BVHNode>(groundModel, &groundShader, 4, makeTranslationMatrix(Eigen::Vector3f(1.f, -1.2f, 2.f))));

	Model jamesModel("../../Models/JamesExport3.obj");
	scene.renderables.push_back(std::make_shared<BVHNode>(jamesModel, &jamesShader, 4, makeTranslationMatrix(Eigen::Vector3f(1.4f, -0.7f, 8.5f)) * uniformScale(1.f) * rotateY(190 * M_PI / 180)));
	
	Model BGModel("../../Models/PlaneExport2.obj");
	//scene.renderables.push_back(std::make_shared<BVHNode>(BGModel, &BGShader, ~SHADOW_BITMASK, 4, makeTranslationMatrix(Eigen::Vector3f(-7.4f, 0.1f, 12.f)) * uniformScale(12) * rotateY(0 * M_PI / 180)));
	scene.renderables.push_back(std::make_shared<Mesh>(&BGShader, &BGModel, nullptr, true, true, VISIBLE_BITMASK));
	scene.renderables.back()->modelToWorld(makeTranslationMatrix(Eigen::Vector3f(-7.5f, 0.1f, 12.f)) * uniformScale(12) * rotateY(0 * M_PI / 180));

	Model BDModel("../../Models/BuildingExport.obj");
	scene.renderables.push_back(std::make_shared<BVHNode>(BDModel, &BDShader, 4, makeTranslationMatrix(Eigen::Vector3f(2.8f, -1.f, 4.8f)) * uniformScale(0.8) * rotateY(0 * M_PI / 180) * rotateX(3.3 * M_PI / 180)));

	Model carModel("../../Models/CarExport.obj");
	auto carTransform = makeTranslationMatrix(Eigen::Vector3f(-.4f, -0.85f, 4.5f)) * uniformScale(1) * rotateY(-25 * M_PI / 180) * rotateX(1.5 * M_PI / 180);
	scene.renderables.push_back(std::make_shared<BVHNode>(carModel, &carShader, 4, carTransform));

	Model carWindowModel("../../Models/CarWindowExport.obj");
	//scene.renderables.push_back(std::make_shared<BVHNode>(carWindowModel, &glassShader, 4, carTransform));
	scene.renderables.push_back(std::make_shared<Mesh>(&glassShader, &carWindowModel, nullptr, true, true, VISIBLE_BITMASK));
	scene.renderables.back()->modelToWorld(carTransform);

	// Here's how to add the mesh without using the BVH.
	// Try comparing performance to the BVH version above.
	//Model spotModel("../models/spot.obj");
	//scene.renderables.push_back(std::make_shared<Mesh>(&spotShader, &spotModel));
	//scene.renderables.back()->modelToWorld(rotateY(M_PI / 4.0f));

	// *** Add lights to scene ***
	Eigen::Vector3f ambientLight(.5f, .5f, .5f);

	std::vector<std::unique_ptr<Light>> lightSources;
	//lightSources.push_back(std::make_unique<PointLight>(Eigen::Vector3f(0.f, 0.f, 4.f), 3.f * Eigen::Vector3f(1.f, 1.f, 1.f)));
	//lightSources.push_back(std::make_unique<DirectionalLight>(Eigen::Vector3f(-0.5f, -1.5f, -1.f), 0.7f * Eigen::Vector3f(1.f, 1.f, 1.f)));

	// *** Render the scene ***

	// Shuffling the scanline order gets better CPU usage between threads
	// when some lines take longer to render than others.
	std::vector<unsigned int> scanlines(pixHeight);
	for (int i = 0; i < pixHeight; ++i) scanlines[i] = i;

	if (config["shuffleScanlines"]) {
		std::random_device rd;
		std::mt19937 g(rd());
		std::shuffle(scanlines.begin(), scanlines.end(), g);
	}

	auto startTime = std::chrono::steady_clock::now();

	Ray ray = cam.getRay(531, 325);
	HitInfo hitInfo;
	scene.intersect(ray, 1e-6f, 1e6f, hitInfo, VISIBLE_BITMASK);
	float x = hitInfo.hitT;

	if (multiSample)
	{
	int samplesPerPixel = pow(multiSampleGridSize, 2);
	#pragma omp parallel for
	for (int y = 0; y < pixHeight; ++y) {
		for (int x = 0; x < pixWidth; ++x) {
			Eigen::Vector3f colorToAverage{ 0, 0, 0};
			for (auto const &ray : cam.getRays(x, scanlines[y], multiSampleGridSize))
			{
			HitInfo hitInfo;
			if (scene.intersect(ray, 1e-6f, 1e6f, hitInfo, VISIBLE_BITMASK)) {
				Eigen::Vector3f color = hitInfo.shader->getColor(
					hitInfo, &scene,
					lightSources, ambientLight,
					0, config["maxBounces"]);

				color.x() = std::min(color.x(), 1.f);
				color.y() = std::min(color.y(), 1.f);
				color.z() = std::min(color.z(), 1.f);


				colorToAverage.x() += color.x() * 255;
				colorToAverage.y() += color.y() * 255;
				colorToAverage.z() += color.z() * 255;
			}
			else {
				continue;
			}
			}

			colorToAverage.x() /= samplesPerPixel;
			colorToAverage.y() /= samplesPerPixel;
			colorToAverage.z() /= samplesPerPixel;

			int line = (pixHeight - scanlines[y]) - 1;
			outImage[(x + line * pixWidth) * nChannels + 0] = colorToAverage.x();
			outImage[(x + line * pixWidth) * nChannels + 1] = colorToAverage.y();
			outImage[(x + line * pixWidth) * nChannels + 2] = colorToAverage.z();
			outImage[(x + line * pixWidth) * nChannels + 3] = 255;
			
		}
		if (omp_get_thread_num() == omp_get_num_threads() - 1) {
			std::clog << "\rScanlines remaining: " << (pixHeight - y) << ' ' << std::flush;
		}
	}
	}
	else
	{
	#pragma omp parallel for
	for (int y = 0; y < pixHeight; ++y) {
		for (int x = 0; x < pixWidth; ++x) {
			Ray ray = cam.getRay(x, scanlines[y]);
			HitInfo hitInfo;
			if (scene.intersect(ray, 1e-6f, 1e6f, hitInfo, VISIBLE_BITMASK)) {
				Eigen::Vector3f color = hitInfo.shader->getColor(
					hitInfo, &scene,
					lightSources, ambientLight,
					0, config["maxBounces"]);

				color.x() = std::min(color.x(), 1.f);
				color.y() = std::min(color.y(), 1.f);
				color.z() = std::min(color.z(), 1.f);


				int line = (pixHeight - scanlines[y]) - 1;
				outImage[(x + line * pixWidth) * nChannels + 0] = color.x() * 255;
				outImage[(x + line * pixWidth) * nChannels + 1] = color.y() * 255;
				outImage[(x + line * pixWidth) * nChannels + 2] = color.z() * 255;
				outImage[(x + line * pixWidth) * nChannels + 3] = 255;
			}
			else {
				int line = (pixHeight - scanlines[y]) - 1;
				outImage[(x + line * pixWidth) * nChannels + 0] = 0;
				outImage[(x + line * pixWidth) * nChannels + 1] = 0;
				outImage[(x + line * pixWidth) * nChannels + 2] = 0;
				outImage[(x + line * pixWidth) * nChannels + 3] = 255;
			}
		}
		if (omp_get_thread_num() == omp_get_num_threads()-1) {
			std::clog << "\rScanlines remaining: " << (pixHeight - y) << ' ' << std::flush;
		}
	}
	}

	auto renderTime = std::chrono::steady_clock::now() - startTime;

	std::cout << "Render duration " << std::chrono::duration_cast<std::chrono::milliseconds>(renderTime).count() * 1e-3f << " seconds." << std::endl;

	// *** Save the output image ***
	int errorCode = 0;
	if (SSAA)
	{
		std::vector<uint8_t> outputBuffer(outputWidth * outputHeight * nChannels);
		doSSAA(outImage, pixHeight, pixWidth, outputBuffer);

		errorCode = lodepng::encode(config["outputFilename"], outputBuffer, outputWidth, outputHeight);
	}
	else
	{
		errorCode = lodepng::encode(config["outputFilename"], outImage, outputWidth, outputHeight);
	}
	/*errorCode = lodepng::encode(config["outputFilename"], outImage, pixWidth, pixHeight);
	if (errorCode) { // check the error code, in case an error occurred.
		std::cout << "lodepng error encoding image: " << lodepng_error_text(errorCode) << std::endl;
		return errorCode;
	}*/

	return 0;
}
