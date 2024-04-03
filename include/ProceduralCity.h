#include <GLFW\glfw3.h>

using namespace std;

void show();

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