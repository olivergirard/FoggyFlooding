#include <iostream>
#include <vector>
#include <stdint.h>
#include <tuple>
#include <random>

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp> 
#include <glm/gtc/matrix_transform.hpp>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"

#include "ProceduralCity.h"

// Noise library
#include "FastNoise.h"

using namespace std;

/* Reduce this number to speed up compilation time. */
const int NUM_PARTICLES = 10;

const int screenWidth = 1200;
const int screenHeight = 1200;
glm::vec4 background = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

/* Do not modify these as they are essential for ray tracing. */
glm::vec3 eye = glm::vec3(60.0f, 30.0f, 30.0f);
glm::vec3 lookAt = glm::vec3(15.0f, 10.0f, 8.0f);
glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

const GLfloat EPSILON = 1e-2;

bool timeToRender = false;
bool moveWater = false;
int frameCount = 3;
int currentFrameIndex = 0;

// Perlin noise generator
FastNoise perlinNoise;

struct Ray {
	glm::vec3 origin;
	glm::vec3 direction;
};

struct Light {
	glm::vec3 position;
	glm::vec3 diff;
	glm::vec3 spec;
};

GLuint cityTexture = -1;

vector<Light> lights;
vector<Surface> particles;
vector<glm::vec4> colors;
vector<Surface> buildings;
vector<Surface> surfaces;

vector<unsigned char*> frames;

int perlinSeed = 12345;
float perlinFrequency = 0.01;

vector<glm::vec4> generatePerlinTexture() {
	// generate perlin noise
	perlinNoise.SetNoiseType(FastNoise::NoiseType::Perlin);
	perlinNoise.SetFrequency(perlinFrequency);

	std::vector<glm::vec4> _PerlinRawValues;

	perlinNoise.SetSeed(perlinSeed);

	for (int x = 0; x < screenWidth; x++) {
		for (int y = 0; y < screenHeight; y++) {
			float noiseVal = perlinNoise.GetNoise(x, y);

			float normalizedNoiseVal = ((noiseVal + 1.0f) * 0.5f);
			if (normalizedNoiseVal > 1.0) {
				normalizedNoiseVal = 1.0;
			}
			else if (normalizedNoiseVal < 0.0) {
				normalizedNoiseVal = 0.0;
			}
			// std::cout << normalizedNoiseVal << std::endl;

			glm::vec4 temp = glm::vec4(normalizedNoiseVal, normalizedNoiseVal, normalizedNoiseVal, normalizedNoiseVal);

			_PerlinRawValues.push_back(temp);
		}
	}

	return _PerlinRawValues;

	/*
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 800, 800, 0, GL_RGBA, GL_FLOAT, _PerlinRawValues.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	return texture;
	*/
}

/* Generates the initial position for the particles that comprise the water. */
glm::vec3 GeneratePosition() {

	std::random_device rd;
	std::mt19937 gen(rd());

	/* Determines the shape of the water triangle. */
	float minX = 10.0f; //decreasing x floods the city
	float maxX = 30.0f;
	float minZ = -30.0f;
	float maxZ = 30.0f;

	std::uniform_real_distribution<float> disX(minX, maxX);
	std::uniform_real_distribution<float> disZ(minZ, maxZ);

	GLfloat x = disX(gen);
	GLfloat z = disZ(gen);

	return glm::vec3(x, 0, z);
}

/* Creates any particles necessary for the scene. */
vector<Surface> CreateParticles(GLuint numParticles) {

	vector<Surface> particles;

	srand(time(NULL));

	Surface p;

	for (int i = 0; i < numParticles; i++) {

		p.position = GeneratePosition();

		p.diff = glm::vec3(0.0f, (double)rand() / RAND_MAX, 1.0f);
		p.type = SPHERE;
		p.radius = 1.0f;
		particles.push_back(p);
	}

	return particles;
}

/* Creates any lights necessary to illuminate the scene. */
vector<Light> CreateLights() {

	Light l;

	l.position = glm::vec3(-5.0f, 10.0f, 10.0f);
	l.diff = glm::vec3(1, 1, 1);
	l.spec = glm::vec3(0, 0, 0);

	lights.push_back(l);
	return lights;
}

/* Calculates the ray going through the pixel at the given location. */
Ray CalculateRay(GLuint pixelWidth, GLuint pixelHeight) {

	GLfloat fovy = 45.0f;
	GLfloat aspectRatio = screenWidth / screenHeight;
	GLfloat focalLength = 1.0f / tan(fovy / 2.0f);

	/* Gramm-Schmidt orthonormalization. */
	glm::vec3 l = glm::normalize(lookAt - eye);
	glm::vec3 v = glm::normalize(glm::cross(l, up));
	glm::vec3 u = glm::cross(v, l);

	glm::vec3 ll = eye + (focalLength * l) - (aspectRatio * v) - u;
	glm::vec3 p = ll + (2.0f * aspectRatio * v * (GLfloat)pixelWidth / (GLfloat)screenWidth) + (2.0f * u * (GLfloat)pixelHeight / (GLfloat)screenHeight);
	glm::vec3 direction = glm::normalize(p - eye);

	Ray ray = { eye, direction };

	return ray;
}

/* Calculates ray-sphere intersection and returns the associated t value. */
GLfloat SphereIntersection(const Ray& ray, const Surface& particle) {

	GLfloat a = glm::dot(ray.direction, ray.direction);
	GLfloat b = 2.0f * glm::dot(ray.origin - particle.position, ray.direction);
	GLfloat c = glm::dot(ray.origin - particle.position, ray.origin - particle.position) - pow(particle.radius, 2);

	GLfloat discriminant = pow(b, 2) - (4.0f * a * c);

	if (discriminant < 0.0f) {
		return -1.0f;
	}

	GLfloat t1 = (-b + sqrt(discriminant)) / (2.0f * a);
	GLfloat t2 = (-b - sqrt(discriminant)) / (2.0f * a);

	if ((t1 > 0.0f) && (t2 > 0.0f)) {
		return fmin(t1, t2);
	}
	else if (t1 > 0.0f) {
		return t1;
	}
	else if (t2 > 0.0f) {
		return t2;
	}

	/* -1.0f indicates no intersection and is checked in FindFirstIntersection(). */
	return -1.0f;
}

/* Calculates ray-triangle intersection and returns the associated t value. */
GLfloat TriangleIntersection(const Ray& ray, glm::vec3 pos1, glm::vec3 pos2, glm::vec3 pos3) {

	glm::vec3 p = pos2 - pos1;
	glm::vec3 q = pos3 - pos1;

	glm::vec3 temp = glm::cross(ray.direction, q);
	GLfloat dot = glm::dot(p, temp);

	if ((dot < EPSILON) && (dot > -EPSILON)) {
		return -1.0f;
	}

	GLfloat f = 1.0f / dot;
	glm::vec3 s = ray.origin - pos1;
	GLfloat u = f * glm::dot(s, temp);

	if ((u < 0.0f) || (u > 1.0f)) {
		return -1.0f;
	}

	temp = glm::cross(s, p);
	GLfloat v = f * glm::dot(ray.direction, temp);

	if ((v < 0.0f) || ((u + v) > 1.0f)) {
		return -1.0f;
	}

	GLfloat t = f * glm::dot(q, temp);
	return t;
}

/* Finds the first surface intersected by the ray. */
std::tuple<GLfloat, Surface> FindFirstIntersection(const Ray& ray) {

	GLfloat firstIntersection = std::numeric_limits<GLfloat>::infinity();
	Surface firstSurface;

	for (const Surface& surface : surfaces) {

		GLfloat intersection = -1.0f;

		if (surface.type == SPHERE) {
			intersection = SphereIntersection(ray, surface);
		}
		else if (surface.type == QUAD) {
			intersection = TriangleIntersection(ray, surface.pos1, surface.pos2, surface.pos3);

			/* If there was no intersection, check the other triangle making up the quad. */
			if (intersection < 0.0f) {
				intersection = TriangleIntersection(ray, surface.pos2, surface.pos4, surface.pos3);
			}
		}

		if ((intersection > 0.0f) && (intersection < firstIntersection)) {
			firstIntersection = intersection;
			firstSurface = surface;
		}
	}

	/* Indicates an invalid intersection TraceRay(). */
	if (firstIntersection == std::numeric_limits<GLfloat>::infinity()) {
		firstIntersection = -1.0f;
	}

	return std::tuple<GLfloat, Surface>(firstIntersection, firstSurface);
}

/* Calculates the normal vector for a sphere. */
glm::vec3 SphereNormal(const Ray& ray, const Surface& particle, GLfloat t) {

	glm::vec3 intersection = ray.origin + (t * ray.direction);
	glm::vec3 normal = glm::normalize(intersection - particle.position);

	return normal;
}

/* Calculates the normal vector for a triangle. */
glm::vec3 TriangleNormal(const Ray& ray, const Surface& triangle) {

	glm::vec3 normal = glm::cross(triangle.pos3 - triangle.pos1, triangle.pos2 - triangle.pos1);
	normal = glm::normalize(normal);

	return normal;
}

/* Returns a vector of all of the rays that are visible from the given point on the given surface. */
std::vector<Light> ShadowRays(const Ray& ray, GLfloat t, const Surface& surface) {

	std::vector<Light> visibleLights;
	glm::vec3 intersection = ray.origin + (t * ray.direction);

	for (Light& light : lights) {

		GLfloat distance = glm::length(light.position - intersection);
		GLfloat t = -1.0f;

		if (surface.type == SPHERE) {
			t = SphereIntersection(ray, surface);
		}
		else if (surface.type == QUAD) {
			t = TriangleIntersection(ray, surface.pos1, surface.pos2, surface.pos3);

			if (t < 0.0f) {
				t = TriangleIntersection(ray, surface.pos2, surface.pos4, surface.pos3);
			}
		}

		/* If the distance from the intersection point to the light is less than the distance from the ray to the intersection point. */
		if ((t > 0.0f) && (distance < t)) {
			visibleLights.push_back(light);
		}
	}

	return visibleLights;
}

/* Calculates the color associated with Phong shading for the given point on the surface. */
glm::vec3 Phong(const Ray& ray, const Light& light, GLfloat t, const Surface& surface) {

	glm::vec3 intersection = ray.origin + (ray.direction * t);
	glm::vec3 normal = SphereNormal(ray, surface, t);

	glm::vec3 lightRay = glm::normalize(light.position - intersection);
	glm::vec3 reflectedRay = glm::reflect(lightRay, normal);
	glm::vec3 viewRay = -ray.direction;

	glm::vec3 diffuse = surface.diff * glm::dot(lightRay, normal) * light.diff;
	glm::vec3 specular = surface.spec * pow(glm::dot(reflectedRay, viewRay), surface.shininess) * light.spec;

	glm::vec3 color = diffuse + specular;
	return color;
}

/* Determines whether to return the background color or the calculated Phong shading value per pixel. */
glm::vec4 TraceRay(const Ray& ray) {

	tuple<GLfloat, Surface> tuple = FindFirstIntersection(ray);
	GLfloat t = get<0>(tuple);
	Surface particle = get<1>(tuple);

	if ((t < 0.0f)) { // or quad
		return background;
	}

	vector<Light> contributedLights = ShadowRays(ray, t, particle);
	glm::vec3 color = glm::vec3(0.0f, 0.0f, 0.0f);

	for (Light& light : contributedLights) {
		color += Phong(ray, light, t, particle);
	}

	return glm::vec4(color, 1.0f);
}

/* Create a vector of colors used for generating the final scene. */
vector<glm::vec4> RayTraceOutput() {

	glm::vec4 color;

	vector<glm::vec4> colors;

	for (GLuint i = 0; i < screenWidth; i++)
	{
		for (GLuint j = 0; j < screenHeight; j++)
		{
			Ray ray = CalculateRay(i, j);
			glm::vec4 color = TraceRay(ray);
			color = glm::clamp(color, 0.0f, 1.0f);

			colors.push_back(color);
		}
	}

	return colors;
}

glm::vec4 mix(glm::vec4 originalColor, glm::vec4 fogColor, glm::vec4 fogFactor) {
	glm::vec4 mixed = originalColor * (glm::vec4(1.0, 1.0, 1.0, 1.0) - fogFactor) + fogColor * fogFactor;
	return mixed;
}

/* Draws the water particles over the city. */
void DrawWaterParticles() {

	if (moveWater == true) {
		colors.clear();
		colors = RayTraceOutput();
	}

	vector<glm::vec4> perlinTexture = generatePerlinTexture();

	// Interpolating between scene and perlin texture
	vector<glm::vec4> finalFog;

	/*
	float fogDensity_factor = 0.97;
	glm::vec4 fogColor = glm::vec4(0.7, 0.7, 0.7, 0.7);
	float fogFactor = 0.7;
	glm::vec4 fogFactorVec = glm::vec4(fogFactor, fogFactor, fogFactor, fogFactor);
	for (int i = 0; i < colors.size(); i++) {
		float fogDensity = perlinTexture[i].x * fogDensity_factor;

		glm::vec4 finalColor = mix(colors[i], fogColor, fogFactorVec);

		finalFog.push_back(finalColor);
	}
	*/

	for (int i = 0; i < colors.size(); i++) {
		if (colors[i].w <= 0) {
			colors[i].x = 0;
			colors[i].y = 0;
			colors[i].z = 0;
		}

		// std::cout << colors[i].x << "," << colors[i].y << "," << colors[i].z << "," << colors[i].w << std::endl;
		glm::vec4 finalColor = colors[i] + perlinTexture[i];

		if (finalColor.x > 1.0) {
			finalColor.x = 1.0;
		}
		else if (finalColor.x < 0.0) {
			finalColor.x = 0.0;
		}

		if (finalColor.y > 1.0) {
			finalColor.y = 1.0;
		}
		else if (finalColor.y < 0.0) {
			finalColor.y = 0.0;
		}

		if (finalColor.z > 1.0) {
			finalColor.z = 1.0;
		}
		else if (finalColor.z < 0.0) {
			finalColor.z = 0.0;
		}

		if (finalColor.a > 1.0) {
			finalColor.a = 1.0;
		}
		else if (finalColor.a < 0.0) {
			finalColor.a = 0.0;
		}

		finalFog.push_back(finalColor);

	}

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenWidth, screenHeight, 0, GL_RGBA, GL_FLOAT, finalFog.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glScalef(-1.0f, 1.0f, 1.0f);
	glTranslatef(0.0f, 0.0f, 0.0f);
	glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
	glScalef(1.333, 1.333, 1.333);

	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT0);

	/* Drawing the water particles. */
	glBegin(GL_QUADS);

	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(-1.0f, -1.0f);

	glTexCoord2f(1.0f, 0.0f);
	glVertex2f(1.0f, -1.0f);

	glTexCoord2f(1.0f, 1.0f);
	glVertex2f(1.0f, 1.0f);

	glTexCoord2f(0.0f, 1.0f);
	glVertex2f(-1.0f, 1.0f);

	glEnd();

	glPopMatrix();

	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

GLuint CaptureScreenToTexture() {

	GLuint textureID;

	// Generate a texture ID
	glGenTextures(1, &textureID);

	// Bind the texture
	glBindTexture(GL_TEXTURE_2D, textureID);

	// Allocate storage for the texture
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenWidth, screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	// Set texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Create framebuffer object (FBO)
	GLuint framebuffer;
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	// Attach the texture to the framebuffer
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);

	// Check framebuffer status
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "Framebuffer is not complete\n");
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &framebuffer);
		glDeleteTextures(1, &textureID);
		return 0;
	}

	// Set the viewport to match the texture size
	glViewport(0, 0, screenWidth, screenHeight);

	// Generate ground plane and draw the buildings
	GLfloat groundX = 15.0f;
	GLfloat groundZ = 15.0f;

	procedural(groundX, groundZ, dynamicBuildingNum);


	// Unbind the framebuffer and texture
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	return textureID;
}

/* Drawing the preexisting city. */
void DrawCityFromTexture() {
	if (cityTexture == -1) {
		cityTexture = CaptureScreenToTexture();
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Set viewport to match screen size
	glViewport(0, 0, screenWidth, screenHeight);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, cityTexture);

	// Use normalized device coordinates (NDC) for vertex positions
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(-1.0f, -1.0f);

	glTexCoord2f(1.0f, 0.0f);
	glVertex2f(1.0f, -1.0f);

	glTexCoord2f(1.0f, 1.0f);
	glVertex2f(1.0f, 1.0f);

	glTexCoord2f(0.0f, 1.0f);
	glVertex2f(-1.0f, 1.0f);
	glEnd();

	glDisable(GL_TEXTURE_2D);
}

/* Final display function for showing the city, water, and fog. */
void InitDisplay() {

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	if (buildings.empty() == true) {
		buildings = BuildingWalls();

		particles = CreateParticles(NUM_PARTICLES);

		surfaces = particles;
		surfaces.insert(surfaces.end(), buildings.begin(), buildings.end());

		lights = CreateLights();
	}

	DrawCityFromTexture();
	DrawWaterParticles();

	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
}

void displayNewWindow() {
	glClear(GL_COLOR_BUFFER_BIT);

	// Set up the orthographic projection matrix
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0, 800, 0, 800);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Display the texture on a quad
	if (currentFrameIndex < frames.size()) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0); // Assuming texture ID is 0
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 800, 800, 0, GL_RGBA, GL_UNSIGNED_BYTE, frames[currentFrameIndex]);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
		glTexCoord2f(1.0f, 0.0f); glVertex2f(screenWidth, 0.0f);
		glTexCoord2f(1.0f, 1.0f); glVertex2f(screenWidth, screenHeight);
		glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, screenHeight);
		glEnd();
		glDisable(GL_TEXTURE_2D);
	}

	glutSwapBuffers();
}

// Timer function for displaying frames sequentially
void displayNextFrame(int value) {
	// Increment the current frame index
	currentFrameIndex++;

	if (currentFrameIndex < frames.size()) {
		glutPostRedisplay();
		glutTimerFunc(500, displayNextFrame, 0);
	}
	else {
		currentFrameIndex = 0;
		glutPostRedisplay();
		glutTimerFunc(500, displayNextFrame, 0);
	}
}

/* What controls the rendering timer. */
void RenderingTimer(int value) {


	/* If the IMGUI checkbox to render has been selected... */
	if (timeToRender == true) {
		// Update particle positions
		for (Surface& p : particles) {
			p.position.x -= 5; // Move particles by -500 along the x-axis
		}
		srand(time(NULL));
		perlinSeed = rand();


		// Re-render the scene to reflect updated particle positions
		InitDisplay();

		unsigned char* textureBuffer = new unsigned char[screenWidth * screenHeight * 4]; // Assuming RGBA format
		glReadPixels(0, 0, screenWidth, screenHeight, GL_RGBA, GL_UNSIGNED_BYTE, textureBuffer);
		frames.push_back(textureBuffer);

		frameCount--;

		/* If change is detected, call timer. */
		glutTimerFunc(10000, RenderingTimer, 0);
	}

	if (frameCount <= 0) {
		timeToRender = false;
		cout << "Display time!" << endl;
		glutDisplayFunc(displayNewWindow);
		glutTimerFunc(500, displayNextFrame, 0);
	}
}

int main(int argc, char* argv[]) {
	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW." << std::endl;
		return -1;
	}

	GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "Procedural City", NULL, NULL);
	if (!window) {
		std::cerr << "Failed to create GLFW window." << std::endl;
		glfwTerminate();
		return -1;
	}

	double lastTime = glfwGetTime();
	glfwMakeContextCurrent(window);

	if (glewInit() != GLEW_OK) {
		std::cerr << "Failed to initialize GLEW." << std::endl;
		return -1;
	}

	// Setup ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark(); // Set style
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
	glfwSetKeyCallback(window, key_callback);

	SetupCamera();
	//SetupLighting();
	glClearColor(1, 1, 1, 1.0f);

	// Main loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);

		if (timeToRender == true) {
			InitDisplay();
			moveWater = true;
		}
		else {
			Load(buildingTexture, "textures/Building.bmp");

			// Generate ground plane and draw the buildings
			GLfloat groundX = 15.0f;
			GLfloat groundY = 1.0f;
			GLfloat groundZ = 15.0f;

			glPushMatrix();
			glTranslatef(0.0f, -0.5f * groundY, 0.0f);
			glScalef(groundX, groundY, groundZ);
			draw(false);
			glPopMatrix();

			// Toggle between random and grid positions
			if (useGridPos) {
				gridPos(dynamicBuildingNum, dynamicHeightNum);
			}
			else {
				randPos(dynamicBuildingNum, dynamicHeightNum);
			}

			randPos(dynamicBuildingNum, dynamicHeightNum);
			procedural(groundX, groundZ, dynamicBuildingNum);
		}

		ImGui::SetNextWindowSize(ImVec2(550, 150));
		ImGui::Begin("Scene Controls");
		ImGui::Text("Adjust city parameters.");
		ImGui::SliderInt("Number of Buildings", &dynamicBuildingNum, 1, 150);
		ImGui::SliderInt("Building Height", &dynamicHeightNum, 0, 8);
		ImGui::Checkbox("Use Grid Street", &useGridPos);

		ImGui::Checkbox("Place water?", &timeToRender);

		if (useGridPos) {
			ImGui::SliderFloat("Street Width", &dynamicStreetWidth, 0, 2.5);
		}
		ImGui::End();

		// Rendering
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}