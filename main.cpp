#include <iostream>
#include <cmath>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_image/SDL_image.h>
#include <list>
#include <unordered_map>

float playerX=0;
float playerY=0;
float playerZ=3;

float yaw=0;
float pitch=0;

using namespace std;

list<tuple<string, string, string>> TextureAtlas = {
    tuple<string, string, string>("grass_top.png", "grass.png", "dirt.png"),
    tuple<string, string, string>("dirt.png","dirt.png","dirt.png"),
    tuple<string, string, string>("stone.png","stone.png","stone.png"),
    tuple<string, string, string>("error.png","error.png","error.png")
};
unordered_map<std::string, int> KeyMapper = {
    {"error", 0},
    {"grass" , 1},
    {"dirt", 2},
    {"stone", 3}
};
unordered_map<string, GLuint> Textures = {};

std::unordered_map<std::string, std::string> worldBlocks;

// Helper to make key
std::string posKey(int x, int y, int z) {
    return std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z);
}

bool isSolid(int x, int y, int z) {
    std::string key = posKey(x, y, z);
    auto it = worldBlocks.find(key);
    if (it == worldBlocks.end()) return false;           // air = not solid
    std::string type = it->second;
    // Add "air" or transparent types later; for now assume everything is solid
    return type != "air";  // or check against a set of transparent types
}

GLuint LoadTexture(const char* file)
{
    SDL_Surface* raw = IMG_Load(file);
    if (!raw) {
        std::cout << "Failed to load " << file << ": " << SDL_GetError() << std::endl;
        return 0;
    }

    // Convert to RGBA32 → OpenGL expects this byte order reliably
    SDL_Surface* surface = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(raw);
    if (!surface) {
        std::cout << "Conversion failed for " << file << ": " << SDL_GetError() << std::endl;
        return 0;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,                // internal format
        surface->w,
        surface->h,
        0,
        GL_RGBA,                // data format — matches RGBA32 bytes
        GL_UNSIGNED_BYTE,
        surface->pixels
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    SDL_DestroySurface(surface);

    return texture;
}

void InitOpenGL(int width, int height)
{
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float aspect = (float)width / (float)height;

    float fov_y_tan_half = tan(10.0f * M_PI / 360.0f);
    float top    = fov_y_tan_half;
    float bottom = -fov_y_tan_half;
    float right  = top * aspect;
    float left   = -right;

    glFrustum(left, right, bottom, top, 0.1f, 200.0f);

    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
}

void DrawCube(GLuint topTex, GLuint sideTex, GLuint bottomTex,
              int worldX, int worldY, int worldZ)
{
    // Sides
    glBindTexture(GL_TEXTURE_2D, sideTex);
    glBegin(GL_QUADS);

    // Front (+Z face)
    if (!isSolid(worldX, worldY, worldZ + 1)) {
        glTexCoord2f(0,1); glVertex3f(-0.5f,-0.5f, 0.5f);
        glTexCoord2f(1,1); glVertex3f( 0.5f,-0.5f, 0.5f);
        glTexCoord2f(1,0); glVertex3f( 0.5f, 0.5f, 0.5f);
        glTexCoord2f(0,0); glVertex3f(-0.5f, 0.5f, 0.5f);
    }

    // Back (-Z face)
    if (!isSolid(worldX, worldY, worldZ - 1)) {
        glTexCoord2f(0,1); glVertex3f( 0.5f,-0.5f,-0.5f);
        glTexCoord2f(1,1); glVertex3f(-0.5f,-0.5f,-0.5f);
        glTexCoord2f(1,0); glVertex3f(-0.5f, 0.5f,-0.5f);
        glTexCoord2f(0,0); glVertex3f( 0.5f, 0.5f,-0.5f);
    }

    // Left (-X)
    if (!isSolid(worldX - 1, worldY, worldZ)) {
        glTexCoord2f(0,1); glVertex3f(-0.5f,-0.5f,-0.5f);
        glTexCoord2f(1,1); glVertex3f(-0.5f,-0.5f, 0.5f);
        glTexCoord2f(1,0); glVertex3f(-0.5f, 0.5f, 0.5f);
        glTexCoord2f(0,0); glVertex3f(-0.5f, 0.5f,-0.5f);
    }

    // Right (+X)
    if (!isSolid(worldX + 1, worldY, worldZ)) {
        glTexCoord2f(0,1); glVertex3f(0.5f,-0.5f, 0.5f);
        glTexCoord2f(1,1); glVertex3f(0.5f,-0.5f,-0.5f);
        glTexCoord2f(1,0); glVertex3f(0.5f, 0.5f,-0.5f);
        glTexCoord2f(0,0); glVertex3f(0.5f, 0.5f, 0.5f);
    }

    glEnd();

    // Top (+Y)
    glBindTexture(GL_TEXTURE_2D, topTex);
    glBegin(GL_QUADS);
    if (!isSolid(worldX, worldY + 1, worldZ)) {
        glTexCoord2f(0,1); glVertex3f(-0.5f,0.5f, 0.5f);
        glTexCoord2f(1,1); glVertex3f( 0.5f,0.5f, 0.5f);
        glTexCoord2f(1,0); glVertex3f( 0.5f,0.5f,-0.5f);
        glTexCoord2f(0,0); glVertex3f(-0.5f,0.5f,-0.5f);
    }
    glEnd();

    // Bottom (-Y)
    glBindTexture(GL_TEXTURE_2D, bottomTex);
    glBegin(GL_QUADS);
    if (!isSolid(worldX, worldY - 1, worldZ)) {
        glTexCoord2f(0,1); glVertex3f(-0.5f,-0.5f,-0.5f);
        glTexCoord2f(1,1); glVertex3f( 0.5f,-0.5f,-0.5f);
        glTexCoord2f(1,0); glVertex3f( 0.5f,-0.5f, 0.5f);
        glTexCoord2f(0,0); glVertex3f(-0.5f,-0.5f, 0.5f);
    }
    glEnd();

    glPopMatrix();  // assuming push was done before
}

std::tuple<string, string, string> Find_tuple(string Index_name) {
    auto it = KeyMapper.find(Index_name);
    if (it == KeyMapper.end()) {
        std::cout << "Unknown block type: " << Index_name << std::endl;
        return {"error.png", "error.png", "error.png"};  // fallback
    }
    int target = it->second;

    int idx = 1;
    for (const auto& tup : TextureAtlas) {
        if (idx == target) {
            return tup;
        }
        idx++;
    }

    std::cout << "Index not found for " << Index_name << " (value=" << target << ")" << std::endl;
    return {"error.png", "error.png", "error.png"};  // safe fallback
}

void RenderCube(float wx, float wy, float wz, std::string type) {
    auto tup = Find_tuple(type);
    string top    = std::get<0>(tup);
    string side   = std::get<1>(tup);
    string bottom = std::get<2>(tup);

    GLuint t = Textures[top];
    GLuint s = Textures[side];
    GLuint b = Textures[bottom];

    glPushMatrix();
    glTranslatef(static_cast<float>(wx), static_cast<float>(wy), static_cast<float>(wz));
    DrawCube(t, s, b, wx, wy, wz);   // now passes world pos
    glPopMatrix();
}

int main(int argc,char* argv[])
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow(
        "Minecraft",
        1280,
        720,
        SDL_WINDOW_OPENGL
    );

    SDL_SetWindowRelativeMouseMode(window, true);

    SDL_GLContext context = SDL_GL_CreateContext(window);

    InitOpenGL(1280,720);

    Textures["grass_top.png"]   = LoadTexture("grass_top.png");
    Textures["grass.png"]       = LoadTexture("grass.png");
    Textures["dirt.png"]        = LoadTexture("dirt.png");
    Textures["stone.png"]       = LoadTexture("stone.png");
    Textures["error.png"]       = LoadTexture("error.png");

    GLuint grassTop = LoadTexture("grass_top.png");
    GLuint grassSide = LoadTexture("grass.png");
    GLuint dirt = LoadTexture("dirt.png");

    worldBlocks[posKey(0,0,0)] = "grass";
    worldBlocks[posKey(1,0,0)] = "grass";
    worldBlocks[posKey(2,0,0)] = "grass";
    
    worldBlocks[posKey(0,0,1)] = "grass";
    worldBlocks[posKey(1,0,1)] = "grass";
    worldBlocks[posKey(2,0,1)] = "grass";

    worldBlocks[posKey(0,0,2)] = "grass";
    worldBlocks[posKey(1,0,2)] = "grass";
    worldBlocks[posKey(2,0,2)] = "grass";

    playerX=0;
    playerY=0;
    playerZ=0;

    yaw=0;
    pitch=0;

    float speed= 1.0f;
    float damper = 0.4f;

    Uint64 NOW = SDL_GetPerformanceCounter();
    Uint64 LAST = 0;
    double deltaTime = 0; // Stored in seconds

    bool running=true;
    SDL_Event event;

    while(running)
    {
        LAST = NOW;
        NOW = SDL_GetPerformanceCounter();
        deltaTime = (double)((NOW - LAST) / (double)SDL_GetPerformanceFrequency());
        while(SDL_PollEvent(&event))
        {
            if(event.type==SDL_EVENT_QUIT)
                running=false;

            if(event.type==SDL_EVENT_KEY_DOWN)
            {
                if (event.key.key==SDLK_ESCAPE){
                    return 0;
                }
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                yaw -= event.motion.xrel * damper;
                pitch -= event.motion.yrel * damper;
                if (pitch > 89.0f)  pitch = 89.0f;
                if (pitch < -89.0f) pitch = -89.0f;
            }
        }

        const bool* state = SDL_GetKeyboardState(NULL);
        float rad = yaw * M_PI / 180.0f;
        float fx = sin(rad);   // forward X
        float fz = cos(rad);   // forward Z
        float rx = cos(rad);   // right X
        float rz = -sin(rad);  // right Z

        if (state[SDL_SCANCODE_W]) {
            playerX -= fx * speed * deltaTime;
            playerZ -= fz * speed * deltaTime;
        }
        if (state[SDL_SCANCODE_S]) {
            playerX += fx * speed * deltaTime;
            playerZ += fz * speed * deltaTime;
        }
        if (state[SDL_SCANCODE_D]) {
            playerX += rx * speed * deltaTime;
            playerZ += rz * speed * deltaTime;
        }
        if (state[SDL_SCANCODE_A]) {
            playerX -= rx * speed * deltaTime;
            playerZ -= rz * speed * deltaTime;
        }
        if (state[SDL_SCANCODE_SPACE]) {
            playerY += speed * deltaTime;
        }
        if (state[SDL_SCANCODE_LSHIFT]) {
            playerY -= speed * deltaTime;
        }

        glClearColor(0.5f,0.7f,1.0f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        glRotatef(-pitch,1,0,0);
        glRotatef(-yaw,0,1,0);
        glTranslatef(-playerX,-playerY,-playerZ);
        RenderCube(0.0f, 0.0f, 0.0f, "grass");
        RenderCube(1.0f, 0.0f, 0.0f, "grass");
        RenderCube(2.0f, 0.0f, 0.0f, "grass");
        RenderCube(0.0f, 0.0f, 1.0f, "grass");
        RenderCube(1.0f, 0.0f, 1.0f, "grass");
        RenderCube(2.0f, 0.0f, 1.0f, "grass");
        RenderCube(0.0f, 0.0f, 2.0f, "grass");
        RenderCube(1.0f, 0.0f, 2.0f, "grass");
        RenderCube(2.0f, 0.0f, 2.0f, "grass");

        SDL_GL_SwapWindow(window);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}