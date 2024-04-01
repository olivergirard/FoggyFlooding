#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp> 

std::vector<std::vector<double>> buildingPos;
std::vector<double> height;
const int buildingNum = 220; // Define the number of buildings
unsigned int buildingTexture;
using namespace std;

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

    GLfloat centerX = 10.0f;
    GLfloat centerY = 10.0f;
    GLfloat centerZ = 10.0f;

    GLfloat upX = 0.0f;
    GLfloat upY = 1.0f;
    GLfloat upZ = 0.0f;

    gluLookAt(eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ);
}


void SetupLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat light_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat light_diffuse[] = { 5.0f, 5.0f, 5.0f, 1.0f };
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

void draw(
    bool t = false
) {
    applyProperties();
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
            case 1: texCoords[0] = repeat; texCoords[1] = 0.0f; break;
            case 2: texCoords[0] = repeat; texCoords[1] = repeat; break;
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

void positions() {
    buildingPos.clear();
    height.clear();

    double customHeights[] = { 0.3, 0.2, 0.5, 1.0, 2.0 };

    for (int i = 0; i < buildingNum; i++) {
        double posX = Random::random(0, 19);
        double posZ = Random::random(0, 19);
        buildingPos.push_back({ posX, posZ });

        int heightIndex = Random::random(0, sizeof(customHeights) / sizeof(customHeights[0]) - 1);
        height.push_back(customHeights[heightIndex]);
    }
}

void procedural() {
    for (int i = 0; i < buildingNum; i++) {
        glPushMatrix();
        glTranslatef(buildingPos[i][0], 0, buildingPos[i][1]);
        glScalef(0.5, height[i], 0.5);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, buildingTexture);
        glPushMatrix();
        glScalef(1.0, 5.0, 1.0); // Scale to create a tall building shape
        draw(true);
        glPopMatrix();
        glDisable(GL_TEXTURE_2D);
        glPopMatrix();
    }
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
    glClearColor(0, 1, 1, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    Load(buildingTexture, "textures/Building.bmp");

    // Generate ground plane and draw the buildings
    GLfloat groundX = 25.0f;
    GLfloat groundY = 1.0f;
    GLfloat groundZ = 25.0f;

    glPushMatrix();

    glScalef(groundX, groundY, groundZ);
    glTranslatef(0.0f, -0.5f / groundY, 0.0f);

    draw(false);

    glPopMatrix();

    positions();
    procedural();

    glutSwapBuffers();
}
