#ifndef MODEL_H
#define MODEL_H

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>

#include <cstdint>

class Model
{
public:


protected:
    uint32_t m_offset;
    uint32_t m_numVertices;
};

#endif // MODEL_H
