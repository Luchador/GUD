#include "matrixmath.h"
#include "math_atan2f.h"
#include <PR/gu.h>
#include <math.h>

/* Avoid Gimble Lock? */
#define EPSILON FLT_EPSILON * 16

f32 g_SavedMtxConversionScale;

f32 g_MtxConversionScale[2] = {M_U16_MAX_VALUE_F, M_U16_MAX_VALUE_F};


void matrix_4x4_set_identity(Mtxf *matrix)
{
    matrix->m[0][0] = 1.0f;
    matrix->m[0][1] = 0.0f;
    matrix->m[0][2] = 0.0f;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][0] = 0.0f;
    matrix->m[1][1] = 1.0f;
    matrix->m[1][2] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][0] = 0.0f;
    matrix->m[2][1] = 0.0f;
    matrix->m[2][2] = 1.0f;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][0] = 0.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][2] = 0.0f;
    matrix->m[3][3] = 1.0f;
}


void matrix_4x4_copy(Mtxf *src, Mtxf *dst)
{
    u32 *srcwords = (u32 *) src;
    u32 *dstwords = (u32 *) dst;

    dstwords[0] = srcwords[0];
    dstwords[1] = srcwords[1];
    dstwords[2] = srcwords[2];
    dstwords[3] = srcwords[3];
    dstwords[4] = srcwords[4];
    dstwords[5] = srcwords[5];
    dstwords[6] = srcwords[6];
    dstwords[7] = srcwords[7];
    dstwords[8] = srcwords[8];
    dstwords[9] = srcwords[9];
    dstwords[10] = srcwords[10];
    dstwords[11] = srcwords[11];
    dstwords[12] = srcwords[12];
    dstwords[13] = srcwords[13];
    dstwords[14] = srcwords[14];
    dstwords[15] = srcwords[15];
}


/**
 * Multiply lhs by rhs and store the result back into rhs.
 */
void matrix_4x4_multiply_in_place(Mtxf *lhs, Mtxf *rhs)
{
    Mtxf result;

    matrix_4x4_multiply(lhs, rhs, &result);
    matrix_4x4_copy(&result, rhs);
}


void matrix_4x4_multiply_homogeneous_in_place(Mtxf *lhs, Mtxf *rhs)
{
    Mtxf result;

    matrix_4x4_multiply_homogeneous(lhs, rhs, &result);
    matrix_4x4_copy(&result, rhs);
}


void matrix_4x4_multiply(Mtxf *lhs, Mtxf *rhs, Mtxf *result)
{
    s32 i;

    for (i = 0; i < 4; i++)
    {
        f32 lhs0 = lhs->m[0][i];
        f32 lhs1 = lhs->m[1][i];
        f32 lhs2 = lhs->m[2][i];
        f32 lhs3 = lhs->m[3][i];

        result->m[0][i] = lhs0 * rhs->m[0][0] + lhs1 * rhs->m[0][1] + lhs2 * rhs->m[0][2] + lhs3 * rhs->m[0][3];
        result->m[1][i] = lhs0 * rhs->m[1][0] + lhs1 * rhs->m[1][1] + lhs2 * rhs->m[1][2] + lhs3 * rhs->m[1][3];
        result->m[2][i] = lhs0 * rhs->m[2][0] + lhs1 * rhs->m[2][1] + lhs2 * rhs->m[2][2] + lhs3 * rhs->m[2][3];
        result->m[3][i] = lhs0 * rhs->m[3][0] + lhs1 * rhs->m[3][1] + lhs2 * rhs->m[3][2] + lhs3 * rhs->m[3][3];
    }
}


void matrix_4x4_multiply_homogeneous(Mtxf *lhs, Mtxf *rhs, Mtxf *result)
{
    s32 i;

    for (i = 0; i < 3; i++)
    {
        f32 lhs0 = lhs->m[0][i];
        f32 lhs1 = lhs->m[1][i];
        f32 lhs2 = lhs->m[2][i];

        result->m[0][i] = lhs0 * rhs->m[0][0] + lhs1 * rhs->m[0][1] + lhs2 * rhs->m[0][2];
        result->m[1][i] = lhs0 * rhs->m[1][0] + lhs1 * rhs->m[1][1] + lhs2 * rhs->m[1][2];
        result->m[2][i] = lhs0 * rhs->m[2][0] + lhs1 * rhs->m[2][1] + lhs2 * rhs->m[2][2];
        result->m[3][i] = lhs0 * rhs->m[3][0] + lhs1 * rhs->m[3][1] + lhs2 * rhs->m[3][2] + lhs->m[3][i];
    }

    result->m[0][3] = 0.0f;
    result->m[1][3] = 0.0f;
    result->m[2][3] = 0.0f;
    result->m[3][3] = 1.0f;
}


void matrix_4x4_rotate_vector(Mtxf *matrix, struct coord3d *vector, struct coord3d *result)
{
    f32 x = vector->f[0];
    f32 y = vector->f[1];
    f32 z = vector->f[2];

    result->f[0] = matrix->m[0][0] * x + matrix->m[1][0] * y + matrix->m[2][0] * z;
    result->f[1] = matrix->m[0][1] * x + matrix->m[1][1] * y + matrix->m[2][1] * z;
    result->f[2] = matrix->m[0][2] * x + matrix->m[1][2] * y + matrix->m[2][2] * z;
}


void mtx4RotateVecInPlace(Mtxf *matrix, struct coord3d *vector)
{
    f32 x = vector->f[0];
    f32 y = vector->f[1];
    f32 z = vector->f[2];
    f32 rotated_x = matrix->m[0][0] * x + matrix->m[1][0] * y + matrix->m[2][0] * z;
    f32 rotated_y = matrix->m[0][1] * x + matrix->m[1][1] * y + matrix->m[2][1] * z;
    f32 rotated_z = matrix->m[0][2] * x + matrix->m[1][2] * y + matrix->m[2][2] * z;

    vector->f[0] = rotated_x;
    vector->f[1] = rotated_y;
    vector->f[2] = rotated_z;
}


void matrix_4x4_transform_vector(Mtxf *matrix, struct coord3d *vector, struct coord3d *result)
{
    f32 x = vector->f[0];
    f32 y = vector->f[1];
    f32 z = vector->f[2];

    result->f[0] = matrix->m[0][0] * x + matrix->m[1][0] * y + matrix->m[2][0] * z + matrix->m[3][0];
    result->f[1] = matrix->m[0][1] * x + matrix->m[1][1] * y + matrix->m[2][1] * z + matrix->m[3][1];
    result->f[2] = matrix->m[0][2] * x + matrix->m[1][2] * y + matrix->m[2][2] * z + matrix->m[3][2];
}


void mtx4TransformVecInPlace(Mtxf *matrix, struct coord3d *vector)
{
    f32 x = vector->f[0];
    f32 y = vector->f[1];
    f32 z = vector->f[2];
    f32 transformed_x = matrix->m[0][0] * x + matrix->m[1][0] * y + matrix->m[2][0] * z;
    f32 transformed_y = matrix->m[0][1] * x + matrix->m[1][1] * y + matrix->m[2][1] * z;
    f32 transformed_z = matrix->m[0][2] * x + matrix->m[1][2] * y + matrix->m[2][2] * z;

    vector->f[0] = transformed_x + matrix->m[3][0];
    vector->f[1] = transformed_y + matrix->m[3][1];
    vector->f[2] = transformed_z + matrix->m[3][2];
}


void matrix_4x4_set_position_and_rotation_around_y(f32 *position, f32 angle, Mtxf *matrix)
{
    f32 cosine      = cosf(angle);
    f32 sine        = sinf(angle);
    matrix->m[0][0] = cosine;
    matrix->m[0][1] = 0.0f;
    matrix->m[0][2] = -sine;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][0] = 0.0f;
    matrix->m[1][1] = 1.0f;
    matrix->m[1][2] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][0] = sine;
    matrix->m[2][1] = 0.0f;
    matrix->m[2][2] = cosine;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][0] = position[0];
    matrix->m[3][1] = position[1];
    matrix->m[3][2] = position[2];
    matrix->m[3][3] = 1.0f;
}


void matrix_4x4_set_rotation_around_x(f32 angle, Mtxf *matrix)
{
    f32 cosine      = cosf(angle);
    f32 sine        = sinf(angle);
    matrix->m[0][0] = 1.0f;
    matrix->m[0][1] = 0.0f;
    matrix->m[0][2] = 0.0f;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][0] = 0.0f;
    matrix->m[1][1] = cosine;
    matrix->m[1][2] = sine;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][0] = 0.0f;
    matrix->m[2][1] = -sine;
    matrix->m[2][2] = cosine;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][0] = 0.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][2] = 0.0f;
    matrix->m[3][3] = 1.0f;
}


void matrix_4x4_set_rotation_around_y(f32 angle, Mtxf *matrix)
{
    f32 cosine      = cosf(angle);
    f32 sine        = sinf(angle);
    matrix->m[0][0] = cosine;
    matrix->m[0][1] = 0.0f;
    matrix->m[0][2] = -sine;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][0] = 0.0f;
    matrix->m[1][1] = 1.0f;
    matrix->m[1][2] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][0] = sine;
    matrix->m[2][1] = 0.0f;
    matrix->m[2][2] = cosine;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][0] = 0.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][2] = 0.0f;
    matrix->m[3][3] = 1.0f;
}


void matrix_4x4_set_rotation_around_z(f32 angle, Mtxf *matrix)
{
    f32 cosine      = cosf(angle);
    f32 sine        = sinf(angle);
    matrix->m[0][0] = cosine;
    matrix->m[0][1] = sine;
    matrix->m[0][2] = 0.0f;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][0] = -sine;
    matrix->m[1][1] = cosine;
    matrix->m[1][2] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][0] = 0.0f;
    matrix->m[2][1] = 0.0f;
    matrix->m[2][2] = 1.0f;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][0] = 0.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][2] = 0.0f;
    matrix->m[3][3] = 1.0f;
}


void matrix_4x4_set_rotation_around_xyz(struct coord3d *angles, Mtxf *matrix)
{
    f32 cos_x       = cosf(angles->f[0]);
    f32 sin_x       = sinf(angles->f[0]);
    f32 cos_y       = cosf(angles->f[1]);
    f32 sin_y       = sinf(angles->f[1]);
    f32 cos_z       = cosf(angles->f[2]);
    f32 sin_z       = sinf(angles->f[2]);
    f32 sin_x_sin_z = sin_x * sin_z;
    f32 cos_x_sin_z = cos_x * sin_z;
    f32 sin_x_cos_z = sin_x * cos_z;
    f32 cos_x_cos_z = cos_x * cos_z;
    matrix->m[0][0] = (cos_y * cos_z);
    matrix->m[0][1] = (cos_y * sin_z);
    matrix->m[0][2] = -sin_y;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][0] = ((sin_x_cos_z * sin_y) - cos_x_sin_z);
    matrix->m[1][1] = ((sin_x_sin_z * sin_y) + cos_x_cos_z);
    matrix->m[1][2] = sin_x * cos_y;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][0] = ((cos_x_cos_z * sin_y) + sin_x_sin_z);
    matrix->m[2][1] = ((cos_x_sin_z * sin_y) - sin_x_cos_z);
    matrix->m[2][2] = cos_x * cos_y;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][0] = 0.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][2] = 0.0f;
    matrix->m[3][3] = 1.0f;
}


void matrix_4x4_get_rotation_around_xyz(Mtxf *matrix, struct coord3d *angles)
{
    f32 norm;
    f32 sin_x_cos_y = matrix->m[1][2];
    f32 cos_x_cos_y = matrix->m[2][2];
    norm            = sqrtf(SQR(sin_x_cos_y) + SQR(cos_x_cos_y));
    if (EPSILON < norm)
    {
        angles->f[0] = atan2f(matrix->m[1][2], matrix->m[2][2]);
        angles->f[1] = atan2f(-matrix->m[0][2], norm);
        angles->f[2] = atan2f(matrix->m[0][1], matrix->m[0][0]);
    }
    else
    {
        angles->f[0] = 0.0f;
        angles->f[1] = atan2f(-matrix->m[0][2], norm);
        angles->f[2] = atan2f(-matrix->m[1][0], matrix->m[1][1]);
    }
}


void matrix_4x4_set_position_and_rotation_around_xyz(struct coord3d *position, struct coord3d * rotation, Mtxf *matrix)
{
    matrix_4x4_set_rotation_around_xyz(rotation, matrix);
    matrix_4x4_set_position(position, matrix);
}


void matrix_4x4_set_identity_and_position(struct coord3d *position, Mtxf *matrix)
{
    matrix->m[0][0] = 1.0f;
    matrix->m[0][1] = 0.0f;
    matrix->m[0][2] = 0.0f;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][0] = 0.0f;
    matrix->m[1][1] = 1.0f;
    matrix->m[1][2] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][0] = 0.0f;
    matrix->m[2][1] = 0.0f;
    matrix->m[2][2] = 1.0f;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][0] = position->f[0];
    matrix->m[3][1] = position->f[1];
    matrix->m[3][2] = position->f[2];
    matrix->m[3][3] = 1.0f;
}


void matrix_4x4_set_position(struct coord3d *position, Mtxf *matrix)
{
    matrix->m[3][0] = position->f[0];
    matrix->m[3][1] = position->f[1];
    matrix->m[3][2] = position->f[2];
}


void matrix_column_1_scalar_multiply(f32 scalar, f32 *matrix)
{
    matrix[0] *= scalar;
    matrix[1] *= scalar;
    matrix[2] *= scalar;
}


void matrix_column_2_scalar_multiply(f32 scalar, f32 *matrix)
{
    matrix[4] *= scalar;
    matrix[5] *= scalar;
    matrix[6] *= scalar;
}


void matrix_column_3_scalar_multiply(f32 scalar, f32 *matrix)
{
    matrix[8] *= scalar;
    matrix[9] *= scalar;
    matrix[10] *= scalar;
    matrix[11] *= scalar;
}


void matrix_column_3_scalar_multiply_2(f32 scalar, f32 *matrix)
{
    matrix[8] *= scalar;
    matrix[9] *= scalar;
    matrix[10] *= scalar;
}


void matrix_scalar_multiply(f32 scalar, f32 *matrix)
{
    matrix[0] *= scalar;
    matrix[1] *= scalar;
    matrix[2] *= scalar;
    matrix[3] *= scalar;
    matrix[4] *= scalar;
    matrix[5] *= scalar;
    matrix[6] *= scalar;
    matrix[7] *= scalar;
    matrix[8] *= scalar;
    matrix[9] *= scalar;
    matrix[10] *= scalar;
    matrix[11] *= scalar;
}


void matrix_scalar_multiply_2(f32 scalar, f32 *matrix)
{
    matrix[0] *= scalar;
    matrix[1] *= scalar;
    matrix[2] *= scalar;
    matrix[4] *= scalar;
    matrix[5] *= scalar;
    matrix[6] *= scalar;
    matrix[8] *= scalar;
    matrix[9] *= scalar;
    matrix[10] *= scalar;
}


void matrix_row_3_scalar_multiply(f32 scalar, f32 *matrix)
{
    matrix[2] *= scalar;
    matrix[6] *= scalar;
    matrix[10] *= scalar;
    matrix[14] *= scalar;
}


void matrix_scalar_multiply_3(f32 scalar, f32 *matrix)
{
    matrix[0] *= scalar;
    matrix[4] *= scalar;
    matrix[8] *= scalar;
    matrix[12] *= scalar;
    matrix[1] *= scalar;
    matrix[5] *= scalar;
    matrix[9] *= scalar;
    matrix[13] *= scalar;
    matrix[2] *= scalar;
    matrix[6] *= scalar;
    matrix[10] *= scalar;
    matrix[14] *= scalar;
}


void matrixSetConversionScale(f32 scale)
{
    g_MtxConversionScale[0] = (M_U16_MAX_VALUE_F * scale);
}


void matrixSuspendConversionScale(void)
{
    g_SavedMtxConversionScale = g_MtxConversionScale[0];
    g_MtxConversionScale[0] = M_U16_MAX_VALUE_F;
}


void matrixRestoreConversionScale(void)
{
    g_MtxConversionScale[0] = g_SavedMtxConversionScale;
}


void matrix_4x4_f32_to_s32(f32 mf[4][4], s32 ms[4][4])
{
    f32 *src = (f32 *)mf;
    s32 *dst = (s32 *)ms;
    f32 scale0 = g_MtxConversionScale[0];
    f32 scale1 = g_MtxConversionScale[1];
    s32 i;

    for (i = 0; i < 8; i += 2)
    {
        s32 e1, e2;

        e1 = (s32)(src[(i << 1) + 0] * scale0);
        e2 = (s32)(src[(i << 1) + 1] * scale0);
        dst[i + 0] = (e1 & 0xffff0000) | (((u32)e2) >> 16);
        dst[i + 8] = (e1 << 16) | (e2 & 0xffff);

        e1 = (s32)(src[(i << 1) + 2] * scale0);
        e2 = (s32)(src[(i << 1) + 3] * scale1);
        dst[i + 1] = (e1 & 0xffff0000) | (((u32)e2) >> 16);
        dst[i + 9] = (e1 << 16) | (e2 & 0xffff);
    }
}


/**
 * Converts a packed RSP fixed-point matrix back to floats. Inverse of matrix_4x4_f32_to_s32.
 * The Mtx format stores the sixteen 16.16 values split. Words 0-7 hold the integer halves
 * (two per word), words 8-15 the fractional halves. Each value is reassembled and divided back to f32.
 */
void matrix_4x4_s32_to_f32(Mtxf *src, Mtxf *dst)
{
    u32 *srcwords = (u32 *) src;
    f32 *dstfloats = (f32 *) dst;
    f32 inverseScale0 = 1.0f / g_MtxConversionScale[0];
    f32 inverseScale1 = 1.0f / g_MtxConversionScale[1];
    s32 i;

    for (i = 0; i < 8; i += 2)
    {
        u32 word1 = srcwords[i + 0];
        u32 word2 = srcwords[i + 8];

        dstfloats[(i << 1) + 0] = (s32) ((word1 & 0xffff0000) | (word2 >> 16)) * inverseScale0;
        dstfloats[(i << 1) + 1] = (s32) ((word1 << 16) | (word2 & 0xffff)) * inverseScale0;

        word1 = srcwords[i + 1];
        word2 = srcwords[i + 9];

        dstfloats[(i << 1) + 2] = (s32) ((word1 & 0xffff0000) | (word2 >> 16)) * inverseScale0;
        dstfloats[(i << 1) + 3] = (s32) ((word1 << 16) | (word2 & 0xffff)) * inverseScale1;
    }
}


/**
 * Re-interleaves a packed RSP matrix (integer halves in words 0-7,
 * fractional halves in 8-15) into element order - one complete 16.16
 * fixed-point value per word. No float conversion and no conversion
 * scale applied; use this when fixed-point elements need to be
 * addressable individually, which the packed layout doesn't allow.
 */
void matrix_4x4_s32_unpack(Mtx *src, Mtx *dst)
{
    u32 *srcwords = (u32 *) src;
    u32 *dstwords = (u32 *) dst;
    s32 i;

    for (i = 0; i < 8; i++) 
    {
        u32 word1 = srcwords[i + 0];
        u32 word2 = srcwords[i + 8];

        dstwords[(i << 1) + 0] = (word1 & 0xffff0000) | (word2 >> 16);
        dstwords[(i << 1) + 1] = (word1 << 16) | (word2 & 0xffff);
    }
}


void matrix_4x4_set_lookat(Mtxf *matrix, f32 eye_x, f32 eye_y, f32 eye_z, f32 forward_x, f32 forward_y, f32 forward_z, f32 up_x, f32 up_y, f32 up_z)
{
    f32 right_x;
    f32 right_y;
    f32 norm_right;
    f32 norm_up;
    f32 right_z;

    // Normalize forward vector
    f32 norm_forward = -1.0f / sqrtf((forward_x * forward_x) + (forward_y * forward_y) + (forward_z * forward_z));
    forward_x *= norm_forward;
    forward_y *= norm_forward;
    forward_z *= norm_forward;

    // Compute right vector (cross product of up and forward)
    right_x = (up_y * forward_z) - (up_z * forward_y);
    right_y = (up_z * forward_x) - (up_x * forward_z);
    right_z = (up_x * forward_y) - (up_y * forward_x);

    // Normalize right vector
    norm_right = 1.0f / sqrtf((right_x * right_x) + (right_y * right_y) + (right_z * right_z));
    right_x *= norm_right;
    right_y *= norm_right;
    right_z *= norm_right;

    // Recompute up vector (cross product of forward and right)
    up_x = (forward_y * right_z) - (forward_z * right_y);
    up_y = (forward_z * right_x) - (forward_x * right_z);
    up_z = (forward_x * right_y) - (forward_y * right_x);

    // Normalize up vector
    norm_up = 1.0f / sqrtf((up_x * up_x) + (up_y * up_y) + (up_z * up_z));
    up_x *= norm_up;
    up_y *= norm_up;
    up_z *= norm_up;

    // Set matrix columns
    matrix->m[0][0] = right_x;
    matrix->m[1][0] = right_y;
    matrix->m[2][0] = right_z;
    matrix->m[3][0] = -((eye_x * right_x) + (eye_y * right_y) + (eye_z * right_z));
    matrix->m[0][1] = up_x;
    matrix->m[1][1] = up_y;
    matrix->m[2][1] = up_z;
    matrix->m[3][1] = -((eye_x * up_x) + (eye_y * up_y) + (eye_z * up_z));
    matrix->m[0][2] = forward_x;
    matrix->m[1][2] = forward_y;
    matrix->m[2][2] = forward_z;
    matrix->m[3][2] = -((eye_x * forward_x) + (eye_y * forward_y) + (eye_z * forward_z));
    matrix->m[0][3] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][3] = 1.0f;
}


void matrix_4x4_set_lookat_target(Mtxf *matrix, f32 eye_x, f32 eye_y, f32 eye_z, f32 target_x, f32 target_y, f32 target_z, f32 up_x, f32 up_y, f32 up_z)
{
    matrix_4x4_set_lookat(matrix, eye_x, eye_y, eye_z, target_x - eye_x, target_y - eye_y, target_z - eye_z, up_x, up_y, up_z);
}


void matrix_4x4_set_basis_and_position(Mtxf *matrix, f32 pos_x, f32 pos_y, f32 pos_z, f32 basis_x, f32 basis_y, f32 basis_z, f32 up_x, f32 up_y, f32 up_z)
{
    f32 right_x;
    f32 right_y;
    f32 norm_right;
    f32 norm_up;
    f32 right_z;
    f32 norm_basis = -1.0f / sqrtf((basis_x * basis_x) + (basis_y * basis_y) + (basis_z * basis_z));

    basis_x *= norm_basis;
    basis_y *= norm_basis;
    basis_z *= norm_basis;
    right_x = (up_y * basis_z) - (up_z * basis_y);
    right_y = (up_z * basis_x) - (up_x * basis_z);
    right_z = (up_x * basis_y) - (up_y * basis_x);
    norm_right = 1.0f / sqrtf((right_x * right_x) + (right_y * right_y) + (right_z * right_z));
    right_x *= norm_right;
    right_y *= norm_right;
    right_z *= norm_right;
    up_x = (basis_y * right_z) - (basis_z * right_y);
    up_y = (basis_z * right_x) - (basis_x * right_z);
    up_z = (basis_x * right_y) - (basis_y * right_x);
    norm_up = 1.0f / sqrtf((up_x * up_x) + (up_y * up_y) + (up_z * up_z));
    up_x *= norm_up;
    up_y *= norm_up;
    up_z *= norm_up;
    matrix->m[0][0] = right_x;
    matrix->m[1][0] = up_x;
    matrix->m[2][0] = basis_x;
    matrix->m[3][0] = pos_x;
    matrix->m[0][1] = right_y;
    matrix->m[1][1] = up_y;
    matrix->m[2][1] = basis_y;
    matrix->m[3][1] = pos_y;
    matrix->m[0][2] = right_z;
    matrix->m[1][2] = up_z;
    matrix->m[2][2] = basis_z;
    matrix->m[3][2] = pos_z;
    matrix->m[0][3] = 0.0f;
    matrix->m[1][3] = 0.0f;
    matrix->m[2][3] = 0.0f;
    matrix->m[3][3] = 1.0f;
}


void matrix_4x4_set_basis_and_position_target(Mtxf *matrix, f32 pos_x, f32 pos_y, f32 pos_z, f32 target_x, f32 target_y, f32 target_z, f32 up_x, f32 up_y, f32 up_z)
{
    matrix_4x4_set_basis_and_position(matrix, pos_x, pos_y, pos_z, target_x - pos_x, target_y - pos_y, target_z - pos_z, up_x, up_y, up_z);
}


u32 matrix_4x4_calc_depth_scale(f32 near, f32 far)
{
    f32 sum = near + far;
    u16 result;

    if (sum <= 2)
    {
        result = 0xffff;
    }
    else
    {
        result = 0x20000 / sum;

        if (result <= 0)
        {
            result = 1;
        }
    }

    return result;
}


void matrix_4x4_set_rotation_axis_angle(Mtxf *matrix, f32 angle, f32 x, f32 y, f32 z) 
{
    f32 sine;
    f32 cosine;
    f32 norm;
    f32 invnorm;
    f32 cos_x;
    f32 sin_x;
    f32 cos_z;
    f32 sin_z;

    guNormalize(&x, &y, &z);
    sine = sinf(angle);
    cosine = cosf(angle);
    norm = sqrtf((x * x) + (z * z));

    if (norm != 0.0f)
    {
        cos_x = x * cosine;
        sin_x = x * sine;
        cos_z = z * cosine;
        sin_z = z * sine;
        invnorm = 1.0f / norm;
        matrix->m[0][0] = ((-cos_z - (y * sin_x)) * invnorm);
        matrix->m[1][0] = (sine * norm);
        matrix->m[2][0] = ((cos_x - (y * sin_z)) * invnorm);
        matrix->m[3][0] = 0.0f;
        matrix->m[0][1] = ((sin_z - (y * cos_x)) * invnorm);
        matrix->m[1][1] = (cosine * norm);
        matrix->m[2][1] = ((-sin_x - (y * cos_z)) * invnorm);
        matrix->m[3][1] = 0.0f;
        matrix->m[0][2] = -x;
        matrix->m[1][2] = -y;
        matrix->m[2][2] = -z;
        matrix->m[3][2] = 0.0f;
        matrix->m[0][3] = 0.0f;
        matrix->m[1][3] = 0.0f;
        matrix->m[2][3] = 0.0f;
        matrix->m[3][3] = 1.0f;
        return;
    }

    matrix_4x4_set_identity(matrix);
}


void matrix_4x4_align(Mtxf *matrix, f32 angle, f32 x, f32 y, f32 z) 
{
    angle = RadToDeg(angle);
    guAlignF(matrix->m, angle, x, y, z);
}


void matrix_4x4_set_rotation_inverse(Mtxf *rotation, Mtxf *transpose) 
{
    transpose->m[0][0] = rotation->m[0][0];
    transpose->m[0][1] = rotation->m[1][0];
    transpose->m[0][2] = rotation->m[2][0];
    transpose->m[1][0] = rotation->m[0][1];
    transpose->m[1][1] = rotation->m[1][1];
    transpose->m[1][2] = rotation->m[2][1];
    transpose->m[2][0] = rotation->m[0][2];
    transpose->m[2][1] = rotation->m[1][2];
    transpose->m[2][2] = rotation->m[2][2];
    transpose->m[3][0] = 0.0f;
    transpose->m[3][1] = 0.0f;
    transpose->m[3][2] = 0.0f;
    transpose->m[0][3] = 0.0f;
    transpose->m[1][3] = 0.0f;
    transpose->m[2][3] = 0.0f;
    transpose->m[3][3] = 1.0f;
}


void matrix_4x4_set_inverse_rotation_and_translation(Mtxf *matrix, Mtxf *result)
{
    f32 norm = (matrix->m[0][0] * matrix->m[0][0]) +
               (matrix->m[1][0] * matrix->m[1][0]) +
               (matrix->m[2][0] * matrix->m[2][0]);

    norm = 1.0f / norm;

    result->m[0][0] = (matrix->m[0][0] * norm);
    result->m[0][1] = (matrix->m[1][0] * norm);
    result->m[0][2] = (matrix->m[2][0] * norm);
    result->m[1][0] = (matrix->m[0][1] * norm);
    result->m[1][1] = (matrix->m[1][1] * norm);
    result->m[1][2] = (matrix->m[2][1] * norm);
    result->m[2][0] = (matrix->m[0][2] * norm);
    result->m[2][1] = (matrix->m[1][2] * norm);
    result->m[2][2] = (matrix->m[2][2] * norm);
    result->m[3][0] = -((result->m[0][0] * matrix->m[3][0]) + (result->m[1][0] * matrix->m[3][1]) + (result->m[2][0] * matrix->m[3][2]));
    result->m[3][1] = -((result->m[0][1] * matrix->m[3][0]) + (result->m[1][1] * matrix->m[3][1]) + (result->m[2][1] * matrix->m[3][2]));
    result->m[3][2] = -((result->m[0][2] * matrix->m[3][0]) + (result->m[1][2] * matrix->m[3][1]) + (result->m[2][2] * matrix->m[3][2]));
    result->m[3][3] = 1.0f;
    result->m[0][3] = 0.0f;
    result->m[1][3] = 0.0f;
    result->m[2][3] = 0.0f;
}


void matrix_4x4_invert_affine(Mtxf *matrix, Mtxf *result)
{
    f32 f0 = 0.0f;
    f0 += matrix->m[0][0] * matrix->m[1][1] * matrix->m[2][2];
    f0 += matrix->m[0][1] * matrix->m[1][2] * matrix->m[2][0];
    f0 += matrix->m[0][2] * matrix->m[1][0] * matrix->m[2][1];
    f0 -= matrix->m[0][2] * matrix->m[1][1] * matrix->m[2][0];
    f0 -= matrix->m[0][1] * matrix->m[1][0] * matrix->m[2][2];
    f0 -= matrix->m[0][0] * matrix->m[1][2] * matrix->m[2][1];
    f0 = 1.0f / f0;

    result->m[0][0] = (matrix->m[1][1] * matrix->m[2][2] - matrix->m[1][2] * matrix->m[2][1]) * f0;
    result->m[1][0] = (matrix->m[1][2] * matrix->m[2][0] - matrix->m[1][0] * matrix->m[2][2]) * f0;
    result->m[2][0] = (matrix->m[1][0] * matrix->m[2][1] - matrix->m[1][1] * matrix->m[2][0]) * f0;
    result->m[0][1] = (matrix->m[0][2] * matrix->m[2][1] - matrix->m[0][1] * matrix->m[2][2]) * f0;
    result->m[1][1] = (matrix->m[0][0] * matrix->m[2][2] - matrix->m[0][2] * matrix->m[2][0]) * f0;
    result->m[2][1] = (matrix->m[0][1] * matrix->m[2][0] - matrix->m[0][0] * matrix->m[2][1]) * f0;
    result->m[0][2] = (matrix->m[0][1] * matrix->m[1][2] - matrix->m[0][2] * matrix->m[1][1]) * f0;
    result->m[1][2] = (matrix->m[0][2] * matrix->m[1][0] - matrix->m[0][0] * matrix->m[1][2]) * f0;
    result->m[2][2] = (matrix->m[0][0] * matrix->m[1][1] - matrix->m[0][1] * matrix->m[1][0]) * f0;
    result->m[3][0] = -(matrix->m[3][0] * result->m[0][0] + matrix->m[3][1] * result->m[1][0] + matrix->m[3][2] * result->m[2][0]);
    result->m[3][1] = -(matrix->m[3][0] * result->m[0][1] + matrix->m[3][1] * result->m[1][1] + matrix->m[3][2] * result->m[2][1]);
    result->m[3][2] = -(matrix->m[3][0] * result->m[0][2] + matrix->m[3][1] * result->m[1][2] + matrix->m[3][2] * result->m[2][2]);
    result->m[0][3] = 0.0f;
    result->m[1][3] = 0.0f;
    result->m[2][3] = 0.0f;
    result->m[3][3] = 1.0f;
}
