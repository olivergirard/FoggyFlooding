#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp> 
#include <glm/gtc/matrix_transform.hpp>

using namespace std;

std::vector<std::vector<double>> buildingPos;
std::vector<double> heights;
const int buildingNum = 20;
unsigned int buildingTexture;

typedef enum { QUAD, SPHERE } typeOfSurface;

struct Surface {

	/* Particle attributes. */
	glm::vec3 position;
	GLfloat radius = 50.0f;

	/* Quad attributes. */
	glm::vec3 pos1;
	glm::vec3 pos2;
	glm::vec3 pos3;
	glm::vec3 pos4;

	/* General attributes. */
	glm::vec3 diff;
	glm::vec3 spec = glm::vec3(1.0f, 1.0f, 01.0f);
	GLfloat shininess = 10.0f;
	typeOfSurface type;
};

std::vector<Surface> walls;

class Random {
public:
	static int random(int min, int max) {
		static std::mt19937 generator(std::random_device{}());
		std::uniform_int_distribution<> dis(min, max);
		return dis(generator);
	}
};

void applyProperties() {
	GLfloat sp[] = { .8f, .8f, .8f, .8f };
	GLfloat sh = 570.0f;

	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, sp);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, sh);
}

void SetupCamera() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0, 1.0, 1.0, 1000.0); // Adjust this based on your city's size

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	GLfloat eyeX = 40.0f;
	GLfloat eyeY = 25.0f;
	GLfloat eyeZ = 25.0f;

	GLfloat centerX = 0.0f;
	GLfloat centerY = 0.0f;
	GLfloat centerZ = 0.0f;

	GLfloat upX = 0.0f;
	GLfloat upY = 1.0f;
	GLfloat upZ = 0.0f;

	gluLookAt(eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ);
}

void SetupLighting() {
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	GLfloat light_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
	GLfloat light_diffuse[] = { 15.0f, 15.0f, 15.0f, 1.0f };
	GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 0.0f };

	glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
}

GLfloat vBuilding[8][3] = {
	{0.0, 0.0, 0.0},
	{0.0, 0.0, 1.0},
	{1.0, 0.0, 1.0},
	{1.0, 0.0, 0.0},
	{0.0, 1.0, 0.0},
	{0.0, 1.0, 1.0},
	{1.0, 1.0, 1.0},
	{1.0, 1.0, 0.0}
};

GLubyte quad[6][4] = {
	{0, 1, 2, 3},
	{4, 5, 6, 7},
	{5, 1, 2, 6},
	{0, 4, 7, 3},
	{2, 3, 7, 6},
	{1, 5, 4, 0}
};

glm::vec3 calculateNormal(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3) {
	glm::vec3 edge1 = v2 - v1;
	glm::vec3 edge2 = v3 - v1;
	glm::vec3 normal = glm::cross(edge1, edge2);
	normal = glm::normalize(normal);
	return normal;
}

void draw(const std::vector<std::vector<double>>& buildingPositions, const std::vector<double>& buildingHeights, bool t = false) {
	applyProperties();
	int repeat = 2;

	if (t) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, buildingTexture);
	}
	else {
		glDisable(GL_TEXTURE_2D);
	}

	/* Building the sandy floor. */
	glBegin(GL_QUADS);
	glColor3f(0.962f, 0.918f, 0.861f);
	glVertex3f(-10.0f, 10.0f, -10.0f);
	glVertex3f(40.0f, 10.0f, -10.0f);
	glVertex3f(40.0f, 10.0f, 30.0f);
	glVertex3f(-10.0f, 10.0f, 30.0f);
	glEnd();

	glBegin(GL_QUADS);
	for (size_t i = 0; i < buildingPositions.size(); ++i) {
		double posX = buildingPositions[i][0];
		double posY = 10.0;
		double posZ = buildingPositions[i][1];
		double height = buildingHeights[i];

		for (GLint j = 0; j < 6; ++j) {
			glm::vec3 normal = calculateNormal(
				glm::vec3(vBuilding[quad[j][0]][0] + posX, vBuilding[quad[j][0]][1] * height + posY, vBuilding[quad[j][0]][2] + posZ),
				glm::vec3(vBuilding[quad[j][1]][0] + posX, vBuilding[quad[j][1]][1] * height + posY, vBuilding[quad[j][1]][2] + posZ),
				glm::vec3(vBuilding[quad[j][2]][0] + posX, vBuilding[quad[j][2]][1] * height + posY, vBuilding[quad[j][2]][2] + posZ)
			);

			glNormal3fv(glm::value_ptr(normal));

			Surface su;

			for (GLint k = 0; k < 4; ++k) {
				float texCoords[2];
				switch (k) {
					case 0: 
						texCoords[0] = 0.0f; 
						texCoords[1] = 0.0f; 
						su.pos1 = glm::vec3(vBuilding[quad[j][k]][0] + posX, vBuilding[quad[j][k]][1] * height + posY, vBuilding[quad[j][k]][2] + posZ); 
						break;
					case 1: 
						texCoords[0] = repeat; 
						texCoords[1] = 0.0f;  
						su.pos2 = glm::vec3(vBuilding[quad[j][k]][0] + posX, vBuilding[quad[j][k]][1] * height + posY, vBuilding[quad[j][k]][2] + posZ); 
						break;
					case 2:
						texCoords[0] = repeat; 
						texCoords[1] = repeat;
						su.pos3 = glm::vec3(vBuilding[quad[j][k]][0] + posX, vBuilding[quad[j][k]][1] * height + posY, vBuilding[quad[j][k]][2] + posZ); 
						break;
					case 3:
						texCoords[0] = 0.0f; 
						texCoords[1] = repeat; 
						su.pos4 = glm::vec3(vBuilding[quad[j][k]][0] + posX, vBuilding[quad[j][k]][1] * height + posY, vBuilding[quad[j][k]][2] + posZ); 
						break;
				}
				
				glTexCoord2fv(texCoords);
				glVertex3f(vBuilding[quad[j][k]][0] + posX, vBuilding[quad[j][k]][1] * height + posY, vBuilding[quad[j][k]][2] + posZ);
				glColor3f(1.0f, 1.0f, 1.0f);

			}

			su.type = QUAD;
			walls.push_back(su);
		}
	}
	glEnd();

	if (t) {
		glDisable(GL_TEXTURE_2D);
	}
}

void positions() {
	buildingPos.clear();
	heights.clear();

	double customHeights[] = { 2, 4, 6, 8, 10 };

	for (int i = 0; i < buildingNum; i++) {
		double posX = Random::random(0, 19);
		double posZ = Random::random(0, 19);
		buildingPos.push_back({ posX, posZ });

		int heightIndex = Random::random(0, sizeof(customHeights) / sizeof(customHeights[0]) - 1);
		heights.push_back(customHeights[heightIndex]);
	}
}

void procedural() {

	glPushMatrix();
	glTranslatef(0.0f, 0.0f, 0.0f);
	glScalef(1.0f, 1.0f, 1.0f);
	draw(buildingPos, heights, true);
	glPopMatrix();
}

class BitmapReader {
public:
	unsigned char* pixel;
	int width, height;

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

		width = infoHeader.biWidth;
		height = infoHeader.biHeight;
	}

	~BitmapReader() {
		delete[] pixel;
	}
};

void Load(unsigned int& textureID, const char* filename) {
	BitmapReader bmpReader(filename);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bmpReader.width, bmpReader.height, 0, GL_BGR, GL_UNSIGNED_BYTE, bmpReader.pixel);
}

void show() {

	SetupCamera();
	SetupLighting();

	glClearColor(0.537, 0.835, 0.922, 1.0f);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	Load(buildingTexture, "textures/Building.bmp");

	draw(buildingPos, heights, false);

	glPopMatrix();

	positions();
	procedural();

	glDisable(GL_DEPTH_TEST);

	glutSwapBuffers();
}

std::vector<Surface> BuildingWalls() {

	return walls;
}