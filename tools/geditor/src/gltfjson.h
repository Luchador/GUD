#ifndef GEDITOR_GLTFJSON_H
#define GEDITOR_GLTFJSON_H
#include <windows.h>
#include <stddef.h>
#include <stdio.h>

/* Shared JSON reader/writer for glTF assets and Render Studio scenes. */
typedef enum GltfJsonType {
    GLTF_JSON_OBJECT,
    GLTF_JSON_ARRAY,
    GLTF_JSON_STRING,
    GLTF_JSON_PRIMITIVE
} GltfJsonType;

typedef struct GltfJsonToken {
    GltfJsonType type;
    size_t start;
    size_t end;
    int parent;
} GltfJsonToken;

BOOL GltfJsonParse(const char *json, size_t length,
                          GltfJsonToken **tokensout, int *countout,
                          const char **reasonout);
int GltfJsonNext(const GltfJsonToken *tokens, int count, int index);
BOOL GltfJsonTokenEquals(const char *json,
                                const GltfJsonToken *token,
                                const char *text);
int GltfJsonObjectGet(const char *json,
                             const GltfJsonToken *tokens, int count,
                             int object, const char *key);
int GltfJsonArrayGet(const GltfJsonToken *tokens, int count,
                            int array, DWORD wanted);
DWORD GltfJsonArrayCount(const GltfJsonToken *tokens, int count,
                                int array);
BOOL GltfJsonUnsigned(const char *json,
                             const GltfJsonToken *token, DWORD *out);
BOOL GltfJsonBool(const char *json, const GltfJsonToken *token,
                         BOOL *out);
char *GltfJsonCopyString(const char *json,
                                const GltfJsonToken *token);
BOOL GltfJsonWriteString(FILE *file, const char *text);
#endif
