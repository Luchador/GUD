#include <stdint.h>
#include <string.h>
#include "modellighting.h"

const ModelLightingSettings g_ModelLightingDefaults = {
    {0.4f, 0.4f, 0.4f, 1},
    {0.8f, 0.8f, 0.8f, 1},
    {0.65f, 0.65f, 0.65f, 1},
    {-0.4f, 0.8f, -0.6f, 0},
    32.0f
};

static const char g_ModelLightingVertexShader[] =
    "#version 120\n"
    "varying vec3 eyePosition;\n"
    "varying vec3 eyeNormal;\n"
    "varying vec4 vertexColor;\n"
    "void main() {\n"
    "    vec4 position = gl_ModelViewMatrix * gl_Vertex;\n"
    "    eyePosition = position.xyz;\n"
    "    eyeNormal = gl_NormalMatrix * gl_Normal;\n"
    "    vertexColor = gl_Color;\n"
    "    gl_Position = gl_ProjectionMatrix * position;\n"
    "}\n";

static const char g_ModelLightingFragmentShader[] =
    "#version 120\n"
    "varying vec3 eyePosition;\n"
    "varying vec3 eyeNormal;\n"
    "varying vec4 vertexColor;\n"
    "void main() {\n"
    "    vec3 normal = normalize(eyeNormal);\n"
    "    if (!gl_FrontFacing) normal = -normal;\n"
    "    vec3 light = normalize(gl_LightSource[0].position.xyz);\n"
    "    vec3 viewer = normalize(-eyePosition);\n"
    "    float diffuse = max(dot(normal, light), 0.0);\n"
    "    float specular = 0.0;\n"
    "    if (diffuse > 0.0) {\n"
    "        float reflection = max(dot(reflect(-light, normal), viewer), 0.0);\n"
    "        if (reflection > 0.0)\n"
    "            specular = pow(reflection, gl_FrontMaterial.shininess);\n"
    "    }\n"
    "    vec3 color = vertexColor.rgb * (gl_LightModel.ambient.rgb\n"
    "        + gl_LightSource[0].diffuse.rgb * diffuse);\n"
    "    color += gl_FrontMaterial.specular.rgb * gl_LightSource[0].specular.rgb * specular;\n"
    "    gl_FragColor = vec4(color, vertexColor.a);\n"
    "}\n";

static void *ModelLightingProc(const char *name)
{
    PROC proc = wglGetProcAddress(name);
    /* WGL can return sentinel values instead of NULL for an unsupported API. */
    return (intptr_t)proc == -1 || (uintptr_t)proc <= 3 ? NULL : (void *)proc;
}

BOOL ModelLightingInit(ModelLighting *lighting)
{
    PFNGLCREATESHADERPROC createshader = (PFNGLCREATESHADERPROC)ModelLightingProc("glCreateShader");
    PFNGLSHADERSOURCEPROC shadersource = (PFNGLSHADERSOURCEPROC)ModelLightingProc("glShaderSource");
    PFNGLCOMPILESHADERPROC compileshader = (PFNGLCOMPILESHADERPROC)ModelLightingProc("glCompileShader");
    PFNGLGETSHADERIVPROC getshaderiv = (PFNGLGETSHADERIVPROC)ModelLightingProc("glGetShaderiv");
    PFNGLGETSHADERINFOLOGPROC shaderlog = (PFNGLGETSHADERINFOLOGPROC)ModelLightingProc("glGetShaderInfoLog");
    PFNGLDELETESHADERPROC deleteshader = (PFNGLDELETESHADERPROC)ModelLightingProc("glDeleteShader");
    PFNGLCREATEPROGRAMPROC createprogram = (PFNGLCREATEPROGRAMPROC)ModelLightingProc("glCreateProgram");
    PFNGLATTACHSHADERPROC attachshader = (PFNGLATTACHSHADERPROC)ModelLightingProc("glAttachShader");
    PFNGLLINKPROGRAMPROC linkprogram = (PFNGLLINKPROGRAMPROC)ModelLightingProc("glLinkProgram");
    PFNGLGETPROGRAMIVPROC getprogramiv = (PFNGLGETPROGRAMIVPROC)ModelLightingProc("glGetProgramiv");
    PFNGLGETPROGRAMINFOLOGPROC programlog = (PFNGLGETPROGRAMINFOLOGPROC)ModelLightingProc("glGetProgramInfoLog");
    GLuint shaders[2] = {0}, program = 0;
    const GLenum types[2] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
    const GLchar *sources[2] = {g_ModelLightingVertexShader, g_ModelLightingFragmentShader};
    GLint status;
    char log[2048] = {0};
    memset(lighting, 0, sizeof(*lighting));
    lighting->useprogram = (PFNGLUSEPROGRAMPROC)ModelLightingProc("glUseProgram");
    lighting->deleteprogram = (PFNGLDELETEPROGRAMPROC)ModelLightingProc("glDeleteProgram");
    if (!createshader || !shadersource || !compileshader || !getshaderiv || !shaderlog
        || !deleteshader || !createprogram || !attachshader || !linkprogram
        || !getprogramiv || !programlog || !lighting->useprogram || !lighting->deleteprogram)
    { return FALSE; }
    for (int i = 0; i < 2; i++)
    {
        shaders[i] = createshader(types[i]);
        if (!shaders[i]) { goto done; }
        shadersource(shaders[i], 1, &sources[i], NULL);
        compileshader(shaders[i]);
        getshaderiv(shaders[i], GL_COMPILE_STATUS, &status);
        if (!status) { shaderlog(shaders[i], sizeof(log)-1, NULL, log); goto done; }
    }
    program = createprogram();
    if (!program) { goto done; }
    attachshader(program, shaders[0]); attachshader(program, shaders[1]);
    linkprogram(program);
    getprogramiv(program, GL_LINK_STATUS, &status);
    if (!status) { programlog(program, sizeof(log)-1, NULL, log); goto done; }
    lighting->program = program; program = 0;
done:
    for (int i = 0; i < 2; i++) { if (shaders[i]) { deleteshader(shaders[i]); } }
    if (program) { lighting->deleteprogram(program); }
    if (!lighting->program)
    {
        OutputDebugStringA("GEditor model lighting: using fixed-function specular lighting.\n");
        if (log[0]) { OutputDebugStringA(log); }
    }
    return lighting->program != 0;
}

void ModelLightingBegin(ModelLighting *lighting, const ModelLightingSettings *settings)
{
    static const GLfloat black[4] = {0,0,0,1}, white[4] = {1,1,1,1};
    if (!settings) { settings = &g_ModelLightingDefaults; }
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL); glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, settings->specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, settings->shininess);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, black);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, settings->ambient);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
    glLightfv(GL_LIGHT0, GL_AMBIENT, black);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, settings->diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, white);
    glLightfv(GL_LIGHT0, GL_POSITION, settings->direction);
    if (lighting->useprogram)
    {
        glGetIntegerv(GL_CURRENT_PROGRAM, &lighting->previousprogram);
        lighting->useprogram(lighting->program);
        lighting->active = TRUE;
    }
}

void ModelLightingEnd(ModelLighting *lighting)
{
    if (!lighting->active) { return; }
    lighting->useprogram((GLuint)lighting->previousprogram);
    lighting->active = FALSE;
}

void ModelLightingFree(ModelLighting *lighting)
{
    ModelLightingEnd(lighting);
    if (lighting->program) { lighting->deleteprogram(lighting->program); }
    memset(lighting, 0, sizeof(*lighting));
}
