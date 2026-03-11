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

float breakCooldown = 0.0f;
const float BREAK_COOLDOWN_TIME = 0.3f;  // seconds

unordered_map<string, string> worldBlocks;
unordered_map<string, GLuint> Textures;

vector<tuple<string, string, string>> TextureAtlas = {
    {"grass_top.png", "grass.png",     "dirt.png"},
    {"dirt.png",      "dirt.png",      "dirt.png"},
    {"stone.png",     "stone.png",     "stone.png"},
    {"log_top.png", "wood.png", "log_top.png"},
    {"error.png",     "error.png",     "error.png"}
};

unordered_map<string, int> KeyMapper = {
    {"error", 0},
    {"grass", 1},
    {"dirt",  2},
    {"stone", 3},
    {"wood", 4}
};

// ─── Helper Functions ───────────────────────────────────────────────────────

string posKey(int x, int y, int z) {
    return to_string(x) + "_" + to_string(y) + "_" + to_string(z);
}

bool isSolid(int x, int y, int z) {
    auto it = worldBlocks.find(posKey(x, y, z));
    return (it != worldBlocks.end()) && (it->second != "air");
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

    int target = it->second;  // 1=grass, 2=dirt, 3=stone

    int idx = 1;  // ← start at 1 to skip error entry at 0
    for (const auto& tup : TextureAtlas) {
        if (idx == target) return tup;
        idx++;
    }

    cout << "Index not found for " << name << " (value=" << target << ")" << endl;
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

    cout << "Loaded texture '" << filename << "' as ID " << tex << endl;

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
    std::unordered_map<GLuint, GLuint> vaos;   // textureID → VAO
    std::unordered_map<GLuint, GLuint> vbos;   // textureID → VBO
    std::unordered_map<GLuint, GLsizei> counts; // textureID → vertex count
    bool dirty = true;
};
unordered_map<string, Chunk> chunks;

string chunkKey(int cx, int cz) {
    return to_string(cx) + "_" + to_string(cz);
}

void buildChunkMesh(Chunk& chunk) {
    // Clean up old data
    for (auto& p : chunk.vaos) glDeleteVertexArrays(1, &p.second);
    for (auto& p : chunk.vbos) glDeleteBuffers(1, &p.second);
    chunk.vaos.clear();
    chunk.vbos.clear();
    chunk.counts.clear();

    std::unordered_map<GLuint, std::vector<Vertex>> vertexGroups;

    for (int lx = 0; lx < 16; ++lx) {
        for (int ly = -30; ly <= 30; ++ly)  {
            for (int lz = 0; lz < 16; ++lz) {
                int wx = chunk.cx * 16 + lx;
                int wy = ly;
                int wz = chunk.cz * 16 + lz;

                string key = posKey(wx, wy, wz);
                if (worldBlocks.find(key) == worldBlocks.end()) continue;

                string type = worldBlocks[key];
                auto [topF, sideF, botF] = Find_tuple(type);

                GLuint topTex  = Textures[topF];
                GLuint sideTex = Textures[sideF];
                GLuint botTex  = Textures[botF];

                // Front (+Z) - use side texture
                if (!isSolid(wx, wy, wz + 1)) {
                    vertexGroups[sideTex].push_back({wx-0.5f, wy-0.5f, wz+0.5f, 0,1});
                    vertexGroups[sideTex].push_back({wx+0.5f, wy-0.5f, wz+0.5f, 1,1});
                    vertexGroups[sideTex].push_back({wx+0.5f, wy+0.5f, wz+0.5f, 1,0});
                    vertexGroups[sideTex].push_back({wx-0.5f, wy-0.5f, wz+0.5f, 0,1});
                    vertexGroups[sideTex].push_back({wx+0.5f, wy+0.5f, wz+0.5f, 1,0});
                    vertexGroups[sideTex].push_back({wx-0.5f, wy+0.5f, wz+0.5f, 0,0});
                }

                // Back (-Z) - side texture
                if (!isSolid(wx, wy, wz - 1)) {
                    vertexGroups[sideTex].push_back({wx+0.5f, wy-0.5f, wz-0.5f, 0,1});
                    vertexGroups[sideTex].push_back({wx-0.5f, wy-0.5f, wz-0.5f, 1,1});
                    vertexGroups[sideTex].push_back({wx-0.5f, wy+0.5f, wz-0.5f, 1,0});
                    vertexGroups[sideTex].push_back({wx+0.5f, wy-0.5f, wz-0.5f, 0,1});
                    vertexGroups[sideTex].push_back({wx-0.5f, wy+0.5f, wz-0.5f, 1,0});
                    vertexGroups[sideTex].push_back({wx+0.5f, wy+0.5f, wz-0.5f, 0,0});
                }

                // Left (-X) - side texture
                if (!isSolid(wx - 1, wy, wz)) {
                    vertexGroups[sideTex].push_back({wx-0.5f, wy-0.5f, wz-0.5f, 0,1});
                    vertexGroups[sideTex].push_back({wx-0.5f, wy-0.5f, wz+0.5f, 1,1});
                    vertexGroups[sideTex].push_back({wx-0.5f, wy+0.5f, wz+0.5f, 1,0});
                    vertexGroups[sideTex].push_back({wx-0.5f, wy-0.5f, wz-0.5f, 0,1});
                    vertexGroups[sideTex].push_back({wx-0.5f, wy+0.5f, wz+0.5f, 1,0});
                    vertexGroups[sideTex].push_back({wx-0.5f, wy+0.5f, wz-0.5f, 0,0});
                }

                // Right (+X) - side texture
                if (!isSolid(wx + 1, wy, wz)) {
                    vertexGroups[sideTex].push_back({wx+0.5f, wy-0.5f, wz+0.5f, 0,1});
                    vertexGroups[sideTex].push_back({wx+0.5f, wy-0.5f, wz-0.5f, 1,1});
                    vertexGroups[sideTex].push_back({wx+0.5f, wy+0.5f, wz-0.5f, 1,0});
                    vertexGroups[sideTex].push_back({wx+0.5f, wy-0.5f, wz+0.5f, 0,1});
                    vertexGroups[sideTex].push_back({wx+0.5f, wy+0.5f, wz-0.5f, 1,0});
                    vertexGroups[sideTex].push_back({wx+0.5f, wy+0.5f, wz+0.5f, 0,0});
                }

                // Top (+Y) - top texture
                if (!isSolid(wx, wy + 1, wz)) {
                    vertexGroups[topTex].push_back({wx-0.5f, wy+0.5f, wz+0.5f, 0,1});
                    vertexGroups[topTex].push_back({wx+0.5f, wy+0.5f, wz+0.5f, 1,1});
                    vertexGroups[topTex].push_back({wx+0.5f, wy+0.5f, wz-0.5f, 1,0});
                    vertexGroups[topTex].push_back({wx-0.5f, wy+0.5f, wz+0.5f, 0,1});
                    vertexGroups[topTex].push_back({wx+0.5f, wy+0.5f, wz-0.5f, 1,0});
                    vertexGroups[topTex].push_back({wx-0.5f, wy+0.5f, wz-0.5f, 0,0});
                }

                // Bottom (-Y) - bottom texture
                if (!isSolid(wx, wy - 1, wz)) {
                    //cout << "Bottom face of " << type << " at (" << wx << "," << wy << "," << wz << ") using texture ID " << botTex << endl;
                    vertexGroups[botTex].push_back({wx-0.5f, wy-0.5f, wz-0.5f, 0,1});
                    vertexGroups[botTex].push_back({wx+0.5f, wy-0.5f, wz-0.5f, 1,1});
                    vertexGroups[botTex].push_back({wx+0.5f, wy-0.5f, wz+0.5f, 1,0});
                    vertexGroups[botTex].push_back({wx-0.5f, wy-0.5f, wz-0.5f, 0,1});
                    vertexGroups[botTex].push_back({wx+0.5f, wy-0.5f, wz+0.5f, 1,0});
                    vertexGroups[botTex].push_back({wx-0.5f, wy-0.5f, wz+0.5f, 0,0});
                }
            }
        }
    }

    // Create one VAO/VBO per texture group
    for (auto& [texID, verts] : vertexGroups) {
        if (verts.empty()) continue;

        GLuint vao, vbo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3*sizeof(float)));
        glEnableVertexAttribArray(1);

        chunk.vaos[texID] = vao;
        chunk.vbos[texID] = vbo;
        chunk.counts[texID] = verts.size();
    }

    chunk.dirty = false;
}

// ─── Chunk Generation ──────────────────────────────────────────────────────

void AddBlock(int x, int y, int z, string type) {
    string key = posKey(x, y, z);
    if (!isSolid(x,y, z)){
    worldBlocks[key] = type;
    }
    if (type == "grass"){
    cout << "Placed " << type << " at " << key << endl;  // ← add this line
    }
}

void AddLotsOfBlocks(int startX, int startY, int startZ, int len, int height, int width, string type) {
    for (int dz = 0; dz < len; ++dz) {
        for (int dy = 0; dy < height; ++dy) {
            for (int dx = 0; dx < width; ++dx) {
                int x = startX + dx;
                int y = startY - dy;
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

    int grasslayers = 1;
    int dirtlayers = 3;
    int stonelayers = 5;

    // Grass layer at y=0 (top exposed)
    AddLotsOfBlocks(cx*16,  0, cz*16, 16, grasslayers, 16, "grass");

    // Dirt layers below grass (y=-1 to -3)
    AddLotsOfBlocks(cx*16, 0 - grasslayers, cz*16, 16, dirtlayers, 16, "dirt");

    // Stone below dirt (y=-4 to -8)
    AddLotsOfBlocks(cx*16, 0 - grasslayers - dirtlayers, cz*16, 16, stonelayers, 16, "stone");

    AddBlock(cx * 16, 0, cz, "wood");
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

void tryBreakInFront() {
    if (breakCooldown > 0.0f) {
        cout << "Break on cooldown (" << breakCooldown << "s left)" << endl;
        return;
    }

    cout << "Trying to break block in front" << endl;

    // Look direction
    float radYaw   = glm::radians(yaw);
    float radPitch = glm::radians(pitch);
    glm::vec3 dir(
        cos(radYaw) * cos(radPitch),
        sin(radPitch),
        sin(radYaw) * cos(radPitch)
    );
    dir = glm::normalize(dir);

    glm::vec3 start(playerX, playerY, playerZ);

    for (int step = 1; step <= 5; ++step) {
        glm::vec3 pos = start + dir * (float)step;

        int bx = floor(pos.x);
        int by = floor(pos.y);
        int bz = floor(pos.z);

        string key = posKey(bx, by, bz);
        if (worldBlocks.erase(key)) {
            cout << "Broke block at (" << bx << ", " << by << ", " << bz << ")" << endl;

            int cx = bx / 16;
            int cz = bz / 16;
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dz = -1; dz <= 1; ++dz) {
                    string ckey = chunkKey(cx + dx, cz + dz);
                    auto it = chunks.find(ckey);
                    if (it != chunks.end()) {
                        it->second.dirty = true;
                    }
                }
            }
            breakCooldown = BREAK_COOLDOWN_TIME;
            return;
        }
    }

    cout << "No breakable block in front" << endl;
    breakCooldown = BREAK_COOLDOWN_TIME / 2;
}

void AddTree(int BaseBlockX, int BaseBlockY, int BaseBlockZ){

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
        cout << "Window failed: " << SDL_GetError() << endl;
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

    // Mouse setup - critical for macOS
    SDL_SetWindowRelativeMouseMode(window, true);
    SDL_HideCursor();
    SDL_RaiseWindow(window);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Load textures
    Textures["grass_top.png"] = LoadTexture("grass_top.png");
    Textures["grass.png"]     = LoadTexture("grass.png");
    Textures["dirt.png"]      = LoadTexture("dirt.png");
    Textures["stone.png"]     = LoadTexture("stone.png");
    Textures["error.png"]     = LoadTexture("error.png");
    Textures["wood.png"]      = LoadTexture("wood.png");
    Textures["log_top.png"]   = LoadTexture("log_top.png");

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

        breakCooldown -= (float)dt;
        if (breakCooldown < 0.0f) breakCooldown = 0.0f;

        // Event loop
        while (SDL_PollEvent(&e)) {

            if (e.type == SDL_EVENT_QUIT) {
                running = false;
            }

            if (e.type == SDL_EVENT_KEY_DOWN) {
                cout << "[KEY] " << e.key.key << endl;
                if (e.key.key == SDLK_ESCAPE) running = false;

                // Break on E
                if (e.key.key == SDLK_E) {
                    cout << "E pressed - trying to break" << endl;
                    tryBreakInFront();
                }
            }

            if (e.type == SDL_EVENT_MOUSE_MOTION) {
                yaw   += e.motion.xrel * mouseSensitivity;
                pitch -= e.motion.yrel * mouseSensitivity;
                if (pitch > 89.0f) pitch = 89.0f;
                if (pitch < -89.0f) pitch = -89.0f;
            }

            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                cout << "[MOUSE DOWN] Button: " << (int)e.button.button << endl;
                if (e.button.button == SDL_BUTTON_LEFT) {
                    cout << "Left click - trying to break" << endl;
                    tryBreakInFront();
                }
            }
        }

        // Poll mouse button state every frame (backup for event drop)
        float mx, my;
        Uint32 mouseState = SDL_GetMouseState(&mx, &my);

        if ((mouseState & SDL_BUTTON_LMASK) && breakCooldown <= 0.0f) {
            cout << "Mouse held - breaking" << endl;
            tryBreakInFront();
        }

        // Old free-fly movement
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
for (auto& [ckey, chunk] : chunks) {
    if (chunk.dirty) buildChunkMesh(chunk);

    if (chunk.counts.empty()) continue;

    //ing chunk " << ckey << " with " << chunk.vaos.size() << " texture groups" << endl;

    for (const auto& [texID, vao] : chunk.vaos) {
        glBindTexture(GL_TEXTURE_2D, texID);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, chunk.counts.at(texID));
    }
}

        SDL_GL_SwapWindow(window);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}