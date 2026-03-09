#include <iostream>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <string>
#include <sstream>
#include <tuple>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

// GLAD – must be included before any OpenGL calls
#include "glad/glad.h"

// GLM for matrices and vectors
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace std;

// ─── Globals ────────────────────────────────────────────────────────────────

float playerX = 0.0f;
float playerY = 10.0f;
float playerZ = 5.0f;

float yaw   = -90.0f;   // starts looking along -Z
float pitch = 0.0f;

float speed = 10.0f;
float mouseSensitivity = 0.15f;

// Physics
float gravity = -20.0f;      // m/s² — tweak this
float jumpVelocity = 8.0f;   // initial upward speed when jumping
float verticalVelocity = 0.0f; // current falling/jumping speed

bool isOnGround = false;     // we'll update this every frame

unordered_map<string, string> worldBlocks;
unordered_map<string, GLuint> Textures;

vector<tuple<string, string, string>> TextureAtlas = {
    {"grass_top.png", "grass.png",     "dirt.png"},
    {"dirt.png",      "dirt.png",      "dirt.png"},
    {"stone.png",     "stone.png",     "stone.png"},
    {"error.png",     "error.png",     "error.png"}
};

unordered_map<string, int> KeyMapper = {
    {"error", 0},
    {"grass", 1},
    {"dirt",  2},
    {"stone", 3}
};

// ─── Helper Functions ───────────────────────────────────────────────────────

string posKey(int x, int y, int z) {
    return to_string(x) + "_" + to_string(y) + "_" + to_string(z);
}

bool isSolid(int x, int y, int z) {
    auto it = worldBlocks.find(posKey(x, y, z));
    return (it != worldBlocks.end()) && (it->second != "air");
}

// Returns true if a block was hit, and fills out hitPos with the block coordinates
bool raycastBreak(float maxDistance, int& hitX, int& hitY, int& hitZ) {
    // Camera position
    glm::vec3 pos(playerX, playerY, playerZ);

    // Look direction (same as in your view matrix)
    float radYaw   = glm::radians(yaw);
    float radPitch = glm::radians(pitch);
    glm::vec3 dir(
        cos(radYaw) * cos(radPitch),
        sin(radPitch),
        sin(radYaw) * cos(radPitch)
    );
    dir = glm::normalize(dir);

    // Step along the ray in small increments
    float dist = 0.0f;
    const float step = 0.05f;  // smaller = more accurate, but slower

    while (dist < maxDistance) {
        glm::vec3 current = pos + dir * dist;

        hitX = static_cast<int>(floor(current.x));
        hitY = static_cast<int>(floor(current.y));
        hitZ = static_cast<int>(floor(current.z));

        if (isSolid(hitX, hitY, hitZ)) {
            return true;  // found a block!
        }

        dist += step;
    }

    return false;  // no block hit within range
}

tuple<int,int,int> ParseCoords(const string& str) {
    stringstream ss(str);
    string token;
    int x,y,z;
    getline(ss, token, '_'); x = stoi(token);
    getline(ss, token, '_'); y = stoi(token);
    getline(ss, token, '_'); z = stoi(token);
    return {x, y, z};
}

tuple<string,string,string> Find_tuple(const string& name) {
    auto it = KeyMapper.find(name);
    if (it == KeyMapper.end()) {
        cout << "Unknown block: " << name << endl;
        return {"error.png", "error.png", "error.png"};
    }

    int idx = 0;
    for (const auto& tup : TextureAtlas) {
        if (idx == it->second) return tup;
        idx++;
    }
    return {"error.png", "error.png", "error.png"};
}

// ─── Texture Loading ────────────────────────────────────────────────────────

GLuint LoadTexture(const string& filename) {
    string fullpath = string(SDL_GetBasePath()) + "Assets/" + filename;
    SDL_Surface* raw = IMG_Load(fullpath.c_str());
    if (!raw) {
        cout << "Failed to load " << filename << ": " << SDL_GetError() << endl;
        raw = IMG_Load((string(SDL_GetBasePath()) + "Assets/error.png").c_str());
    }

    SDL_Surface* converted = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(raw);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, converted->pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    SDL_DestroySurface(converted);
    return tex;
}

// ─── Shader ─────────────────────────────────────────────────────────────────

GLuint shaderProgram;

void createShader() {
    const char* vs = R"(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec2 aTex;
    uniform mat4 uMVP;
    out vec2 TexCoord;
    void main() {
        gl_Position = uMVP * vec4(aPos, 1.0);
        TexCoord = aTex;
    }
    )";

    const char* fs = R"(
    #version 330 core
    in vec2 TexCoord;
    out vec4 FragColor;
    uniform sampler2D uTexture;
    void main() {
        FragColor = texture(uTexture, TexCoord);
    }
    )";

    GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vshader, 1, &vs, nullptr);
    glCompileShader(vshader);

    GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fshader, 1, &fs, nullptr);
    glCompileShader(fshader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vshader);
    glAttachShader(shaderProgram, fshader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vshader);
    glDeleteShader(fshader);
}

// ─── Chunk & Mesh ───────────────────────────────────────────────────────────

struct Vertex { float x,y,z, u,v; };

struct Chunk {
    int cx, cz;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei count = 0;
    bool dirty = true;
};

unordered_map<string, Chunk> chunks;

string chunkKey(int cx, int cz) {
    return to_string(cx) + "_" + to_string(cz);
}

void buildChunkMesh(Chunk& chunk) {
    if (chunk.vao) glDeleteVertexArrays(1, &chunk.vao);
    if (chunk.vbo) glDeleteBuffers(1, &chunk.vbo);
    chunk.vao = chunk.vbo = 0;
    chunk.count = 0;

    vector<Vertex> vertices;

    for (int lx = 0; lx < 16; ++lx) {
        for (int ly = -10; ly <= 5; ++ly) {
            for (int lz = 0; lz < 16; ++lz) {
                int wx = chunk.cx * 16 + lx;
                int wy = ly;
                int wz = chunk.cz * 16 + lz;

                string key = posKey(wx, wy, wz);
                if (worldBlocks.find(key) == worldBlocks.end()) continue;

                string type = worldBlocks[key];
                auto [topF, sideF, botF] = Find_tuple(type);

                // Front (+Z)
                if (!isSolid(wx, wy, wz + 1)) {
                    vertices.push_back({wx-0.5f, wy-0.5f, wz+0.5f, 0,1});
                    vertices.push_back({wx+0.5f, wy-0.5f, wz+0.5f, 1,1});
                    vertices.push_back({wx+0.5f, wy+0.5f, wz+0.5f, 1,0});
                    vertices.push_back({wx-0.5f, wy-0.5f, wz+0.5f, 0,1});
                    vertices.push_back({wx+0.5f, wy+0.5f, wz+0.5f, 1,0});
                    vertices.push_back({wx-0.5f, wy+0.5f, wz+0.5f, 0,0});
                }

                // Back (-Z)
                if (!isSolid(wx, wy, wz - 1)) {
                    vertices.push_back({wx+0.5f, wy-0.5f, wz-0.5f, 0,1});
                    vertices.push_back({wx-0.5f, wy-0.5f, wz-0.5f, 1,1});
                    vertices.push_back({wx-0.5f, wy+0.5f, wz-0.5f, 1,0});
                    vertices.push_back({wx+0.5f, wy-0.5f, wz-0.5f, 0,1});
                    vertices.push_back({wx-0.5f, wy+0.5f, wz-0.5f, 1,0});
                    vertices.push_back({wx+0.5f, wy+0.5f, wz-0.5f, 0,0});
                }

                // Left (-X)
                if (!isSolid(wx - 1, wy, wz)) {
                    vertices.push_back({wx-0.5f, wy-0.5f, wz-0.5f, 0,1});
                    vertices.push_back({wx-0.5f, wy-0.5f, wz+0.5f, 1,1});
                    vertices.push_back({wx-0.5f, wy+0.5f, wz+0.5f, 1,0});
                    vertices.push_back({wx-0.5f, wy-0.5f, wz-0.5f, 0,1});
                    vertices.push_back({wx-0.5f, wy+0.5f, wz+0.5f, 1,0});
                    vertices.push_back({wx-0.5f, wy+0.5f, wz-0.5f, 0,0});
                }

                // Right (+X)
                if (!isSolid(wx + 1, wy, wz)) {
                    vertices.push_back({wx+0.5f, wy-0.5f, wz+0.5f, 0,1});
                    vertices.push_back({wx+0.5f, wy-0.5f, wz-0.5f, 1,1});
                    vertices.push_back({wx+0.5f, wy+0.5f, wz-0.5f, 1,0});
                    vertices.push_back({wx+0.5f, wy-0.5f, wz+0.5f, 0,1});
                    vertices.push_back({wx+0.5f, wy+0.5f, wz-0.5f, 1,0});
                    vertices.push_back({wx+0.5f, wy+0.5f, wz+0.5f, 0,0});
                }

                // Top (+Y)
                if (!isSolid(wx, wy + 1, wz)) {
                    vertices.push_back({wx-0.5f, wy+0.5f, wz+0.5f, 0,1});
                    vertices.push_back({wx+0.5f, wy+0.5f, wz+0.5f, 1,1});
                    vertices.push_back({wx+0.5f, wy+0.5f, wz-0.5f, 1,0});
                    vertices.push_back({wx-0.5f, wy+0.5f, wz+0.5f, 0,1});
                    vertices.push_back({wx+0.5f, wy+0.5f, wz-0.5f, 1,0});
                    vertices.push_back({wx-0.5f, wy+0.5f, wz-0.5f, 0,0});
                }

                // Bottom (-Y)
                if (!isSolid(wx, wy - 1, wz)) {
                    vertices.push_back({wx-0.5f, wy-0.5f, wz-0.5f, 0,1});
                    vertices.push_back({wx+0.5f, wy-0.5f, wz-0.5f, 1,1});
                    vertices.push_back({wx+0.5f, wy-0.5f, wz+0.5f, 1,0});
                    vertices.push_back({wx-0.5f, wy-0.5f, wz-0.5f, 0,1});
                    vertices.push_back({wx+0.5f, wy-0.5f, wz+0.5f, 1,0});
                    vertices.push_back({wx-0.5f, wy-0.5f, wz+0.5f, 0,0});
                }
            }
        }
    }

    if (vertices.empty()) {
        chunk.dirty = false;
        return;
    }

    glGenVertexArrays(1, &chunk.vao);
    glGenBuffers(1, &chunk.vbo);

    glBindVertexArray(chunk.vao);
    glBindBuffer(GL_ARRAY_BUFFER, chunk.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    chunk.count = vertices.size();
    chunk.dirty = false;
}

// ─── Chunk Generation ──────────────────────────────────────────────────────

void AddBlock(int x, int y, int z, string type) {
    worldBlocks[posKey(x, y, z)] = type;
}

void AddLotsOfBlocks(int startX, int startY, int startZ, int len, int height, int width, string type) {
    for (int dz = 0; dz < len; ++dz) {
        for (int dy = 0; dy < height; ++dy) {
            for (int dx = 0; dx < width; ++dx) {
                int x = startX + dx;
                int y = startY + dy;   // ← note: removed -dy, use +dy for normal up direction
                int z = startZ + dz;
                AddBlock(x, y, z, type);
            }
        }
    }
}

void GenerateChunk(int cx, int cz) {
    string ckey = chunkKey(cx, cz);
    if (chunks.count(ckey)) return;

    Chunk ch;
    ch.cx = cx;
    ch.cz = cz;
    ch.dirty = true;
    chunks[ckey] = ch;

    AddLotsOfBlocks(cx*16,  0, cz*16, 16, 1, 16, "grass");
    AddLotsOfBlocks(cx*16, -1, cz*16, 16, 3, 16, "dirt");
    AddLotsOfBlocks(cx*16, -4, cz*16, 16, 5, 16, "stone");
}

void GenerateUnloadedChunks() {
    int px = floor(playerX / 16.0f);
    int pz = floor(playerZ / 16.0f);

    for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
            GenerateChunk(px + dx, pz + dz);
        }
    }
}

// ─── Main ──────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        cout << "SDL init failed: " << SDL_GetError() << endl;
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_Window* window = SDL_CreateWindow("Voxel Test - Modern GL", 1280, 720, SDL_WINDOW_OPENGL);
    if (!window) {
        cout << "Window creation failed: " << SDL_GetError() << endl;
        SDL_Quit();
        return 1;
    }

    SDL_GL_CreateContext(window);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        cout << "GLAD failed\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Load textures
    Textures["grass_top.png"] = LoadTexture("grass_top.png");
    Textures["grass.png"]     = LoadTexture("grass.png");
    Textures["dirt.png"]      = LoadTexture("dirt.png");
    Textures["stone.png"]     = LoadTexture("stone.png");
    Textures["error.png"]     = LoadTexture("error.png");

    createShader();
    glUseProgram(shaderProgram);

    GenerateChunk(0, 0);

    bool running = true;
    SDL_Event e;

    Uint64 lastTime = SDL_GetPerformanceCounter();

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - lastTime) / SDL_GetPerformanceFrequency();
        lastTime = now;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                running = false;
            }

            if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.key == SDLK_ESCAPE) {
                    running = false;
                }
            }

            // Mouse look (continuous motion)
            if (e.type == SDL_EVENT_MOUSE_MOTION) {
                yaw   += e.motion.xrel * mouseSensitivity;
                pitch -= e.motion.yrel * mouseSensitivity;

                if (pitch > 89.0f)  pitch = 89.0f;
                if (pitch < -89.0f) pitch = -89.0f;
            }

            // Block breaking on left mouse click
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    int hx, hy, hz;
                    if (raycastBreak(6.0f, hx, hy, hz)) {  // 6 blocks reach — feel free to change
                        string key = posKey(hx, hy, hz);
                        if (worldBlocks.erase(key)) {  // erase returns >0 if key existed
                            // Mark nearby chunks dirty to rebuild mesh
                            int cx = hx / 16;
                            int cz = hz / 16;

                            for (int dx = -1; dx <= 1; ++dx) {
                                for (int dz = -1; dz <= 1; ++dz) {
                                    string ckey = chunkKey(cx + dx, cz + dz);
                                    auto it = chunks.find(ckey);
                                    if (it != chunks.end()) {
                                        it->second.dirty = true;
                                    }
                                }
                            }
                            cout << "Broke block at (" << hx << ", " << hy << ", " << hz << ")\n";
                        }
                    } else {
                        cout << "No block hit within range\n";
                    }
                }
                cout << "click!" << endl;
            }
            cout << "looping.." << endl;
        }
        std::cout << "loopign out" << std::endl;

// Old simple movement (free fly, no gravity/collision)
const bool* keys = SDL_GetKeyboardState(nullptr);

float rad = glm::radians(yaw);
glm::vec3 forward(cos(rad), 0.0f, sin(rad));
forward = glm::normalize(forward);
glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));

glm::vec3 moveDir(0.0f);

if (keys[SDL_SCANCODE_W]) moveDir += forward;
if (keys[SDL_SCANCODE_S]) moveDir -= forward;
if (keys[SDL_SCANCODE_A]) moveDir -= right;
if (keys[SDL_SCANCODE_D]) moveDir += right;

moveDir *= speed * (float)dt;

playerX += moveDir.x;
playerZ += moveDir.z;

if (keys[SDL_SCANCODE_SPACE]) playerY += speed * (float)dt;
if (keys[SDL_SCANCODE_LSHIFT]) playerY -= speed * (float)dt;
        GenerateUnloadedChunks();

        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = glm::perspective(glm::radians(70.0f), 1280.0f / 720.0f, 0.1f, 200.0f);

        glm::vec3 direction(
            cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
            sin(glm::radians(pitch)),
            sin(glm::radians(yaw)) * cos(glm::radians(pitch))
        );

        glm::mat4 view = glm::lookAt(
            glm::vec3(playerX, playerY, playerZ),
            glm::vec3(playerX, playerY, playerZ) + direction,
            glm::vec3(0,1,0)
        );

        glm::mat4 mvp = proj * view;
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));

        glUseProgram(shaderProgram);
        for (auto& [_, chunk] : chunks) {
            if (chunk.dirty) buildChunkMesh(chunk);
            if (chunk.count == 0) continue;

            glBindTexture(GL_TEXTURE_2D, Textures["grass.png"]);
            glBindVertexArray(chunk.vao);
            glDrawArrays(GL_TRIANGLES, 0, chunk.count);
        }

        SDL_GL_SwapWindow(window);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}