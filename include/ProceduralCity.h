#include <GLFW\glfw3.h>

using namespace std;

extern unsigned int buildingTexture;
static int dynamicBuildingNum = 10;
static int dynamicHeightNum = 5;
static float dynamicStreetWidth = 2;
static int dynamicGridSize = 7;

extern bool useGridPos;

void draw(bool);

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

vector<Surface> BuildingWalls();

void SetupCamera();
void SetupLighting();

void key_callback(GLFWwindow*, int, int, int, int);

void Load(unsigned int&, const char*);
void gridPos(int, int);
void procedural(GLfloat, GLfloat, int);
void randPos(int, int);