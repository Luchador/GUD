#ifndef GEDITOR_MODELLIGHTING_H
#define GEDITOR_MODELLIGHTING_H

#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>

/* Shared by lit editor GLBs and future scene renderers. Direction is a
 * world-space directional light; shininess is the Phong exponent (0..128
 * also supported by the fixed-function fallback). Vertex color supplies the
 * diffuse material, while specular is an independent highlight color. */
typedef struct ModelLightingSettings {
    GLfloat ambient[4], diffuse[4], specular[4], direction[4];
    GLfloat shininess;
} ModelLightingSettings;
extern const ModelLightingSettings g_ModelLightingDefaults;

/* One instance per GL context; create, use and free with that context current. */
typedef struct ModelLighting {
    GLuint program;
    PFNGLUSEPROGRAMPROC useprogram;
    PFNGLDELETEPROGRAMPROC deleteprogram;
    GLint previousprogram;
    BOOL active;
} ModelLighting;

/* GLSL 1.20 compatibility shaders share the existing vertex/color/normal
 * arrays and matrices. FALSE leaves a working fixed-function specular path. */
BOOL ModelLightingInit(ModelLighting *lighting);
void ModelLightingFree(ModelLighting *lighting);

/* Begin after the view matrix and before model transforms, with a matching
 * End after drawing. Caller saves/restores fixed-function GL attributes;
 * these functions also restore the previous shader program. */
void ModelLightingBegin(ModelLighting *lighting, const ModelLightingSettings *settings);
void ModelLightingEnd(ModelLighting *lighting);

#endif
