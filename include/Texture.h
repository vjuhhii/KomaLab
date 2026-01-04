#ifndef TEXTURE_H
#define TEXTURE_H

#include <glad/glad.h>
#include <string>

class Texture
{
public:
    unsigned int ID;
    int width, height, nrChannels;

    Texture(const char* imagePath);

    void bind();
};

#endif