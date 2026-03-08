#include <iostream>
#include <cmath>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_image/SDL_image.h>

GLuint LoadTexture(const char* file)
{
    SDL_Surface* surface = IMG_Load(file);

    if (!surface) {
        std::cout << "Failed to load image: " << SDL_GetError() << std::endl;
        return 0;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    GLint format;

switch (surface->format) {
    case SDL_PIXELFORMAT_RGBA32:
        format = GL_RGBA;
        break;
    case SDL_PIXELFORMAT_BGRA32:
        format = GL_BGRA;
        break;
    case SDL_PIXELFORMAT_RGB24:
        format = GL_RGB;
        break;
    case SDL_PIXELFORMAT_BGR24:
        format = GL_BGR;
        break;
    default:
        format = GL_RGBA;
        break;
}

glTexImage2D(
    GL_TEXTURE_2D,
    0,
    GL_RGBA,
    surface->w,
    surface->h,
    0,
    format,
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
    glViewport(0,0,width,height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float aspect = (float)width/(float)height;
    glFrustum(-aspect, aspect, -1, 1, 0.1, 100);   // or even 0.05

    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
}

void DrawCube(GLuint top, GLuint side, GLuint bottom, float x,float y,float z)
{
    glPushMatrix();
    glTranslatef(x,y,z);

    glBegin(GL_QUADS);

    glBindTexture(GL_TEXTURE_2D,side);

    // Front
    glTexCoord2f(0,1); glVertex3f(-0.5,-0.5, 0.5);
    glTexCoord2f(1,1); glVertex3f( 0.5,-0.5, 0.5);
    glTexCoord2f(1,0); glVertex3f( 0.5, 0.5, 0.5);
    glTexCoord2f(0,0); glVertex3f(-0.5, 0.5, 0.5);

    // Back
    glTexCoord2f(0,1); glVertex3f( 0.5,-0.5,-0.5);
    glTexCoord2f(1,1); glVertex3f(-0.5,-0.5,-0.5);
    glTexCoord2f(1,0); glVertex3f(-0.5, 0.5,-0.5);
    glTexCoord2f(0,0); glVertex3f( 0.5, 0.5,-0.5);

    // Left
    glTexCoord2f(0,1); glVertex3f(-0.5,-0.5,-0.5);
    glTexCoord2f(1,1); glVertex3f(-0.5,-0.5, 0.5);
    glTexCoord2f(1,0); glVertex3f(-0.5, 0.5, 0.5);
    glTexCoord2f(0,0); glVertex3f(-0.5, 0.5,-0.5);

    // Right
    glTexCoord2f(0,1); glVertex3f(0.5,-0.5, 0.5);
    glTexCoord2f(1,1); glVertex3f(0.5,-0.5,-0.5);
    glTexCoord2f(1,0); glVertex3f(0.5, 0.5,-0.5);
    glTexCoord2f(0,0); glVertex3f(0.5, 0.5, 0.5);

    glEnd();

    glBindTexture(GL_TEXTURE_2D,top);

    glBegin(GL_QUADS);

    // Top
    glTexCoord2f(0,1); glVertex3f(-0.5,0.5, 0.5);
    glTexCoord2f(1,1); glVertex3f( 0.5,0.5, 0.5);
    glTexCoord2f(1,0); glVertex3f( 0.5,0.5,-0.5);
    glTexCoord2f(0,0); glVertex3f(-0.5,0.5,-0.5);

    glEnd();

    glBindTexture(GL_TEXTURE_2D,bottom);

    glBegin(GL_QUADS);

    // Bottom
    glTexCoord2f(0,1); glVertex3f(-0.5,-0.5,-0.5);
    glTexCoord2f(1,1); glVertex3f( 0.5,-0.5,-0.5);
    glTexCoord2f(1,0); glVertex3f( 0.5,-0.5, 0.5);
    glTexCoord2f(0,0); glVertex3f(-0.5,-0.5, 0.5);

    glEnd();

    glPopMatrix();
}

int main(int argc,char* argv[])
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow(
        "Voxel Test",
        1280,
        720,
        SDL_WINDOW_OPENGL
    );

    SDL_SetWindowRelativeMouseMode(window, true);

    SDL_GLContext context = SDL_GL_CreateContext(window);

    InitOpenGL(1280,720);

    GLuint grassTop = LoadTexture("grass_top.png");
    GLuint grassSide = LoadTexture("grass.png");
    GLuint dirt = LoadTexture("dirt.png");

    float playerX=0;
    float playerY=0;
    float playerZ=3;

    float yaw=0;
    float pitch=0;

    float speed=10.0f;
    float damper = 0.8f;

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
                    SDL_SetWindowRelativeMouseMode(window, false);
                }
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                yaw -= event.motion.xrel * damper;
                pitch -= event.motion.yrel * damper;
            }
        }

        const bool* state = SDL_GetKeyboardState(NULL);
if (state[SDL_SCANCODE_W]) {
    playerX -= sin(yaw * M_PI / 180.0) * speed * deltaTime;
    playerZ -= cos(yaw * M_PI / 180.0) * speed * deltaTime;   // ← changed - to +
}

if (state[SDL_SCANCODE_S]) {
    playerX += sin(yaw * M_PI / 180.0) * speed * deltaTime;
    playerZ += cos(yaw * M_PI / 180.0) * speed * deltaTime;   // ← changed + to -
}
        if (state[SDL_SCANCODE_A]) {
            playerX-=cos(yaw*M_PI/180)*speed*deltaTime;
            playerZ-=sin(yaw*M_PI/180)*speed*deltaTime;
        }
        if (state[SDL_SCANCODE_D]) {
            playerX+=cos(yaw*M_PI/180)*speed*deltaTime;
            playerZ+=sin(yaw*M_PI/180)*speed*deltaTime;
        }

        glClearColor(0.5f,0.7f,1.0f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glLoadIdentity();

        glRotatef(-pitch,1,0,0);
        glRotatef(-yaw,0,1,0);
        glTranslatef(-playerX,-playerY,-playerZ);

        DrawCube(grassTop,grassSide,dirt,0,0,0);

        SDL_GL_SwapWindow(window);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}