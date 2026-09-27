#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "gltfjson.h"

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


BOOL GltfJsonParse(const char *json, size_t length,
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


int GltfJsonNext(const GltfJsonToken *tokens, int count, int index)
{
    size_t end = tokens[index].end;

    index++;
    while (index < count && tokens[index].start < end)
    {
        index++;
    }
    return index;
}


BOOL GltfJsonTokenEquals(const char *json,
                                const GltfJsonToken *token,
                                const char *text)
{
    size_t length = token->end - token->start;

    return token->type == GLTF_JSON_STRING
        && strlen(text) == length
        && memcmp(json + token->start, text, length) == 0;
}


int GltfJsonObjectGet(const char *json,
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


int GltfJsonArrayGet(const GltfJsonToken *tokens, int count,
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


DWORD GltfJsonArrayCount(const GltfJsonToken *tokens, int count,
                                int array)
{
    DWORD result = 0;

    while (GltfJsonArrayGet(tokens, count, array, result) >= 0)
    {
        result++;
    }
    return result;
}


BOOL GltfJsonUnsigned(const char *json,
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


BOOL GltfJsonBool(const char *json, const GltfJsonToken *token,
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


static BOOL GltfJsonHex4(const char *text, size_t length, size_t *index, unsigned *value)
{
    *value=0;
    for (int digit=0;digit<4;digit++)
    {
        if (++*index>=length) { return FALSE; }
        unsigned char ch=(unsigned char)text[*index];
        int hex=ch>='0' && ch<='9' ? ch-'0' : ch>='a' && ch<='f' ? ch-'a'+10 : ch>='A' && ch<='F' ? ch-'A'+10 : -1;
        if (hex<0) { return FALSE; } *value=(*value<<4)|(unsigned)hex;
    }
    return TRUE;
}

char *GltfJsonCopyString(const char *json,
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
            case 'u':
            {
                unsigned code,low; const char *raw=json+token->start;
                if (!GltfJsonHex4(raw,length,&input,&code)) { free(copy); return NULL; }
                if (code>=0xd800 && code<=0xdbff)
                {
                    if (input+2>=length || raw[input+1]!='\\' || raw[input+2]!='u') { free(copy); return NULL; }
                    input+=2;
                    if (!GltfJsonHex4(raw,length,&input,&low) || low<0xdc00 || low>0xdfff) { free(copy); return NULL; }
                    code=0x10000+((code-0xd800)<<10)+(low-0xdc00);
                }
                else if (code>=0xdc00 && code<=0xdfff) { free(copy); return NULL; }
                /* Our filenames/labels are C strings; embedded NUL is invalid. */
                if (!code) { free(copy); return NULL; }
                if (code<0x80) { copy[output++]=(char)code; }
                else if (code<0x800)
                { copy[output++]=(char)(0xc0|(code>>6)); copy[output++]=(char)(0x80|(code&63)); }
                else if (code<0x10000)
                { copy[output++]=(char)(0xe0|(code>>12)); copy[output++]=(char)(0x80|((code>>6)&63)); copy[output++]=(char)(0x80|(code&63)); }
                else
                { copy[output++]=(char)(0xf0|(code>>18)); copy[output++]=(char)(0x80|((code>>12)&63)); copy[output++]=(char)(0x80|((code>>6)&63)); copy[output++]=(char)(0x80|(code&63)); }
                continue;
            }
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


BOOL GltfJsonWriteString(FILE *file,const char *text)
{
    const unsigned char *p=(const unsigned char *)text;
    if (fputc('"',file)==EOF) return FALSE;
    for (;*p;p++)
    {
        if (*p=='"' || *p=='\\') { if (fputc('\\',file)==EOF) return FALSE; }
        if (*p<32) { if (fprintf(file,"\\u%04x",*p)<0) return FALSE; }
        else if (fputc(*p,file)==EOF) return FALSE;
    }
    return fputc('"',file)!=EOF;
}
