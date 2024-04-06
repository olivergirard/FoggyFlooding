#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <random>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/freeglut.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp> 
#include <ProceduralCity.h>

unsigned int buildingTexture;
bool useGridPos = false;

std::vector<std::vector<double>> buildingPos;
std::vector<double> height;
std::vector<std::vector<double>> savedBuildingPos;
std::vector<double> savedHeights;
const GLfloat streetWidth = 1.0f;

std::vector<Surface> walls;

using namespace std;

class BitmapReader {
public:
	unsigned char* pixel;
	int w, h;

	BitmapReader(const char* path) {
		ifstream file(path, ios::binary);

		BITMAPFILEHEADER fileHeader;
		file.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));

		BITMAPINFOHEADER infoHeader;
		file.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));
		if (infoHeader.biSizeImage == 0)
			infoHeader.biSizeImage = infoHeader.biHeight * abs(infoHeader.biWidth) * 3;

		pixel = new unsigned char[infoHeader.biSizeImage];
		file.seekg(fileHeader.bfOffBits);
		file.read(reinterpret_cast<char*>(pixel), infoHeader.biSizeImage);

		for (int i = 0; i < infoHeader.biSizeImage; i += 3) {
			swap(pixel[i], pixel[i + 2]);
		}

		w = infoHeader.biWidth;
		h = infoHeader.biHeight;
	}

	~BitmapReader() {
		delete[] pixel;
	}
};

void SetupCamera() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(35.0, 1.0, 1.0, 1000.0); // Adjust this based on your city's size

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	GLfloat eyeX = 60.0f;
	GLfloat eyeY = 30.0f;
	GLfloat eyeZ = 30.0f;

	GLfloat centerX = 15.0f;
	GLfloat centerY = 10.0f;
	GLfloat centerZ = 8.0f;

	GLfloat upX = 0.0f;
	GLfloat upY = 1.0f;
	GLfloat upZ = 0.0f;

	gluLookAt(eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ);
}

void SetupLighting() {
	glEnable(GL_LIGHTING);

	// Primary Light (GL_LIGHT0)
	glEnable(GL_LIGHT0);
	GLfloat light0_ambient[] = { 0.25f, 0.25f, 0.25f, 1.0f };
	GLfloat light0_diffuse[] = { 0.5f, 0.5f, 0.5f, 1.0f };
	GLfloat light0_position[] = { 1.0f, 1.0f, 1.0f, 0.0f };
	glLightfv(GL_LIGHT0, GL_AMBIENT, light0_ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, light0_position);

	// Secondary Light (GL_LIGHT1)
	glEnable(GL_LIGHT1);
	GLfloat light1_ambient[] = { 0.1f, 0.1f, 0.1f, 1.0f };
	GLfloat light1_diffuse[] = { 0.4f, 0.4f, 0.4f, 1.0f };
	GLfloat light1_position[] = { -1.0f, 1.0f, -1.0f, 0.0f };
	glLightfv(GL_LIGHT1, GL_AMBIENT, light1_ambient);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
	glLightfv(GL_LIGHT1, GL_POSITION, light1_position);

	// Tertiary Light (GL_LIGHT2) - Overhead Light
	glEnable(GL_LIGHT2);
	GLfloat light2_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
	GLfloat light2_diffuse[] = { 0.6f, 0.6f, 0.6f, 1.0f };
	GLfloat light2_position[] = { 0.0f, 2.0f, 0.0f, 1.0f };
	glLightfv(GL_LIGHT2, GL_AMBIENT, light2_ambient);
	glLightfv(GL_LIGHT2, GL_DIFFUSE, light2_diffuse);
	glLightfv(GL_LIGHT2, GL_POSITION, light2_position);
}

void properties() {
	GLfloat sp[] = { .8f, .8f, .8f, .8f };
	GLfloat sh = 570.0f;

	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, sp);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, sh);
}

GLfloat vBuilding[8][3] = {
	{-1.0, 0.0, -1.0},
	{-1.0, 0.0, 1.0},
	{1.0, 0.0, 1.0},
	{1.0, 0.0, -1.0},
	{-1.0, 1.0, -1.0},
	{-1.0, 1.0, 1.0},
	{1.0, 1.0, 1.0},
	{1.0, 1.0, -1.0}
};

GLubyte quad[6][4] = {
	{0, 1, 5, 4},
	{3, 2, 6, 7},
	{0, 3, 7, 4},
	{1, 2, 6, 5},
	{4, 5, 6, 7},
	{0, 1, 2, 3}
};

std::array<std::array<double, 7>, 9> heightSets = { {
	{0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8},
	{0.3, 0.5, 0.7, 0.9, 1.1, 1.3, 1.5},
	{0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6},
	{0.5, 0.7, 0.9, 1.1, 1.3, 1.5, 1.7},
	{0.5, 0.7, 1.0, 1.3, 2.0, 2.2, 3.5},
	{0.7, 0.9, 1.2, 1.5, 2.2, 2.5, 3.8},
	{0.9, 1.1, 1.4, 1.7, 2.4, 2.7, 4.0},
	{1.0, 1.3, 1.6, 1.9, 2.6, 2.9, 4.2},
	{1.2, 1.5, 1.8, 2.1, 2.8, 3.1, 4.4}
} };


glm::vec3 calculateNormal(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3) {
	glm::vec3 edge1 = v2 - v1;
	glm::vec3 edge2 = v3 - v1;
	glm::vec3 normal = glm::cross(edge1, edge2);
	normal = glm::normalize(normal);
	return normal;
}

void draw(bool t = false) {
	properties();
	int repeat = 2;

	if (t) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, buildingTexture);
	}
	else {
		glDisable(GL_TEXTURE_2D);
	}

	glBegin(GL_QUADS);
	for (GLint i = 0; i < 6; ++i) {
		glm::vec3 normal = calculateNormal(
			glm::vec3(vBuilding[quad[i][0]][0], vBuilding[quad[i][0]][1], vBuilding[quad[i][0]][2]),
			glm::vec3(vBuilding[quad[i][1]][0], vBuilding[quad[i][1]][1], vBuilding[quad[i][1]][2]),
			glm::vec3(vBuilding[quad[i][2]][0], vBuilding[quad[i][2]][1], vBuilding[quad[i][2]][2])
		);

		glNormal3fv(glm::value_ptr(normal));

		for (GLint j = 0; j < 4; ++j) { 
			float texCoords[2];
			switch (j) {
			case 0: texCoords[0] = 0.0f; texCoords[1] = 0.0f; break;
			case 1: texCoords[0] = 1.0f; texCoords[1] = 0.0f; break;
			case 2: texCoords[0] = 1.0f; texCoords[1] = repeat; break;
			case 3: texCoords[0] = 0.0f; texCoords[1] = repeat; break;
			}

			glTexCoord2fv(texCoords);
			glVertex3fv(vBuilding[quad[i][j]]);
		}
	}

	glEnd();

	if (t) {
		glDisable(GL_TEXTURE_2D);
	}
}

class Random {
public:
	static int random(int min, int max) {
		static std::mt19937 generator(std::random_device{}());
		std::uniform_int_distribution<> dis(min, max);
		return dis(generator);
	}
};

void randPos(int dNum, int heightSetIndex) {
	if (heightSetIndex < 0 || heightSetIndex >= heightSets.size()) {
		std::cerr << "Invalid height set index." << std::endl;
		return;
	}

	if (!savedBuildingPos.empty() && savedBuildingPos.size() == dNum && savedHeights.size() == dNum) {
		buildingPos = savedBuildingPos;
		height = savedHeights;
		return;
	}

	buildingPos.clear();
	height.clear();
	savedBuildingPos.clear();
	savedHeights.clear();

	auto& selectedHeightSet = heightSets[heightSetIndex];

	for (int i = 0; i < dNum; i++) {
		double posX = Random::random(0, 19);
		double posZ = Random::random(0, 19);
		buildingPos.push_back({ posX, posZ });

		int heightIndex = Random::random(0, selectedHeightSet.size() - 1);
		height.push_back(selectedHeightSet[heightIndex]);
	}

	savedBuildingPos = buildingPos;
	savedHeights = height;
}

void gridPos(int dNum, int heightSetIndex) {
	if (heightSetIndex < 0 || heightSetIndex >= heightSets.size()) {
		std::cerr << "Invalid height set index." << std::endl;
		return;
	}

	if (!savedBuildingPos.empty() && savedBuildingPos.size() == dNum && savedHeights.size() == dNum) {
		buildingPos = savedBuildingPos;
		height = savedHeights;
		return;
	}

	buildingPos.clear();
	height.clear();
	savedBuildingPos.clear();
	savedHeights.clear();

	auto& selectedHeightSet = heightSets[heightSetIndex];

	for (int i = 0; i < dNum; i++) {
		double posX = Random::random(0, dynamicGridSize - 1) * (1.0 + dynamicStreetWidth);
		double posZ = Random::random(0, dynamicGridSize - 1) * (1.0 + dynamicStreetWidth);
		buildingPos.push_back({ posX, posZ });
		int heightIndex = Random::random(0, selectedHeightSet.size() - 1);
		height.push_back(selectedHeightSet[heightIndex]);
	}

	savedBuildingPos = buildingPos;
	savedHeights = height;
}

void procedural(GLfloat groundX, GLfloat groundZ, int dNum) {

	GLfloat offsetX = groundX / 2.0f;
	GLfloat offsetZ = groundZ / 2.0f;

	glBegin(GL_QUADS);

	glColor3f(1.0f, 0.929f, 0.714f);

	glVertex3f(40.0f, -0.1f, 30.0f);
	glVertex3f(40.0f, -0.1f, -40.0f);
	glVertex3f(-20.0f, -0.1f, -50.0f);
	glVertex3f(-20.0f, -0.1f, 30.0f);

	glColor3f(1.0f, 1.0f, 1.0f);

	glVertex3f(15.0f, 0.0f, 15.0f);
	glVertex3f(15.0f, 0.0f, -15.0f);
	glVertex3f(-15.0f, 0.0f, -15.0f);
	glVertex3f(-15.0f, 0.0f, 15.0f);

	glEnd();

	walls.clear();

	for (int i = 0; i < dNum; i++) {
		glPushMatrix();

		// Adjust building positions to be centered relative to the ground plane
		GLfloat adjustedX = buildingPos[i][0] - offsetX;
		GLfloat adjustedZ = buildingPos[i][1] - offsetZ;

		glTranslatef(adjustedX, 0, adjustedZ);
		glScalef(0.5, height[i], 0.5);

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, buildingTexture);

		// Draw a building
		glPushMatrix();
		glScalef(1.0, 5.0, 1.0);
		draw(true);
		glPopMatrix();

		glDisable(GL_TEXTURE_2D);
		glPopMatrix();

		// Store the final positions and heights in a Surface struct
		for (int j = 0; j < 6; ++j) {
			glm::vec3 finalVertices[4];
			for (int k = 0; k < 4; ++k) {
				glm::vec3 originalVertex(vBuilding[quad[j][k]][0], vBuilding[quad[j][k]][1], vBuilding[quad[j][k]][2]);
				glm::vec3 transformedVertex = glm::vec3(
					originalVertex.x + adjustedX * 1.65,
					(originalVertex.y * height[i] * 9) - 2.75,
					originalVertex.z + adjustedZ * 1.65
				);
				finalVertices[k] = transformedVertex;
			}

			Surface wall;
			wall.type = QUAD;
			wall.pos1 = finalVertices[0];
			wall.pos2 = finalVertices[1];
			wall.pos3 = finalVertices[2];
			wall.pos4 = finalVertices[3];
			walls.push_back(wall);
		}
	}
}

void Load(unsigned int& textureID, const char* filename) {
	BitmapReader bmpReader(filename);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bmpReader.w, bmpReader.h, 0, GL_BGR, GL_UNSIGNED_BYTE, bmpReader.pixel);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_R) {
			useGridPos = !useGridPos;
		}
	}
}

std::vector<Surface> BuildingWalls() {
	return walls;
}