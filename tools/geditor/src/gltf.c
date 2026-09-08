/*
 * Small glTF 2.0 bridge for GEditor's flattened GoldenEye model geometry.
 *
 * New project assets use JSON .gltf files with an embedded binary buffer so
 * every model remains a single editable file. The reader deliberately uses
 * standard buffers, buffer views, accessors, and mesh primitives rather than
 * depending on the writer's JSON layout. This also permits buffer repacking
 * and indexed triangle primitives emitted by common DCC tools.
 */

#include <windows.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gltf.h"
#include "texload.h"

#define GLTF_COMPONENT_BYTE           5120
#define GLTF_COMPONENT_UNSIGNED_BYTE  5121
#define GLTF_COMPONENT_SHORT          5122
#define GLTF_COMPONENT_UNSIGNED_SHORT 5123
#define GLTF_COMPONENT_UNSIGNED_INT   5125
#define GLTF_COMPONENT_FLOAT          5126
#define GLTF_MODE_TRIANGLES              4
#define GLTF_VERTEX_STRIDE               24u
#define GLTF_MAX_FACES              1000000u

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

typedef struct GltfBuffer {
    unsigned char *data;
    size_t size;
} GltfBuffer;

typedef struct GltfBufferView {
    DWORD buffer;
    size_t offset;
    size_t length;
    size_t stride;
} GltfBufferView;

typedef struct GltfAccessor {
    GltfBufferView view;
    size_t offset;
    DWORD count;
    int componenttype;
    int components;
    BOOL normalized;
} GltfAccessor;

typedef struct GltfBuilder {
    BgVertex *vertices;
    unsigned short *tags;
    DWORD tricount;
    DWORD capacity;
} GltfBuilder;

typedef struct GltfGroup {
    unsigned short tag;
    int texturewidth;
    int textureheight;
    DWORD tricount;
    DWORD firstvertex;
    DWORD written;
    float min[3];
    float max[3];
} GltfGroup;


static BOOL GltfJsonPushToken(GltfJsonToken **tokens, int *count,
                              int *capacity, GltfJsonType type,
                              size_t start, size_t end, int parent)
{
    GltfJsonToken *grown;
    int next;

    if (*count == *capacity)
    {
        if (*capacity > INT_MAX / 2)
        {
            return FALSE;
        }

        next = *capacity != 0 ? *capacity * 2 : 256;
        if ((size_t)next > (size_t)-1 / sizeof(**tokens))
        {
            return FALSE;
        }

        grown = (GltfJsonToken *)realloc(*tokens,
                    (size_t)next * sizeof(**tokens));
        if (grown == NULL)
        {
            return FALSE;
        }
        *tokens = grown;
        *capacity = next;
    }

    (*tokens)[*count].type = type;
    (*tokens)[*count].start = start;
    (*tokens)[*count].end = end;
    (*tokens)[*count].parent = parent;
    (*count)++;
    return TRUE;
}


static BOOL GltfJsonParse(const char *json, size_t length,
                          GltfJsonToken **tokensout, int *countout,
                          const char **reasonout)
{
    GltfJsonToken *tokens = NULL;
    int count = 0;
    int capacity = 0;
    int parent = -1;
    size_t position = 0;

    while (position < length)
    {
        unsigned char ch = (unsigned char)json[position];

        if (isspace(ch) || ch == ':' || ch == ',')
        {
            position++;
            continue;
        }

        if (ch == '{' || ch == '[')
        {
            int index = count;
            GltfJsonType type = ch == '{' ? GLTF_JSON_OBJECT
                                           : GLTF_JSON_ARRAY;

            if (!GltfJsonPushToken(&tokens, &count, &capacity, type,
                                   position, 0, parent))
            {
                goto out_of_memory;
            }
            parent = index;
            position++;
            continue;
        }

        if (ch == '}' || ch == ']')
        {
            GltfJsonType expected = ch == '}' ? GLTF_JSON_OBJECT
                                               : GLTF_JSON_ARRAY;

            if (parent < 0 || tokens[parent].type != expected)
            {
                *reasonout = "the glTF JSON has mismatched containers.";
                goto fail;
            }
            tokens[parent].end = position + 1;
            parent = tokens[parent].parent;
            position++;
            continue;
        }

        if (ch == '"')
        {
            size_t start = ++position;

            while (position < length && json[position] != '"')
            {
                if (json[position] == '\\')
                {
                    position++;
                    if (position >= length)
                    {
                        break;
                    }
                }
                position++;
            }

            if (position >= length)
            {
                *reasonout = "the glTF JSON contains an unfinished string.";
                goto fail;
            }

            if (!GltfJsonPushToken(&tokens, &count, &capacity,
                                   GLTF_JSON_STRING, start, position,
                                   parent))
            {
                goto out_of_memory;
            }
            position++;
            continue;
        }

        {
            size_t start = position;

            while (position < length
                && !isspace((unsigned char)json[position])
                && json[position] != ',' && json[position] != ']'
                && json[position] != '}')
            {
                position++;
            }

            if (position == start
                || !GltfJsonPushToken(&tokens, &count, &capacity,
                                      GLTF_JSON_PRIMITIVE, start, position,
                                      parent))
            {
                if (position == start)
                {
                    *reasonout = "the glTF JSON contains an invalid token.";
                    goto fail;
                }
                goto out_of_memory;
            }
        }
    }

    if (parent >= 0 || count == 0 || tokens[0].type != GLTF_JSON_OBJECT)
    {
        *reasonout = "the glTF JSON has an incomplete root object.";
        goto fail;
    }

    *tokensout = tokens;
    *countout = count;
    return TRUE;

out_of_memory:
    *reasonout = "out of memory parsing the glTF JSON.";
fail:
    free(tokens);
    return FALSE;
}


static int GltfJsonNext(const GltfJsonToken *tokens, int count, int index)
{
    size_t end = tokens[index].end;

    index++;
    while (index < count && tokens[index].start < end)
    {
        index++;
    }
    return index;
}


static BOOL GltfJsonTokenEquals(const char *json,
                                const GltfJsonToken *token,
                                const char *text)
{
    size_t length = token->end - token->start;

    return token->type == GLTF_JSON_STRING
        && strlen(text) == length
        && memcmp(json + token->start, text, length) == 0;
}


static int GltfJsonObjectGet(const char *json,
                             const GltfJsonToken *tokens, int count,
                             int object, const char *key)
{
    int index;

    if (object < 0 || object >= count
        || tokens[object].type != GLTF_JSON_OBJECT)
    {
        return -1;
    }

    index = object + 1;
    while (index + 1 < count && tokens[index].start < tokens[object].end)
    {
        int value = index + 1;

        if (tokens[index].parent != object
            || tokens[index].type != GLTF_JSON_STRING
            || tokens[value].parent != object)
        {
            index++;
            continue;
        }

        if (GltfJsonTokenEquals(json, &tokens[index], key))
        {
            return value;
        }
        index = GltfJsonNext(tokens, count, value);
    }

    return -1;
}


static int GltfJsonArrayGet(const GltfJsonToken *tokens, int count,
                            int array, DWORD wanted)
{
    DWORD found = 0;
    int index;

    if (array < 0 || array >= count
        || tokens[array].type != GLTF_JSON_ARRAY)
    {
        return -1;
    }

    index = array + 1;
    while (index < count && tokens[index].start < tokens[array].end)
    {
        if (tokens[index].parent == array)
        {
            if (found == wanted)
            {
                return index;
            }
            found++;
            index = GltfJsonNext(tokens, count, index);
        }
        else
        {
            index++;
        }
    }

    return -1;
}


static DWORD GltfJsonArrayCount(const GltfJsonToken *tokens, int count,
                                int array)
{
    DWORD result = 0;

    while (GltfJsonArrayGet(tokens, count, array, result) >= 0)
    {
        result++;
    }
    return result;
}


static BOOL GltfJsonUnsigned(const char *json,
                             const GltfJsonToken *token, DWORD *out)
{
    char text[32];
    char *end;
    unsigned long long value;
    size_t length;

    if (token->type != GLTF_JSON_PRIMITIVE)
    {
        return FALSE;
    }

    length = token->end - token->start;
    if (length == 0 || length >= sizeof(text)
        || json[token->start] == '-')
    {
        return FALSE;
    }

    memcpy(text, json + token->start, length);
    text[length] = '\0';
    value = strtoull(text, &end, 10);
    if (*end != '\0' || value > 0xffffffffull)
    {
        return FALSE;
    }

    *out = (DWORD)value;
    return TRUE;
}


static BOOL GltfJsonBool(const char *json, const GltfJsonToken *token,
                         BOOL *out)
{
    size_t length = token->end - token->start;

    if (token->type != GLTF_JSON_PRIMITIVE)
    {
        return FALSE;
    }
    if (length == 4 && memcmp(json + token->start, "true", 4) == 0)
    {
        *out = TRUE;
        return TRUE;
    }
    if (length == 5 && memcmp(json + token->start, "false", 5) == 0)
    {
        *out = FALSE;
        return TRUE;
    }
    return FALSE;
}


static char *GltfJsonCopyString(const char *json,
                                const GltfJsonToken *token)
{
    size_t input;
    size_t output = 0;
    size_t length;
    char *copy;

    if (token->type != GLTF_JSON_STRING)
    {
        return NULL;
    }

    length = token->end - token->start;
    copy = (char *)malloc(length + 1);
    if (copy == NULL)
    {
        return NULL;
    }

    for (input = 0; input < length; input++)
    {
        char ch = json[token->start + input];

        if (ch == '\\')
        {
            if (++input >= length)
            {
                free(copy);
                return NULL;
            }
            ch = json[token->start + input];
            switch (ch)
            {
            case '"': case '\\': case '/': break;
            case 'b': ch = '\b'; break;
            case 'f': ch = '\f'; break;
            case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;
            case 't': ch = '\t'; break;
            default:
                free(copy);
                return NULL;
            }
        }
        copy[output++] = ch;
    }

    copy[output] = '\0';
    return copy;
}


static char *GltfReadTextFile(const char *path, size_t *sizeout)
{
    FILE *file = fopen(path, "rb");
    long length;
    char *data;

    *sizeout = 0;
    if (file == NULL || fseek(file, 0, SEEK_END) != 0)
    {
        if (file != NULL) fclose(file);
        return NULL;
    }

    length = ftell(file);
    if (length <= 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }

    data = (char *)malloc((size_t)length + 1);
    if (data == NULL
        || fread(data, 1, (size_t)length, file) != (size_t)length)
    {
        free(data);
        fclose(file);
        return NULL;
    }

    fclose(file);
    data[length] = '\0';
    *sizeout = (size_t)length;
    return data;
}


static unsigned char *GltfReadBinaryFile(const char *path, size_t *sizeout)
{
    FILE *file = fopen(path, "rb");
    long length;
    unsigned char *data;

    *sizeout = 0;
    if (file == NULL || fseek(file, 0, SEEK_END) != 0)
    {
        if (file != NULL) fclose(file);
        return NULL;
    }

    length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }

    data = (unsigned char *)malloc(length != 0 ? (size_t)length : 1);
    if (data == NULL
        || (length != 0
            && fread(data, 1, (size_t)length, file) != (size_t)length))
    {
        free(data);
        fclose(file);
        return NULL;
    }

    fclose(file);
    *sizeout = (size_t)length;
    return data;
}


static int GltfBase64Value(unsigned char ch)
{
    if (ch >= 'A' && ch <= 'Z') return ch - 'A';
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9') return ch - '0' + 52;
    if (ch == '+') return 62;
    if (ch == '/') return 63;
    return -1;
}


static unsigned char *GltfDecodeBase64(const char *text, size_t length,
                                       size_t *sizeout)
{
    unsigned char *data;
    size_t capacity = length / 4 * 3 + 3;
    size_t output = 0;
    unsigned int accumulator = 0;
    int bits = 0;
    BOOL padding = FALSE;
    size_t index;

    *sizeout = 0;
    data = (unsigned char *)malloc(capacity != 0 ? capacity : 1);
    if (data == NULL)
    {
        return NULL;
    }

    for (index = 0; index < length; index++)
    {
        unsigned char ch = (unsigned char)text[index];
        int value;

        if (isspace(ch))
        {
            continue;
        }
        if (ch == '=')
        {
            padding = TRUE;
            continue;
        }
        if (padding || (value = GltfBase64Value(ch)) < 0)
        {
            free(data);
            return NULL;
        }

        accumulator = (accumulator << 6) | (unsigned int)value;
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            data[output++] = (unsigned char)(accumulator >> bits);
            accumulator &= bits != 0 ? (1u << bits) - 1u : 0u;
        }
    }

    *sizeout = output;
    return data;
}


static unsigned char *GltfLoadBufferUri(const char *gltfpath,
                                        const char *uri, size_t *sizeout)
{
    if (strncmp(uri, "data:", 5) == 0)
    {
        const char *comma = strchr(uri, ',');
        const char *base64 = strstr(uri, ";base64");

        if (comma == NULL || base64 == NULL || base64 > comma)
        {
            return NULL;
        }
        return GltfDecodeBase64(comma + 1, strlen(comma + 1), sizeout);
    }
    else
    {
        const char *slash = strrchr(gltfpath, '/');
        const char *backslash = strrchr(gltfpath, '\\');
        const char *separator = slash;
        size_t directory;
        size_t urilength = strlen(uri);
        char *path;
        unsigned char *data;

        if (backslash != NULL && (separator == NULL || backslash > separator))
        {
            separator = backslash;
        }
        directory = separator != NULL
            ? (size_t)(separator - gltfpath + 1) : 0;

        if (directory > (size_t)-1 - urilength - 1)
        {
            return NULL;
        }
        path = (char *)malloc(directory + urilength + 1);
        if (path == NULL)
        {
            return NULL;
        }
        memcpy(path, gltfpath, directory);
        memcpy(path + directory, uri, urilength + 1);
        data = GltfReadBinaryFile(path, sizeout);
        free(path);
        return data;
    }
}


static void GltfFreeBuffers(GltfBuffer *buffers, DWORD count)
{
    DWORD index;

    for (index = 0; index < count; index++)
    {
        free(buffers[index].data);
    }
    free(buffers);
}


static BOOL GltfLoadBuffers(const char *path, const char *json,
                            const GltfJsonToken *tokens, int tokencount,
                            int root, GltfBuffer **buffersout,
                            DWORD *buffercountout, const char **reasonout)
{
    int array = GltfJsonObjectGet(json, tokens, tokencount,
                                  root, "buffers");
    DWORD count = GltfJsonArrayCount(tokens, tokencount, array);
    GltfBuffer *buffers;
    DWORD index;

    *buffersout = NULL;
    *buffercountout = 0;
    if (count == 0 || count > 64)
    {
        *reasonout = "the glTF contains no usable buffers.";
        return FALSE;
    }

    buffers = (GltfBuffer *)calloc(count, sizeof(*buffers));
    if (buffers == NULL)
    {
        *reasonout = "out of memory loading glTF buffers.";
        return FALSE;
    }

    for (index = 0; index < count; index++)
    {
        int object = GltfJsonArrayGet(tokens, tokencount, array, index);
        int uritoken = GltfJsonObjectGet(json, tokens, tokencount,
                                         object, "uri");
        int lengthtoken = GltfJsonObjectGet(json, tokens, tokencount,
                                            object, "byteLength");
        DWORD declared;
        char *uri;

        if (uritoken < 0 || lengthtoken < 0
            || !GltfJsonUnsigned(json, &tokens[lengthtoken], &declared)
            || (uri = GltfJsonCopyString(json, &tokens[uritoken])) == NULL)
        {
            *reasonout = "a glTF buffer description is invalid.";
            goto fail;
        }

        buffers[index].data = GltfLoadBufferUri(path, uri,
                                                 &buffers[index].size);
        free(uri);
        if (buffers[index].data == NULL
            || buffers[index].size < (size_t)declared)
        {
            *reasonout = "a glTF binary buffer is missing or incomplete.";
            goto fail;
        }
    }

    *buffersout = buffers;
    *buffercountout = count;
    return TRUE;

fail:
    GltfFreeBuffers(buffers, count);
    return FALSE;
}


static int GltfComponentSize(int componenttype)
{
    switch (componenttype)
    {
    case GLTF_COMPONENT_BYTE:
    case GLTF_COMPONENT_UNSIGNED_BYTE: return 1;
    case GLTF_COMPONENT_SHORT:
    case GLTF_COMPONENT_UNSIGNED_SHORT: return 2;
    case GLTF_COMPONENT_UNSIGNED_INT:
    case GLTF_COMPONENT_FLOAT: return 4;
    }
    return 0;
}


static int GltfTypeComponents(const char *json,
                              const GltfJsonToken *token)
{
    if (GltfJsonTokenEquals(json, token, "SCALAR")) return 1;
    if (GltfJsonTokenEquals(json, token, "VEC2")) return 2;
    if (GltfJsonTokenEquals(json, token, "VEC3")) return 3;
    if (GltfJsonTokenEquals(json, token, "VEC4")) return 4;
    return 0;
}


static BOOL GltfResolveAccessor(const char *json,
                                const GltfJsonToken *tokens, int tokencount,
                                int root, DWORD accessorindex,
                                DWORD buffercount, GltfAccessor *out)
{
    int accessors = GltfJsonObjectGet(json, tokens, tokencount,
                                      root, "accessors");
    int views = GltfJsonObjectGet(json, tokens, tokencount,
                                  root, "bufferViews");
    int accessor = GltfJsonArrayGet(tokens, tokencount, accessors,
                                    accessorindex);
    int viewtoken;
    int componenttoken;
    int counttoken;
    int typetoken;
    int offsettoken;
    int normalizedtoken;
    int view;
    int buffertoken;
    int lengthtoken;
    int stridetoken;
    DWORD value;
    int componentsize;
    size_t elementsize;

    ZeroMemory(out, sizeof(*out));
    if (accessor < 0 || tokens[accessor].type != GLTF_JSON_OBJECT)
    {
        return FALSE;
    }

    viewtoken = GltfJsonObjectGet(json, tokens, tokencount,
                                  accessor, "bufferView");
    componenttoken = GltfJsonObjectGet(json, tokens, tokencount,
                                       accessor, "componentType");
    counttoken = GltfJsonObjectGet(json, tokens, tokencount,
                                   accessor, "count");
    typetoken = GltfJsonObjectGet(json, tokens, tokencount,
                                  accessor, "type");
    if (viewtoken < 0 || componenttoken < 0 || counttoken < 0
        || typetoken < 0
        || !GltfJsonUnsigned(json, &tokens[viewtoken], &value))
    {
        return FALSE;
    }

    view = GltfJsonArrayGet(tokens, tokencount, views, value);
    if (view < 0 || tokens[view].type != GLTF_JSON_OBJECT)
    {
        return FALSE;
    }

    buffertoken = GltfJsonObjectGet(json, tokens, tokencount,
                                    view, "buffer");
    lengthtoken = GltfJsonObjectGet(json, tokens, tokencount,
                                    view, "byteLength");
    if (buffertoken < 0 || lengthtoken < 0
        || !GltfJsonUnsigned(json, &tokens[buffertoken], &out->view.buffer)
        || out->view.buffer >= buffercount
        || !GltfJsonUnsigned(json, &tokens[lengthtoken], &value))
    {
        return FALSE;
    }
    out->view.length = value;

    offsettoken = GltfJsonObjectGet(json, tokens, tokencount,
                                    view, "byteOffset");
    if (offsettoken >= 0)
    {
        if (!GltfJsonUnsigned(json, &tokens[offsettoken], &value))
        {
            return FALSE;
        }
        out->view.offset = value;
    }

    stridetoken = GltfJsonObjectGet(json, tokens, tokencount,
                                    view, "byteStride");
    if (stridetoken >= 0)
    {
        if (!GltfJsonUnsigned(json, &tokens[stridetoken], &value))
        {
            return FALSE;
        }
        out->view.stride = value;
    }

    if (!GltfJsonUnsigned(json, &tokens[componenttoken], &value))
    {
        return FALSE;
    }
    out->componenttype = (int)value;
    if (!GltfJsonUnsigned(json, &tokens[counttoken], &out->count))
    {
        return FALSE;
    }
    out->components = GltfTypeComponents(json, &tokens[typetoken]);

    offsettoken = GltfJsonObjectGet(json, tokens, tokencount,
                                    accessor, "byteOffset");
    if (offsettoken >= 0)
    {
        if (!GltfJsonUnsigned(json, &tokens[offsettoken], &value))
        {
            return FALSE;
        }
        out->offset = value;
    }

    normalizedtoken = GltfJsonObjectGet(json, tokens, tokencount,
                                        accessor, "normalized");
    if (normalizedtoken >= 0
        && !GltfJsonBool(json, &tokens[normalizedtoken], &out->normalized))
    {
        return FALSE;
    }

    componentsize = GltfComponentSize(out->componenttype);
    if (componentsize == 0 || out->components == 0)
    {
        return FALSE;
    }
    elementsize = (size_t)componentsize * out->components;
    if (out->view.stride == 0)
    {
        out->view.stride = elementsize;
    }

    return out->view.stride >= elementsize
        && out->offset <= out->view.length;
}


static unsigned short GltfReadU16(const unsigned char *data)
{
    return (unsigned short)((unsigned int)data[0]
                          | ((unsigned int)data[1] << 8));
}


static DWORD GltfReadU32(const unsigned char *data)
{
    return (DWORD)data[0] | ((DWORD)data[1] << 8)
         | ((DWORD)data[2] << 16) | ((DWORD)data[3] << 24);
}


static float GltfReadFloat(const unsigned char *data)
{
    union { DWORD word; float value; } convert;

    convert.word = GltfReadU32(data);
    return convert.value;
}


static const unsigned char *GltfAccessorElement(const GltfAccessor *accessor,
                                                 const GltfBuffer *buffers,
                                                 DWORD index)
{
    const GltfBuffer *buffer;
    size_t componentsize = GltfComponentSize(accessor->componenttype);
    size_t elementsize = componentsize * accessor->components;
    size_t relative;
    size_t absolute;

    if (index >= accessor->count
        || index > ((size_t)-1 - accessor->offset) / accessor->view.stride)
    {
        return NULL;
    }
    relative = accessor->offset + (size_t)index * accessor->view.stride;
    if (relative > accessor->view.length
        || elementsize > accessor->view.length - relative
        || accessor->view.offset > (size_t)-1 - relative)
    {
        return NULL;
    }

    absolute = accessor->view.offset + relative;
    buffer = &buffers[accessor->view.buffer];
    if (absolute > buffer->size || elementsize > buffer->size - absolute)
    {
        return NULL;
    }
    return buffer->data + absolute;
}


static BOOL GltfAccessorFloats(const GltfAccessor *accessor,
                               const GltfBuffer *buffers, DWORD index,
                               float *out, int wanted)
{
    const unsigned char *data = GltfAccessorElement(accessor, buffers, index);
    int componentsize = GltfComponentSize(accessor->componenttype);
    int component;

    if (data == NULL || accessor->components < wanted)
    {
        return FALSE;
    }

    for (component = 0; component < wanted; component++)
    {
        const unsigned char *value = data + component * componentsize;

        switch (accessor->componenttype)
        {
        case GLTF_COMPONENT_FLOAT:
            out[component] = GltfReadFloat(value);
            break;
        case GLTF_COMPONENT_UNSIGNED_BYTE:
            out[component] = accessor->normalized
                ? value[0] / 255.0f : (float)value[0];
            break;
        case GLTF_COMPONENT_BYTE:
        {
            signed char signedvalue = (signed char)value[0];
            out[component] = accessor->normalized
                ? (signedvalue == -128 ? -1.0f : signedvalue / 127.0f)
                : (float)signedvalue;
            break;
        }
        case GLTF_COMPONENT_UNSIGNED_SHORT:
        {
            unsigned short shortvalue = GltfReadU16(value);
            out[component] = accessor->normalized
                ? shortvalue / 65535.0f : (float)shortvalue;
            break;
        }
        case GLTF_COMPONENT_SHORT:
        {
            short signedvalue = (short)GltfReadU16(value);
            out[component] = accessor->normalized
                ? (signedvalue == -32768 ? -1.0f : signedvalue / 32767.0f)
                : (float)signedvalue;
            break;
        }
        default:
            return FALSE;
        }
    }
    return TRUE;
}


static BOOL GltfAccessorIndex(const GltfAccessor *accessor,
                              const GltfBuffer *buffers, DWORD index,
                              DWORD *out)
{
    const unsigned char *data = GltfAccessorElement(accessor, buffers, index);

    if (data == NULL || accessor->components != 1)
    {
        return FALSE;
    }
    switch (accessor->componenttype)
    {
    case GLTF_COMPONENT_UNSIGNED_BYTE: *out = data[0]; return TRUE;
    case GLTF_COMPONENT_UNSIGNED_SHORT: *out = GltfReadU16(data); return TRUE;
    case GLTF_COMPONENT_UNSIGNED_INT: *out = GltfReadU32(data); return TRUE;
    }
    return FALSE;
}


static unsigned char GltfColorByte(float value)
{
    if (value <= 0.0f) return 0;
    if (value >= 1.0f) return 255;
    return (unsigned char)(value * 255.0f + 0.5f);
}


static BOOL GltfBuilderReserve(GltfBuilder *builder, DWORD add)
{
    DWORD needed;
    DWORD capacity;
    BgVertex *vertices;
    unsigned short *tags;

    if (add > GLTF_MAX_FACES - builder->tricount)
    {
        return FALSE;
    }
    needed = builder->tricount + add;
    if (needed <= builder->capacity)
    {
        return TRUE;
    }

    capacity = builder->capacity != 0 ? builder->capacity : 1024;
    while (capacity < needed)
    {
        if (capacity > GLTF_MAX_FACES / 2)
        {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }

    vertices = (BgVertex *)realloc(builder->vertices,
        (size_t)capacity * 3 * sizeof(*vertices));
    if (vertices == NULL)
    {
        return FALSE;
    }
    builder->vertices = vertices;

    tags = (unsigned short *)realloc(builder->tags,
        (size_t)capacity * sizeof(*tags));
    if (tags == NULL)
    {
        return FALSE;
    }
    builder->tags = tags;
    builder->capacity = capacity;
    return TRUE;
}


static BOOL GltfPrimitiveTag(const char *json,
                             const GltfJsonToken *tokens, int tokencount,
                             int root, int primitive,
                             unsigned short *tagout)
{
    int extras = GltfJsonObjectGet(json, tokens, tokencount,
                                   primitive, "extras");
    int tagtoken = GltfJsonObjectGet(json, tokens, tokencount,
                                     extras, "goldeneyeTextureTag");
    DWORD tag;

    *tagout = BG_TEX_NONE;
    if (tagtoken >= 0)
    {
        if (!GltfJsonUnsigned(json, &tokens[tagtoken], &tag)
            || tag > 0xffffu)
        {
            return FALSE;
        }
        *tagout = (unsigned short)tag;
        return TRUE;
    }

    {
        int materialtoken = GltfJsonObjectGet(json, tokens, tokencount,
                                              primitive, "material");

        if (materialtoken >= 0
            && GltfJsonUnsigned(json, &tokens[materialtoken], &tag))
        {
            int materials = GltfJsonObjectGet(json, tokens, tokencount,
                                              root, "materials");
            int material = GltfJsonArrayGet(tokens, tokencount,
                                             materials, tag);
            int name;

            extras = GltfJsonObjectGet(json, tokens, tokencount,
                                       material, "extras");
            tagtoken = GltfJsonObjectGet(json, tokens, tokencount,
                                         extras, "goldeneyeTextureTag");
            if (tagtoken >= 0)
            {
                if (!GltfJsonUnsigned(json, &tokens[tagtoken], &tag)
                    || tag > 0xffffu)
                {
                    return FALSE;
                }
                *tagout = (unsigned short)tag;
                return TRUE;
            }

            name = GltfJsonObjectGet(json, tokens, tokencount,
                                     material, "name");
            if (name >= 0 && tokens[name].type == GLTF_JSON_STRING)
            {
                char *copy = GltfJsonCopyString(json, &tokens[name]);
                unsigned int parsed;

                if (copy != NULL
                    && sscanf(copy, "GUD Texture Tag 0x%x", &parsed) == 1
                    && parsed <= 0xffffu)
                {
                    *tagout = (unsigned short)parsed;
                }
                free(copy);
            }
        }
    }

    return TRUE;
}


static BOOL GltfUsesNormalizedUvs(const char *json,
                                  const GltfJsonToken *tokens,
                                  int tokencount, int root)
{
    int extras = GltfJsonObjectGet(json, tokens, tokencount,
                                   root, "extras");
    int units = GltfJsonObjectGet(json, tokens, tokencount,
                                  extras, "goldeneyeUvUnits");
    int asset;
    int generator;

    if (units >= 0)
    {
        return GltfJsonTokenEquals(json, &tokens[units], "normalized");
    }

    asset = GltfJsonObjectGet(json, tokens, tokencount, root, "asset");
    extras = GltfJsonObjectGet(json, tokens, tokencount,
                               asset, "extras");
    units = GltfJsonObjectGet(json, tokens, tokencount,
                              extras, "goldeneyeUvUnits");
    if (units >= 0)
    {
        return GltfJsonTokenEquals(json, &tokens[units], "normalized");
    }

    /* The first GEditor glTF exporter wrote texel-space values directly.
       Other glTF producers use normalized texture coordinates by default. */
    generator = GltfJsonObjectGet(json, tokens, tokencount,
                                  asset, "generator");
    return generator < 0
        || !GltfJsonTokenEquals(json, &tokens[generator], "GEditor");
}


static BOOL GltfReadTextureSize(const char *json,
                                const GltfJsonToken *tokens,
                                int tokencount, int extras,
                                int *widthout, int *heightout)
{
    int size = GltfJsonObjectGet(json, tokens, tokencount,
                                 extras, "goldeneyeTextureSize");
    int width = GltfJsonArrayGet(tokens, tokencount, size, 0);
    int height = GltfJsonArrayGet(tokens, tokencount, size, 1);
    DWORD w;
    DWORD h;

    if (width < 0 || height < 0
        || !GltfJsonUnsigned(json, &tokens[width], &w)
        || !GltfJsonUnsigned(json, &tokens[height], &h)
        || w == 0 || h == 0 || w > 0xffffu || h > 0xffffu)
    {
        return FALSE;
    }

    *widthout = (int)w;
    *heightout = (int)h;
    return TRUE;
}


static void GltfPrimitiveTextureSize(const char *json,
                                     const GltfJsonToken *tokens,
                                     int tokencount, int root,
                                     int primitive,
                                     int *widthout, int *heightout)
{
    int extras = GltfJsonObjectGet(json, tokens, tokencount,
                                   primitive, "extras");
    DWORD materialindex;
    int materialtoken;
    int materials;
    int material;

    *widthout = 1;
    *heightout = 1;
    if (GltfReadTextureSize(json, tokens, tokencount, extras,
                            widthout, heightout))
    {
        return;
    }

    materialtoken = GltfJsonObjectGet(json, tokens, tokencount,
                                      primitive, "material");
    if (materialtoken < 0
        || !GltfJsonUnsigned(json, &tokens[materialtoken],
                             &materialindex))
    {
        return;
    }

    materials = GltfJsonObjectGet(json, tokens, tokencount,
                                  root, "materials");
    material = GltfJsonArrayGet(tokens, tokencount,
                                materials, materialindex);
    extras = GltfJsonObjectGet(json, tokens, tokencount,
                               material, "extras");
    GltfReadTextureSize(json, tokens, tokencount, extras,
                        widthout, heightout);
}


static BOOL GltfLoadPrimitive(const char *json,
                              const GltfJsonToken *tokens, int tokencount,
                              int root, int primitive,
                              const GltfBuffer *buffers, DWORD buffercount,
                              const char *projectdir, BOOL normalizeduvs,
                              GltfBuilder *builder, const char **reasonout)
{
    int attributes;
    int positiontoken;
    int colortoken;
    int texcoordtoken;
    int indicestoken;
    int modetoken;
    DWORD accessorindex;
    DWORD elementcount;
    DWORD trianglecount;
    DWORD outputindex;
    GltfAccessor positions;
    GltfAccessor colors;
    GltfAccessor texcoords;
    GltfAccessor indices;
    BOOL hascolors = FALSE;
    BOOL hastexcoords = FALSE;
    BOOL hasindices = FALSE;
    unsigned short tag;
    int texturewidth = 1;
    int textureheight = 1;

    if (primitive < 0 || tokens[primitive].type != GLTF_JSON_OBJECT)
    {
        *reasonout = "a glTF mesh primitive is invalid.";
        return FALSE;
    }

    modetoken = GltfJsonObjectGet(json, tokens, tokencount,
                                  primitive, "mode");
    if (modetoken >= 0)
    {
        DWORD mode;

        if (!GltfJsonUnsigned(json, &tokens[modetoken], &mode))
        {
            *reasonout = "a glTF primitive has an invalid drawing mode.";
            return FALSE;
        }
        if (mode != GLTF_MODE_TRIANGLES)
        {
            return TRUE; /* Points, lines, strips, and fans are not BG faces. */
        }
    }

    attributes = GltfJsonObjectGet(json, tokens, tokencount,
                                   primitive, "attributes");
    positiontoken = GltfJsonObjectGet(json, tokens, tokencount,
                                      attributes, "POSITION");
    if (positiontoken < 0
        || !GltfJsonUnsigned(json, &tokens[positiontoken], &accessorindex)
        || !GltfResolveAccessor(json, tokens, tokencount, root,
                                accessorindex, buffercount, &positions)
        || positions.componenttype != GLTF_COMPONENT_FLOAT
        || positions.components != 3)
    {
        *reasonout = "a glTF triangle primitive has no valid POSITION accessor.";
        return FALSE;
    }

    colortoken = GltfJsonObjectGet(json, tokens, tokencount,
                                   attributes, "COLOR_0");
    if (colortoken >= 0)
    {
        if (!GltfJsonUnsigned(json, &tokens[colortoken], &accessorindex)
            || !GltfResolveAccessor(json, tokens, tokencount, root,
                                    accessorindex, buffercount, &colors)
            || (colors.components != 3 && colors.components != 4))
        {
            *reasonout = "a glTF COLOR_0 accessor is invalid.";
            return FALSE;
        }
        hascolors = TRUE;
    }

    texcoordtoken = GltfJsonObjectGet(json, tokens, tokencount,
                                      attributes, "TEXCOORD_0");
    if (texcoordtoken >= 0)
    {
        if (!GltfJsonUnsigned(json, &tokens[texcoordtoken], &accessorindex)
            || !GltfResolveAccessor(json, tokens, tokencount, root,
                                    accessorindex, buffercount, &texcoords)
            || texcoords.components != 2)
        {
            *reasonout = "a glTF TEXCOORD_0 accessor is invalid.";
            return FALSE;
        }
        hastexcoords = TRUE;
    }

    indicestoken = GltfJsonObjectGet(json, tokens, tokencount,
                                     primitive, "indices");
    if (indicestoken >= 0)
    {
        if (!GltfJsonUnsigned(json, &tokens[indicestoken], &accessorindex)
            || !GltfResolveAccessor(json, tokens, tokencount, root,
                                    accessorindex, buffercount, &indices)
            || indices.components != 1)
        {
            *reasonout = "a glTF triangle index accessor is invalid.";
            return FALSE;
        }
        hasindices = TRUE;
        elementcount = indices.count;
    }
    else
    {
        elementcount = positions.count;
    }

    if (elementcount == 0 || elementcount % 3 != 0)
    {
        *reasonout = "a glTF triangle primitive has an incomplete face.";
        return FALSE;
    }
    trianglecount = elementcount / 3;
    if (!GltfBuilderReserve(builder, trianglecount))
    {
        *reasonout = "the glTF contains too many triangles or could not be allocated.";
        return FALSE;
    }
    if (!GltfPrimitiveTag(json, tokens, tokencount, root,
                          primitive, &tag))
    {
        *reasonout = "a glTF primitive has an invalid GoldenEye texture tag.";
        return FALSE;
    }

    if (normalizeduvs && hastexcoords)
    {
        GltfPrimitiveTextureSize(json, tokens, tokencount, root,
                                 primitive, &texturewidth, &textureheight);

        if (projectdir != NULL && BG_TEX_ID(tag) != BG_TEX_NONE)
        {
            int projectwidth;
            int projectheight;

            if (TexGetProjectImageSize(projectdir, BG_TEX_ID(tag),
                                       &projectwidth, &projectheight))
            {
                texturewidth = projectwidth;
                textureheight = projectheight;
            }
        }
    }

    for (outputindex = 0; outputindex < elementcount; outputindex++)
    {
        DWORD sourceindex = outputindex;
        DWORD destination = builder->tricount * 3 + outputindex;
        BgVertex *vertex = &builder->vertices[destination];
        float values[4];

        if ((hasindices
             && !GltfAccessorIndex(&indices, buffers, outputindex,
                                   &sourceindex))
            || !GltfAccessorFloats(&positions, buffers, sourceindex,
                                   values, 3))
        {
            *reasonout = "a glTF triangle references vertex data outside its buffer.";
            return FALSE;
        }

        ZeroMemory(vertex, sizeof(*vertex));
        vertex->x = values[0];
        vertex->y = values[1];
        vertex->z = values[2];
        vertex->r = vertex->g = vertex->b = vertex->a = 255;

        if (hastexcoords)
        {
            if (!GltfAccessorFloats(&texcoords, buffers, sourceindex,
                                    values, 2))
            {
                *reasonout = "a glTF triangle references invalid texture coordinates.";
                return FALSE;
            }
            vertex->s = values[0] * (float)texturewidth;
            vertex->t = values[1] * (float)textureheight;
        }

        if (hascolors)
        {
            values[3] = 1.0f;
            if (!GltfAccessorFloats(&colors, buffers, sourceindex,
                                    values, colors.components))
            {
                *reasonout = "a glTF triangle references invalid vertex colors.";
                return FALSE;
            }
            vertex->r = GltfColorByte(values[0]);
            vertex->g = GltfColorByte(values[1]);
            vertex->b = GltfColorByte(values[2]);
            vertex->a = GltfColorByte(values[3]);
        }
    }

    for (outputindex = 0; outputindex < trianglecount; outputindex++)
    {
        builder->tags[builder->tricount + outputindex] = tag;
    }
    builder->tricount += trianglecount;
    return TRUE;
}


BgVertex *GltfLoadModel(const char *path, const char *projectdir,
                        DWORD *tricount,
                        unsigned short **tritags,
                        const char **reasonout)
{
    char *json = NULL;
    size_t jsonsize;
    GltfJsonToken *tokens = NULL;
    int tokencount = 0;
    GltfBuffer *buffers = NULL;
    DWORD buffercount = 0;
    GltfBuilder builder;
    int asset;
    int version;
    int meshes;
    DWORD meshcount;
    DWORD meshindex;
    BOOL normalizeduvs;

    ZeroMemory(&builder, sizeof(builder));
    *tricount = 0;
    *tritags = NULL;
    *reasonout = "";

    json = GltfReadTextFile(path, &jsonsize);
    if (json == NULL)
    {
        *reasonout = "the object's glTF model file is missing or unreadable.";
        goto fail;
    }
    if (!GltfJsonParse(json, jsonsize, &tokens, &tokencount, reasonout))
    {
        goto fail;
    }

    asset = GltfJsonObjectGet(json, tokens, tokencount, 0, "asset");
    version = GltfJsonObjectGet(json, tokens, tokencount,
                                asset, "version");
    if (version < 0 || tokens[version].type != GLTF_JSON_STRING
        || tokens[version].end == tokens[version].start
        || json[tokens[version].start] != '2')
    {
        *reasonout = "the object model is not a glTF 2.0 file.";
        goto fail;
    }

    if (!GltfLoadBuffers(path, json, tokens, tokencount, 0,
                         &buffers, &buffercount, reasonout))
    {
        goto fail;
    }

    normalizeduvs = GltfUsesNormalizedUvs(json, tokens, tokencount, 0);

    meshes = GltfJsonObjectGet(json, tokens, tokencount, 0, "meshes");
    meshcount = GltfJsonArrayCount(tokens, tokencount, meshes);
    for (meshindex = 0; meshindex < meshcount; meshindex++)
    {
        int mesh = GltfJsonArrayGet(tokens, tokencount, meshes, meshindex);
        int primitives = GltfJsonObjectGet(json, tokens, tokencount,
                                           mesh, "primitives");
        DWORD primitivecount = GltfJsonArrayCount(tokens, tokencount,
                                                  primitives);
        DWORD primitiveindex;

        for (primitiveindex = 0; primitiveindex < primitivecount;
             primitiveindex++)
        {
            int primitive = GltfJsonArrayGet(tokens, tokencount,
                                             primitives, primitiveindex);

            if (!GltfLoadPrimitive(json, tokens, tokencount, 0, primitive,
                                   buffers, buffercount, projectdir,
                                   normalizeduvs, &builder, reasonout))
            {
                goto fail;
            }
        }
    }

    if (builder.tricount == 0)
    {
        *reasonout = "the glTF object model contains no triangle primitives.";
        goto fail;
    }

    free(json);
    free(tokens);
    GltfFreeBuffers(buffers, buffercount);
    *tricount = builder.tricount;
    *tritags = builder.tags;
    return builder.vertices;

fail:
    free(json);
    free(tokens);
    GltfFreeBuffers(buffers, buffercount);
    free(builder.vertices);
    free(builder.tags);
    return NULL;
}


/* Embedded UI meshes use GLB's BIN chunk and the selected scene's node
 * hierarchy. This path is separate from the flattened project model reader. */
static BOOL GltfNodeArray(const char *json, const GltfJsonToken *tokens,
                          int tokencount, int node, const char *name,
                          double *values, int count)
{
    int array = GltfJsonObjectGet(json, tokens, tokencount, node, name);
    int i;
    if (array < 0) { return TRUE; }
    if (GltfJsonArrayCount(tokens, tokencount, array) != (DWORD)count) { return FALSE; }
    for (i = 0; i < count; i++)
    {
        int token = GltfJsonArrayGet(tokens, tokencount, array, i);
        char *end;
        values[i] = strtod(json + tokens[token].start, &end);
        if (end != json + tokens[token].end || !isfinite(values[i])) { return FALSE; }
    }
    return TRUE;
}

static BOOL GltfLoadGlbNode(const char *json, const GltfJsonToken *tokens,
    int tokencount, DWORD nodeindex, const GltfBuffer *buffer,
    const double parent[16], GltfBuilder *builder, int depth, int *visited,
    const char **reasonout)
{
    int nodes = GltfJsonObjectGet(json, tokens, tokencount, 0, "nodes");
    int node = GltfJsonArrayGet(tokens, tokencount, nodes, nodeindex);
    int mesh, children, token, axis, row, col, k;
    DWORD index, i, first = builder->tricount;
    double local[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}, world[16];
    double t[3] = {0,0,0}, s[3] = {1,1,1}, q[4] = {0,0,0,1};
    if (node < 0 || depth > 64 || ++*visited > 4096) { return FALSE; }
    token = GltfJsonObjectGet(json, tokens, tokencount, node, "matrix");
    if (token >= 0)
    {
        if (!GltfNodeArray(json,tokens,tokencount,node,"matrix",local,16)) { return FALSE; }
    }
    else
    {
        double x, y, z, w, length;
        if (!GltfNodeArray(json,tokens,tokencount,node,"translation",t,3)
            || !GltfNodeArray(json,tokens,tokencount,node,"scale",s,3)
            || !GltfNodeArray(json,tokens,tokencount,node,"rotation",q,4)) { return FALSE; }
        length = sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
        if (!(length > 0)) { return FALSE; }
        x=q[0]/length; y=q[1]/length; z=q[2]/length; w=q[3]/length;
        local[0]=1-2*(y*y+z*z); local[1]=2*(x*y+z*w); local[2]=2*(x*z-y*w);
        local[4]=2*(x*y-z*w); local[5]=1-2*(x*x+z*z); local[6]=2*(y*z+x*w);
        local[8]=2*(x*z+y*w); local[9]=2*(y*z-x*w); local[10]=1-2*(x*x+y*y);
        for (axis=0; axis<3; axis++)
        {
            for (row=0; row<3; row++) { local[axis*4+row] *= s[axis]; }
            local[12+axis]=t[axis];
        }
    }
    for (col=0; col<4; col++) for (row=0; row<4; row++)
    {
        world[col*4+row]=0;
        for (k=0; k<4; k++) { world[col*4+row] += parent[k*4+row]*local[col*4+k]; }
        if (!isfinite(world[col*4+row])) { return FALSE; }
    }
    token = GltfJsonObjectGet(json, tokens, tokencount, node, "mesh");
    if (token >= 0)
    {
        int meshes = GltfJsonObjectGet(json, tokens, tokencount, 0, "meshes");
        int primitives;
        DWORD count;
        if (!GltfJsonUnsigned(json, &tokens[token], &index)) { return FALSE; }
        mesh = GltfJsonArrayGet(tokens, tokencount, meshes, index);
        if (mesh < 0) { return FALSE; }
        primitives = GltfJsonObjectGet(json, tokens, tokencount, mesh, "primitives");
        count = GltfJsonArrayCount(tokens, tokencount, primitives);
        for (i=0; i<count; i++)
        {
            int primitive = GltfJsonArrayGet(tokens,tokencount,primitives,i);
            if (!GltfLoadPrimitive(json,tokens,tokencount,0,primitive,buffer,1,
                                   NULL,FALSE,builder,reasonout)) { return FALSE; }
        }
        for (i=first*3; i<builder->tricount*3; i++)
        {
            BgVertex *v = &builder->vertices[i];
            double p[3] = {v->x,v->y,v->z};
            float result[3];
            for (axis=0; axis<3; axis++)
            {
                double value = world[12+axis];
                for (k=0; k<3; k++) { value += world[k*4+axis]*p[k]; }
                if (!isfinite(value) || fabs(value)>FLT_MAX) { return FALSE; }
                result[axis]=(float)value;
            }
            v->x=result[0]; v->y=result[1]; v->z=result[2];
        }
    }
    children = GltfJsonObjectGet(json,tokens,tokencount,node,"children");
    for (i=0; i<GltfJsonArrayCount(tokens,tokencount,children); i++)
    {
        token=GltfJsonArrayGet(tokens,tokencount,children,i);
        if (!GltfJsonUnsigned(json,&tokens[token],&index)
            || !GltfLoadGlbNode(json,tokens,tokencount,index,buffer,world,
                                builder,depth+1,visited,reasonout)) { return FALSE; }
    }
    return TRUE;
}

BgVertex *GltfLoadGlbMesh(const unsigned char *data, DWORD size,
                         DWORD *tricount, const char **reasonout)
{
    char *json = NULL;
    GltfJsonToken *tokens = NULL;
    GltfBuffer buffer = {NULL,0};
    GltfBuilder builder;
    int tokencount = 0, visited = 0, scenes, scene, nodes, token;
    DWORD offset=12, jsonsize=0, sceneindex=0, i;
    const double identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    ZeroMemory(&builder,sizeof(builder));
    *tricount=0; *reasonout="The embedded GLB mesh is invalid.";
    if (data==NULL || size<20 || GltfReadU32(data)!=0x46546c67
        || GltfReadU32(data+4)!=2 || GltfReadU32(data+8)!=size) { return NULL; }
    while (offset+8<=size)
    {
        DWORD length=GltfReadU32(data+offset), type=GltfReadU32(data+offset+4);
        offset+=8;
        if (length>size-offset || (length&3)) { goto fail; }
        if (type==0x4e4f534a)
        {
            if (json!=NULL) { goto fail; }
            json=(char *)malloc((size_t)length+1);
            if (json==NULL) { goto fail; }
            memcpy(json,data+offset,length); json[length]=0; jsonsize=length;
        }
        else if (type==0x004e4942)
        {
            if (buffer.data!=NULL) { goto fail; }
            buffer.data=(unsigned char *)data+offset; buffer.size=length;
        }
        offset+=length;
    }
    if (offset!=size || json==NULL || buffer.data==NULL
        || !GltfJsonParse(json,jsonsize,&tokens,&tokencount,reasonout)) { goto fail; }
    token=GltfJsonObjectGet(json,tokens,tokencount,0,"scene");
    if (token>=0 && !GltfJsonUnsigned(json,&tokens[token],&sceneindex)) { goto fail; }
    scenes=GltfJsonObjectGet(json,tokens,tokencount,0,"scenes");
    scene=GltfJsonArrayGet(tokens,tokencount,scenes,sceneindex);
    nodes=GltfJsonObjectGet(json,tokens,tokencount,scene,"nodes");
    for (i=0; i<GltfJsonArrayCount(tokens,tokencount,nodes); i++)
    {
        DWORD index;
        token=GltfJsonArrayGet(tokens,tokencount,nodes,i);
        if (!GltfJsonUnsigned(json,&tokens[token],&index)
            || !GltfLoadGlbNode(json,tokens,tokencount,index,&buffer,identity,
                                &builder,0,&visited,reasonout)) { goto fail; }
    }
    if (builder.tricount==0) { goto fail; }
    free(tokens); free(json); free(builder.tags);
    *tricount=builder.tricount; *reasonout="";
    return builder.vertices;
fail:
    free(tokens); free(json); free(builder.vertices); free(builder.tags);
    if (**reasonout=='\0') { *reasonout="The embedded GLB mesh is invalid."; }
    return NULL;
}

static void GltfWriteU32(unsigned char *data, DWORD value)
{
    data[0] = (unsigned char)value;
    data[1] = (unsigned char)(value >> 8);
    data[2] = (unsigned char)(value >> 16);
    data[3] = (unsigned char)(value >> 24);
}


static void GltfWriteFloat(unsigned char *data, float value)
{
    union { float value; DWORD word; } convert;

    convert.value = value;
    GltfWriteU32(data, convert.word);
}


static void GltfPackVertex(unsigned char *data, const BgVertex *vertex,
                           int texturewidth, int textureheight)
{
    GltfWriteFloat(data + 0, vertex->x);
    GltfWriteFloat(data + 4, vertex->y);
    GltfWriteFloat(data + 8, vertex->z);
    GltfWriteFloat(data + 12, vertex->s / (float)texturewidth);
    GltfWriteFloat(data + 16, vertex->t / (float)textureheight);
    data[20] = vertex->r;
    data[21] = vertex->g;
    data[22] = vertex->b;
    data[23] = vertex->a;
}


static BOOL GltfWriteBase64(FILE *file, const unsigned char *data,
                            size_t size)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t index;

    for (index = 0; index < size; index += 3)
    {
        unsigned int word = (unsigned int)data[index] << 16;
        int remaining = (int)(size - index);

        if (remaining > 1) word |= (unsigned int)data[index + 1] << 8;
        if (remaining > 2) word |= data[index + 2];

        if (fputc(alphabet[(word >> 18) & 63], file) == EOF
            || fputc(alphabet[(word >> 12) & 63], file) == EOF
            || fputc(remaining > 1 ? alphabet[(word >> 6) & 63] : '=', file)
               == EOF
            || fputc(remaining > 2 ? alphabet[word & 63] : '=', file) == EOF)
        {
            return FALSE;
        }
    }
    return TRUE;
}


static BOOL GltfWriteJson(const char *path, const unsigned char *binary,
                          DWORD binarysize, const GltfGroup *groups,
                          DWORD groupcount)
{
    FILE *file = fopen(path, "wb");
    DWORD group;
    BOOL ok = TRUE;

    if (file == NULL)
    {
        return FALSE;
    }

    if (fprintf(file,
        "{\n"
        "  \"asset\": {\"version\": \"2.0\", \"generator\": \"GEditor\"},\n"
        "  \"extras\": {\"goldeneyeUvUnits\": \"normalized\"},\n"
        "  \"scene\": 0,\n"
        "  \"scenes\": [{\"nodes\": [0]}],\n"
        "  \"nodes\": [{\"mesh\": 0, \"name\": \"GoldenEye model\"}],\n"
        "  \"buffers\": [{\"byteLength\": %lu, \"uri\": \"data:application/octet-stream;base64,",
        (unsigned long)binarysize) < 0
        || !GltfWriteBase64(file, binary, binarysize)
        || fprintf(file, "\"}],\n"
                         "  \"bufferViews\": [{\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": %lu, \"byteStride\": %u, \"target\": 34962}],\n"
                         "  \"accessors\": [\n",
                   (unsigned long)binarysize, GLTF_VERTEX_STRIDE) < 0)
    {
        ok = FALSE;
    }

    for (group = 0; ok && group < groupcount; group++)
    {
        const GltfGroup *item = &groups[group];
        DWORD byteoffset = item->firstvertex * GLTF_VERTEX_STRIDE;
        DWORD vertexcount = item->tricount * 3;
        const char *comma = group + 1 < groupcount ? "," : "";

        if (fprintf(file,
            "    {\"bufferView\": 0, \"byteOffset\": %lu, \"componentType\": 5126, \"count\": %lu, \"type\": \"VEC3\", \"min\": [%.9g, %.9g, %.9g], \"max\": [%.9g, %.9g, %.9g]},\n"
            "    {\"bufferView\": 0, \"byteOffset\": %lu, \"componentType\": 5126, \"count\": %lu, \"type\": \"VEC2\"},\n"
            "    {\"bufferView\": 0, \"byteOffset\": %lu, \"componentType\": 5121, \"normalized\": true, \"count\": %lu, \"type\": \"VEC4\"}%s\n",
            (unsigned long)byteoffset, (unsigned long)vertexcount,
            item->min[0], item->min[1], item->min[2],
            item->max[0], item->max[1], item->max[2],
            (unsigned long)(byteoffset + 12), (unsigned long)vertexcount,
            (unsigned long)(byteoffset + 20), (unsigned long)vertexcount,
            comma) < 0)
        {
            ok = FALSE;
        }
    }

    if (ok && fprintf(file, "  ],\n  \"materials\": [\n") < 0)
    {
        ok = FALSE;
    }
    for (group = 0; ok && group < groupcount; group++)
    {
        const GltfGroup *item = &groups[group];
        const char *alpha = BG_TRI_IS_SECONDARY(item->tag)
            ? ", \"alphaMode\": \"BLEND\"" : "";
        const char *comma = group + 1 < groupcount ? "," : "";

        if (fprintf(file,
            "    {\"name\": \"GUD Texture Tag 0x%04X\", \"doubleSided\": true%s, \"extras\": {\"goldeneyeTextureTag\": %u, \"goldeneyeUvUnits\": \"normalized\", \"goldeneyeTextureSize\": [%d, %d]}}%s\n",
            item->tag, alpha, item->tag,
            item->texturewidth, item->textureheight, comma) < 0)
        {
            ok = FALSE;
        }
    }

    if (ok && fprintf(file, "  ],\n  \"meshes\": [{\"name\": \"GoldenEye model\", \"primitives\": [\n") < 0)
    {
        ok = FALSE;
    }
    for (group = 0; ok && group < groupcount; group++)
    {
        const char *comma = group + 1 < groupcount ? "," : "";

        if (fprintf(file,
            "    {\"attributes\": {\"POSITION\": %lu, \"TEXCOORD_0\": %lu, \"COLOR_0\": %lu}, \"material\": %lu, \"mode\": 4, \"extras\": {\"goldeneyeTextureTag\": %u, \"goldeneyeUvUnits\": \"normalized\", \"goldeneyeTextureSize\": [%d, %d]}}%s\n",
            (unsigned long)(group * 3),
            (unsigned long)(group * 3 + 1),
            (unsigned long)(group * 3 + 2),
            (unsigned long)group, groups[group].tag,
            groups[group].texturewidth, groups[group].textureheight,
            comma) < 0)
        {
            ok = FALSE;
        }
    }

    if (ok && fprintf(file, "  ]}]\n}\n") < 0)
    {
        ok = FALSE;
    }
    if (fclose(file) != 0)
    {
        ok = FALSE;
    }
    if (!ok)
    {
        remove(path);
    }
    return ok;
}


BOOL GltfWriteModel(const char *path, const char *projectdir,
                    const BgVertex *vertices,
                    const unsigned short *tritags, DWORD tricount,
                    const char **reasonout)
{
    GltfGroup *groups = NULL;
    int *groupbytag = NULL;
    unsigned char *binary = NULL;
    DWORD groupcount = 0;
    DWORD firstvertex = 0;
    DWORD binarysize;
    DWORD triangle;
    BOOL ok = FALSE;

    *reasonout = "";
    if (path == NULL || vertices == NULL || tricount == 0
        || tricount > GLTF_MAX_FACES
        || tricount > 0xffffffffu / (3u * GLTF_VERTEX_STRIDE)
        || sizeof(float) != 4)
    {
        *reasonout = "the model geometry cannot be represented as glTF 2.0.";
        return FALSE;
    }

    groups = (GltfGroup *)calloc(tricount, sizeof(*groups));
    groupbytag = (int *)malloc(0x10000u * sizeof(*groupbytag));
    binarysize = tricount * 3u * GLTF_VERTEX_STRIDE;
    binary = (unsigned char *)malloc(binarysize);
    if (groups == NULL || groupbytag == NULL || binary == NULL)
    {
        *reasonout = "out of memory converting a model to glTF.";
        goto done;
    }
    memset(groupbytag, 0xff, 0x10000u * sizeof(*groupbytag));

    for (triangle = 0; triangle < tricount; triangle++)
    {
        unsigned short tag = tritags != NULL
            ? tritags[triangle] : BG_TEX_NONE;
        int group = groupbytag[tag];

        if (group < 0)
        {
            group = (int)groupcount++;
            groupbytag[tag] = group;
            groups[group].tag = tag;
        }
        groups[group].tricount++;
    }

    for (triangle = 0; triangle < groupcount; triangle++)
    {
        DWORD textureid = BG_TEX_ID(groups[triangle].tag);

        groups[triangle].texturewidth = 1;
        groups[triangle].textureheight = 1;
        if (projectdir != NULL && textureid != BG_TEX_NONE)
        {
            TexGetProjectImageSize(projectdir, textureid,
                                   &groups[triangle].texturewidth,
                                   &groups[triangle].textureheight);
        }
        if (groups[triangle].texturewidth <= 0
            || groups[triangle].textureheight <= 0)
        {
            groups[triangle].texturewidth = 1;
            groups[triangle].textureheight = 1;
        }

        groups[triangle].firstvertex = firstvertex;
        firstvertex += groups[triangle].tricount * 3;
    }

    for (triangle = 0; triangle < tricount; triangle++)
    {
        unsigned short tag = tritags != NULL
            ? tritags[triangle] : BG_TEX_NONE;
        GltfGroup *group = &groups[groupbytag[tag]];
        DWORD destination = group->firstvertex + group->written * 3;
        int corner;

        for (corner = 0; corner < 3; corner++)
        {
            const BgVertex *vertex = &vertices[triangle * 3 + corner];
            int axis;

            GltfPackVertex(binary + (destination + corner)
                           * GLTF_VERTEX_STRIDE, vertex,
                           group->texturewidth, group->textureheight);

            for (axis = 0; axis < 3; axis++)
            {
                float value = (&vertex->x)[axis];

                if (group->written == 0 && corner == 0)
                {
                    group->min[axis] = group->max[axis] = value;
                }
                else
                {
                    if (value < group->min[axis]) group->min[axis] = value;
                    if (value > group->max[axis]) group->max[axis] = value;
                }
            }
        }
        group->written++;
    }

    ok = GltfWriteJson(path, binary, binarysize, groups, groupcount);
    if (!ok)
    {
        *reasonout = "the glTF model file could not be completely written.";
    }

done:
    free(groups);
    free(groupbytag);
    free(binary);
    return ok;
}
