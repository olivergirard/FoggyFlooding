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
#include "ImGui/imgui_impl_glut.h"
#include "ImGui/imgui_impl_opengl2.h"

#include "ProceduralCity.h"

// Noise library
#include "FastNoise.h"

using namespace std;

/* Reduce this number to speed up compilation time. */
const int NUM_PARTICLES = 10;

const int screenWidth = 800;
const int screenHeight = 800;
glm::vec4 background = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

/* Do not modify these as they are essential for ray tracing. */
glm::vec3 eye = glm::vec3(40.0f, 25.0f, 25.0f);
glm::vec3 lookAt = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

const GLfloat EPSILON = 1e-2;

bool timeToRender = false;
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

GLuint cityTexture;

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

	if ((t < 0.0f) || (particle.type == QUAD)) { // or quad
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

	colors = RayTraceOutput();

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

	colors.clear();

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

/* Draws the city behind the water particles. */
void DrawCity() {

	GLuint framebuffer;
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	glGenTextures(1, &cityTexture);
	glBindTexture(GL_TEXTURE_2D, cityTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenWidth, screenHeight, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cityTexture, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	glViewport(0, 0, screenWidth, screenHeight);
	show();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, screenWidth, screenHeight);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0f, screenWidth, 0.0f, screenHeight, -1.0f, 1.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glBindTexture(GL_TEXTURE_2D, cityTexture);
	glEnable(GL_TEXTURE_2D);

	/* Drawing the city. */
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(0.0f, 0.0f);

	glTexCoord2f(1.0f, 0.0f);
	glVertex2f(screenWidth, 0.0f);

	glTexCoord2f(1.0f, 1.0f);
	glVertex2f(screenWidth, screenHeight);

	glTexCoord2f(0.0f, 1.0f);
	glVertex2f(0.0f, screenHeight);
	glEnd();

	glDisable(GL_TEXTURE_2D);
}

/* Drawing the preexisting city. */
void DrawCityFromTexture() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0f, screenWidth, 0.0f, screenHeight, -1.0f, 1.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, cityTexture);

	/* Drawing the city using the cityTexture. */
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(0.0f, 0.0f);

	glTexCoord2f(1.0f, 0.0f);
	glVertex2f(screenWidth, 0.0f);

	glTexCoord2f(1.0f, 1.0f);
	glVertex2f(screenWidth, screenHeight);

	glTexCoord2f(0.0f, 1.0f);
	glVertex2f(0.0f, screenHeight);
	glEnd();

	glDisable(GL_TEXTURE_2D);
}

/* Final display function for showing the city, water, and fog. */
void InitDisplay() {

	glViewport(0, 0, screenWidth, screenHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	if (timeToRender == false) {
		DrawCity();
		buildings = BuildingWalls();

		particles = CreateParticles(NUM_PARTICLES);

		surfaces = particles;
		surfaces.insert(surfaces.end(), buildings.begin(), buildings.end());

		lights = CreateLights();
	}
	else {
		DrawCityFromTexture();
		surfaces.clear();
		surfaces = particles;
		surfaces.insert(surfaces.end(), buildings.begin(), buildings.end());
	}

	DrawWaterParticles();

	timeToRender = true;

	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	glutSwapBuffers();
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

/* What controls ImGui. */
void StartGUI()
{
	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplGLUT_NewFrame();
	ImGui::NewFrame();
	ImGuiIO& io = ImGui::GetIO();

	ImGui::Begin("CS434 Final Project");
	ImGui::Text("Rendering...");
	ImGui::End();

	ImGui::Render();

	glViewport(0, 0, screenWidth, screenHeight);

	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

	glutSwapBuffers();
	glutPostRedisplay();
}

int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitWindowSize(screenWidth, screenHeight);
	glutCreateWindow("CS434 Final Project");

	if (glewInit() != GLEW_OK) {
		cerr << "Failed to initialize GLEW." << endl;
		exit(-1);
	}

	InitDisplay();

	glutDisplayFunc(StartGUI);
	glutTimerFunc(1000, RenderingTimer, 0);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	ImGui_ImplGLUT_Init();
	ImGui_ImplOpenGL2_Init();
	ImGui_ImplGLUT_InstallFuncs();

	glutMainLoop();

	ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplGLUT_Shutdown();
	ImGui::DestroyContext();

	return 0;
}