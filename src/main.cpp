#include <SDL2/SDL.h>
#include <SDL_events.h>
#include <SDL_render.h>
#include <cstdlib>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/geometric.hpp>
#include <string>
#include <glm/glm.hpp>
#include <vector>
#include "./fps.h"
#include "color.h"
#include "intersect.h"
#include "object.h"
#include "cube.h"
#include "light.h"
#include "camera.h"
#include "skybox.h"
#include "globals.h"

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
std::vector<Object*> objects;
Light light = {glm::vec3(-10.0f, 10.0f, 20.0f), 1.0f, Color(255, 255, 255)};
Camera camera(glm::vec3(-3.0f, 2.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 10.0f);

bool init() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Error: No se puedo inicializar SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    window = SDL_CreateWindow("Proyecto 3 - Raytracing", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Error: No se pudo crear una ventana SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Error: No se pudo crear SDL_Renderer: " << SDL_GetError() << std::endl;
        return false;
    }

    return true;
}

void point(glm::vec2 position, Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawPoint(renderer, position.x, position.y);
}

float castShadow(const glm::vec3& shadowOrigin, const glm::vec3& lightDir, Object* hitObject) {
    for (auto& obj : objects) {
        if (obj != hitObject) {
            Intersect shadowIntersect = obj->rayIntersect(shadowOrigin, lightDir);
            if (shadowIntersect.isIntersecting && shadowIntersect.dist > 0) {
                float shadowRatio = shadowIntersect.dist / glm::length(light.position - shadowOrigin);
                shadowRatio = glm::min(1.0f, shadowRatio);
                return 1.0f - shadowRatio;
            }
        }
    }
    return 1.0f;
}

Color castRay(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const short recursion = 0, Object* currentObj = nullptr) {
    float zBuffer = 99999;
    Object* hitObject = nullptr;
    Intersect intersect;

    for (const auto& object : objects) {
        Intersect i = object->rayIntersect(rayOrigin, rayDirection);
        if (i.isIntersecting && i.dist < zBuffer && currentObj != object) {
            zBuffer = i.dist;
            hitObject = object;
            intersect = i;
        }
    }

    if (!intersect.isIntersecting || recursion == MAX_RECURSION) {
        return Skybox::getColor(rayOrigin, rayDirection);
    }


    glm::vec3 lightDir = glm::normalize(light.position - intersect.point);
    glm::vec3 viewDir = glm::normalize(rayOrigin - intersect.point);
    glm::vec3 reflectDir = glm::reflect(-glm::normalize(rayOrigin), intersect.normal);
    

    float shadowIntensity = castShadow(intersect.point, lightDir, hitObject);

    float diffuseLightIntensity = std::max(0.0f, glm::dot(intersect.normal, lightDir));
    float specReflection = glm::dot(viewDir, reflectDir);
    
    Material mat = hitObject->material;

    float specLightIntensity = std::pow(std::max(0.0f, glm::dot(viewDir, reflectDir)), mat.specularCoefficient);


    Color reflectedColor(0.0f, 0.0f, 0.0f);
    if (mat.reflectivity > 0) {
        glm::vec3 origin = intersect.point + intersect.normal * BIAS;
        reflectedColor = castRay(origin, reflectDir, recursion + 1, hitObject); 
    }

    Color refractedColor(0.0f, 0.0f, 0.0f);
    if (mat.transparency > 0) {
        glm::vec3 origin = intersect.point - intersect.normal * BIAS;
        glm::vec3 refractDir = glm::refract(rayDirection, intersect.normal, mat.refractionIndex);
        refractedColor = castRay(origin, refractDir, recursion + 1, hitObject); 
    }

    Color materialLight = intersect.hasColor ? intersect.color : mat.diffuse;

    Color diffuseLight = materialLight * light.intensity * diffuseLightIntensity * mat.albedo * shadowIntensity;
    Color specularLight = light.color * light.intensity * specLightIntensity * mat.specularAlbedo * shadowIntensity;
    Color color = (diffuseLight + specularLight) * (1.0f - mat.reflectivity - mat.transparency) + reflectedColor * mat.reflectivity + refractedColor * mat.transparency;
    return color;
}

class Diamond : public Cube
{
public:
    Diamond(const glm::vec3 &minBound, const glm::vec3 &maxBound, const Material &mat)
            : Cube(minBound, maxBound, mat) {}

    Intersect rayIntersect(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection) const override
    {
        Intersect intersect = Cube::rayIntersect(rayOrigin, rayDirection);

        const float epsilon = 0.0001;
        if (glm::abs(intersect.point.y - maxBound.y) < epsilon)
        {
            // Añadir textura arriba
            Color c = loadTexture(std::abs(intersect.point.x - minBound.x), std::abs(intersect.point.z - minBound.z), "diamond");

            intersect.color = c;
            intersect.hasColor = true;
        }
        else if (glm::abs(intersect.point.z - maxBound.z) < epsilon || glm::abs(intersect.point.z - minBound.z) < epsilon){
            // caras z
            Color c = loadTexture(std::abs(intersect.point.x - minBound.x), std::abs(intersect.point.y - minBound.y), "diamond");

            intersect.color = c;
            intersect.hasColor = true;
        }
        else if (glm::abs(intersect.point.x - minBound.x) < epsilon || glm::abs(intersect.point.x - maxBound.x) < epsilon){
            // caras x
            Color c = loadTexture(std::abs(intersect.point.z - minBound.z), std::abs(intersect.point.y - minBound.y), "diamond");

            intersect.color = c;
            intersect.hasColor = true;
        }

        return intersect;
    };
};

class Grass : public Cube
{
public:
    Grass(const glm::vec3 &minBound, const glm::vec3 &maxBound, const Material &mat)
            : Cube(minBound, maxBound, mat) {}

    Intersect rayIntersect(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection) const override
    {
        Intersect intersect = Cube::rayIntersect(rayOrigin, rayDirection);

        const float epsilon = 0.0001;
        if (glm::abs(intersect.point.y - maxBound.y) < epsilon)
        {
            Color c = loadTexture(std::abs(intersect.point.x - minBound.x), std::abs(intersect.point.z - minBound.z), "grass");

            intersect.color = c;
            intersect.hasColor = true;
        }
        else if (glm::abs(intersect.point.z - maxBound.z) < epsilon || glm::abs(intersect.point.z - minBound.z) < epsilon){
            // Tierra z
            Color c = loadTexture(std::abs(intersect.point.x - minBound.x), std::abs(intersect.point.y - minBound.y), "grass_side");

            intersect.color = c;
            intersect.hasColor = true;
        }
        else if (glm::abs(intersect.point.x - minBound.x) < epsilon || glm::abs(intersect.point.x - maxBound.x) < epsilon){
            // Tierra x
            Color c = loadTexture(std::abs(intersect.point.z - minBound.z), std::abs(intersect.point.y - minBound.y), "grass_side");

            intersect.color = c;
            intersect.hasColor = true;
        }

        return intersect;
    };
};

class Leaf : public Cube
{
public:
    Leaf(const glm::vec3 &minBound, const glm::vec3 &maxBound, const Material &mat)
            : Cube(minBound, maxBound, mat) {}

    Intersect rayIntersect(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection) const override
    {
        Intersect intersect = Cube::rayIntersect(rayOrigin, rayDirection);

        const float epsilon = 0.0001;
        if (glm::abs(intersect.point.y - maxBound.y) < epsilon)
        {
            // Añadir textura arriba
            Color c = loadTexture(std::abs(intersect.point.x - minBound.x), std::abs(intersect.point.z - minBound.z), "leaf");

            intersect.color = c;
            intersect.hasColor = true;
        }
        else if (glm::abs(intersect.point.z - maxBound.z) < epsilon || glm::abs(intersect.point.z - minBound.z) < epsilon){
            // caras z
            Color c = loadTexture(std::abs(intersect.point.x - minBound.x), std::abs(intersect.point.y - minBound.y), "leaf");

            intersect.color = c;
            intersect.hasColor = true;
        }
        else if (glm::abs(intersect.point.x - minBound.x) < epsilon || glm::abs(intersect.point.x - maxBound.x) < epsilon){
            // caras x
            Color c = loadTexture(std::abs(intersect.point.z - minBound.z), std::abs(intersect.point.y - minBound.y), "leaf");

            intersect.color = c;
            intersect.hasColor = true;
        }

        return intersect;
    };
};

class Oak : public Cube
{
public:
    Oak(const glm::vec3 &minBound, const glm::vec3 &maxBound, const Material &mat)
            : Cube(minBound, maxBound, mat) {}

    Intersect rayIntersect(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection) const override
    {
        Intersect intersect = Cube::rayIntersect(rayOrigin, rayDirection);

        const float epsilon = 0.0001;
        if (glm::abs(intersect.point.y - maxBound.y) < epsilon)
        {
            // Añadir textura arriba
            Color c = loadTexture(std::abs(intersect.point.x - minBound.x), std::abs(intersect.point.z - minBound.z), "oak_side");

            intersect.color = c;
            intersect.hasColor = true;
        }
        else if (glm::abs(intersect.point.z - maxBound.z) < epsilon || glm::abs(intersect.point.z - minBound.z) < epsilon){
            // caras z
            Color c = loadTexture(std::abs(intersect.point.x - minBound.x), std::abs(intersect.point.y - minBound.y), "oak_side");

            intersect.color = c;
            intersect.hasColor = true;
        }
        else if (glm::abs(intersect.point.x - minBound.x) < epsilon || glm::abs(intersect.point.x - maxBound.x) < epsilon){
            // caras x
            Color c = loadTexture(std::abs(intersect.point.z - minBound.z), std::abs(intersect.point.y - minBound.y), "oak_side");

            intersect.color = c;
            intersect.hasColor = true;
        }

        return intersect;
    };
};

class Plank : public Cube
{
public:
    Plank(const glm::vec3 &minBound, const glm::vec3 &maxBound, const Material &mat)
            : Cube(minBound, maxBound, mat) {}

    Intersect rayIntersect(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection) const override
    {
        Intersect intersect = Cube::rayIntersect(rayOrigin, rayDirection);

        const float epsilon = 0.0001;
        if (glm::abs(intersect.point.y - maxBound.y) < epsilon)
        {
            // Añadir textura arriba
            Color c = loadTexture(std::abs(intersect.point.x - minBound.x), std::abs(intersect.point.z - minBound.z), "plank");

            intersect.color = c;
            intersect.hasColor = true;
        }
        else if (glm::abs(intersect.point.z - maxBound.z) < epsilon || glm::abs(intersect.point.z - minBound.z) < epsilon){
            // caras z
            Color c = loadTexture(std::abs(intersect.point.x - minBound.x), std::abs(intersect.point.y - minBound.y), "plank");

            intersect.color = c;
            intersect.hasColor = true;
        }
        else if (glm::abs(intersect.point.x - minBound.x) < epsilon || glm::abs(intersect.point.x - maxBound.x) < epsilon){
            // caras x
            Color c = loadTexture(std::abs(intersect.point.z - minBound.z), std::abs(intersect.point.y - minBound.y), "plank");

            intersect.color = c;
            intersect.hasColor = true;
        }

        return intersect;
    };
};

void setUp() {
    // Define materials
    Material grass = {
            Color(0, 0, 0),
            0.85,
            0.0,
            0.50f,
            0.0f,
            0.0f
    };
    Material wood = {
            Color(0, 0, 0),
            0.85,
            0.0,
            0.50f,
            0.0f,
            0.0f
    };

    Material leaf = {
            Color(0, 0, 0),
            0.85,
            0.0,
            0.50f,
            0.0f,
            0.0f
    };

    Material diamond = {
            Color(0, 0, 0),
            0.85,
            0.4,
            2.50f,
            0.0f,
            0.0f
    };

    // Scene
    // Grass floor
    objects.push_back(new Grass(glm::vec3(-3.0f, -0.5f, -5.0f), glm::vec3(10.0f, 0.5f, 5.0f), grass));

    // Tree
    // Wood
    objects.push_back(new Oak(glm::vec3(-2.0f, 0.5f, -2.0f), glm::vec3(-1.0f, 3.5f, -1.0f), wood));
    // Leaves
    objects.push_back(new Leaf(glm::vec3(-3.0f, 3.5f, -3.0f), glm::vec3(0.0f, 4.5f, 0.0f), leaf));
    objects.push_back(new Leaf(glm::vec3(-2.0f, 4.5f, -2.0f), glm::vec3(-1.0f, 5.5f, -1.0f), leaf));

    // Diamond
    objects.push_back(new Diamond(glm::vec3(-2.0f, 0.5f, 2.0f), glm::vec3(1.0f, 1.5f, 1.0f), diamond));
    objects.push_back(new Diamond(glm::vec3(-1.0f, 1.5f, 2.0f), glm::vec3(0.0f, 2.5f, 1.0f), diamond));
    objects.push_back(new Diamond(glm::vec3(-1.0f, 0.5f, 2.0f), glm::vec3(0.0f, 1.5f, 3.0f), diamond));

    // House
    // Pared atras
    objects.push_back(new Plank(glm::vec3(3.0f, 0.5f, -4.0f), glm::vec3(7.0f, 4.5f, -3.0f), wood));
    // Techo 1
    objects.push_back(new Plank(glm::vec3(2.0f, 3.5f, -3.0f), glm::vec3(8.0f, 4.5f, 0.0f), wood));
    // Techo 2
    objects.push_back(new Plank(glm::vec3(3.0f, 3.5f, 0.0f), glm::vec3(7.0f, 4.5f, 1.0f), wood));
    // Pared lateral 1
    objects.push_back(new Plank(glm::vec3(2.0f, 0.5f, -3.0f), glm::vec3(3.0f, 3.5f, 0.0f), wood));
    // Pared lateral 2
    objects.push_back(new Plank(glm::vec3(7.0f, 0.5f, -3.0f), glm::vec3(8.0f, 3.5f, 0.0f), wood));
    // Columna 1
    objects.push_back(new Oak(glm::vec3(2.0f, 0.5f, 0.0f), glm::vec3(3.0f, 4.5f, 1.0f), wood));
    // Columna 2
    objects.push_back(new Oak(glm::vec3(7.0f, 0.5f, 0.0f), glm::vec3(8.0f, 4.5f, 1.0f), wood));
    // Columna 3
    objects.push_back(new Oak(glm::vec3(2.0f, 0.5f, -4.0f), glm::vec3(3.0f, 4.5f, -3.0f), wood));
    // Columna 4
    objects.push_back(new Oak(glm::vec3(7.0f, 0.5f, -4.0f), glm::vec3(8.0f, 4.5f, -3.0f), wood));
    // Pared frontal
    objects.push_back(new Plank(glm::vec3(3.0f, 0.5f, 0.0f), glm::vec3(5.0f, 3.5f, 1.0f), wood));
    // Puerta
    objects.push_back(new Plank(glm::vec3(6.0f, 0.5f, 0.0f), glm::vec3(7.0f, 3.5f, 1.0f), wood));

}


void render() {
    float fov = 3.1415/3;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            float screenX = (2.0f * (x + 0.5f)) / WIDTH - 1.0f;
            float screenY = -(2.0f * (y + 0.5f)) / HEIGHT + 1.0f;
            screenX *= RATIO;
            screenX *= tan(fov/2.0f);
            screenY *= tan(fov/2.0f);


            glm::vec3 cameraDir = glm::normalize(camera.target - camera.position);

            glm::vec3 cameraX = glm::normalize(glm::cross(cameraDir, camera.up));
            glm::vec3 cameraY = glm::normalize(glm::cross(cameraX, cameraDir));
            glm::vec3 rayDirection = glm::normalize(
                cameraDir + cameraX * screenX + cameraY * screenY
            );
           
            Color pixelColor = castRay(camera.position, rayDirection);

            point(glm::vec2(x, y), pixelColor);
        }
    }
}

int main(int argc, char* argv[]) {

     if (!init()) {
        return 1;
    }

    ImageLoader::loadImage("grass", "../assets/grass.png", 800.0f, 800.0f);
    ImageLoader::loadImage("grass_side", "../assets/grass_side.png", 800.0f, 800.0f);
    ImageLoader::loadImage("plank", "../assets/oak_plank.png", 358.0f, 358.0f);
    ImageLoader::loadImage("oak_side", "../assets/oak_side.png", 320.0f, 318.0f);
    ImageLoader::loadImage("leaf", "../assets/leaf.png", 500.0f, 500.0f);
    ImageLoader::loadImage("diamond", "../assets/diamond_ore.png", 256.0f, 256.0f);
    ImageLoader::loadImage("skybox1", "../assets/skybox_1.png", 793.0f, 877.0f);
    ImageLoader::loadImage("skybox2", "../assets/skybox_2.png", 795.0f, 877.0f);
    ImageLoader::loadImage("skybox3", "../assets/skybox_3.png", 792.0f, 877.0f);
    ImageLoader::loadImage("skybox4", "../assets/skybox_4.png", 792.0f, 877.0f);
    ImageLoader::loadImage("skybox_ground", "../assets/skyboxground.png", 322.0f, 282.0f);
    ImageLoader::loadImage("skybox_sky", "../assets/skybox_sky.png", 1080.0f, 1080.0f);

    bool running = true;
    SDL_Event event;

    setUp();

    while (running) {
        //startFPS();
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (event.type == SDL_KEYDOWN) {
                switch(event.key.keysym.sym) {
                    case SDLK_d:
                        camera.rotate(1.0f, 0.0f);
                        break;
                    case SDLK_a:
                        camera.rotate(-1.0f, 0.0f);
                        break;
                    case SDLK_w:
                        camera.zoom(1.0f);
                        break;
                    case SDLK_s:
                        camera.zoom(-1.0f);
                        break;
                    case SDLK_UP:
                        camera.moveY(1.0f);
                        break;
                    case SDLK_DOWN:
                        camera.moveY(-1.0f);
                        break;
                    case SDLK_RIGHT:
                        camera.moveX(1.0f);
                        break;
                    case SDLK_LEFT:
                        camera.moveX(-1.0f);
                        break;
                 }
            }


        }

        // Clear the screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        render();

        // Present the renderer
        SDL_RenderPresent(renderer);
        //endFPS(window);

    }
        // Cleanup
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();

    return 0;
}