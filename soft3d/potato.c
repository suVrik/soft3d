#include "soft3d.h"

#include <math.h>
#include <stdlib.h>

#define BACKBUFFER_WIDTH 800
#define BACKBUFFER_HEIGHT 600

extern DepthColorBuffer backbuffer;
static ColorBuffer texture;

#include "dog_vertex.h"

static VertexBuffer vertex_buffer = { 55668, vertex_data };

#include "dog_texture.h"

static ColorBuffer texture_buffer = { 1024, 1024, (Color*)texture_data };

void potato_init() {
    backbuffer.width = BACKBUFFER_WIDTH;
    backbuffer.height = BACKBUFFER_HEIGHT;
    backbuffer.data = (Color*)calloc(BACKBUFFER_WIDTH * BACKBUFFER_HEIGHT, sizeof(Color));
    backbuffer.depth = (float*)calloc(BACKBUFFER_WIDTH * BACKBUFFER_HEIGHT, sizeof(float));

    // Convert RGBA to BGRA.
    for (size_t i = 0; i < texture_buffer.height; i++) {
        for (size_t j = 0; j < texture_buffer.width; j++) {
            unsigned char temp = texture_data[i * texture_buffer.width * 4 + j * 4];
            texture_data[i * texture_buffer.width * 4 + j * 4] = texture_data[i * texture_buffer.width * 4 + j * 4 + 2];
            texture_data[i * texture_buffer.width * 4 + j * 4 + 2] = temp;
        }
    }

    // Invert V.
    for (size_t i = 0; i < vertex_buffer.length; i++) {
        vertex_buffer.data[i].v = 1.f - vertex_buffer.data[i].v;
    }
}

void build_translation_matrix(Matrix* result, float x, float y, float z) {
    result->data[0] = 1.f;
    result->data[1] = 0.f;
    result->data[2] = 0.f;
    result->data[3] = 0.f;

    result->data[4] = 0.f;
    result->data[5] = 1.f;
    result->data[6] = 0.f;
    result->data[7] = 0.f;

    result->data[8] = 0.f;
    result->data[9] = 0.f;
    result->data[10] = 1.f;
    result->data[11] = 0.f;

    result->data[12] = x;
    result->data[13] = y;
    result->data[14] = z;
    result->data[15] = 1.f;
}

void build_scale_matrix(Matrix* result, float scale_x, float scale_y, float scale_z) {
    result->data[0] = scale_x;
    result->data[1] = 0.f;
    result->data[2] = 0.f;
    result->data[3] = 0.f;

    result->data[4] = 0.f;
    result->data[5] = scale_y;
    result->data[6] = 0.f;
    result->data[7] = 0.f;

    result->data[8] = 0.f;
    result->data[9] = 0.f;
    result->data[10] = scale_z;
    result->data[11] = 0.f;

    result->data[12] = 0.f;
    result->data[13] = 0.f;
    result->data[14] = 0.f;
    result->data[15] = 1.f;
}

void build_rotation_matrix(Matrix* result, float x, float y, float z, float angle) {
    const float angle_cos = (float)cos(-angle);
    const float angle_sin = (float)sin(-angle);

    result->data[0] = angle_cos + (1.f - angle_cos) * x * x;
    result->data[1] = (1.f - angle_cos) * x * y - angle_sin * z;
    result->data[2] = (1.f - angle_cos) * x * z + angle_sin * y;
    result->data[3] = 0.f;

    result->data[4] = (1.f - angle_cos) * y * x + angle_sin * z;
    result->data[5] = angle_cos + (1.f - angle_cos) * y * y;
    result->data[6] = (1.f - angle_cos) * y * z - angle_sin * x;
    result->data[7] = 0.f;

    result->data[8] = (1.f - angle_cos) * z * x - angle_sin * y;
    result->data[9] = (1.f - angle_cos) * z * y + angle_sin * x;
    result->data[10] = angle_cos + (1 - angle_cos) * z * z;
    result->data[11] = 0.f;

    result->data[12] = 0.f;
    result->data[13] = 0.f;
    result->data[14] = 0.f;
    result->data[15] = 1.f;
}

void build_projection_matrix(Matrix* result, float fov, float aspect, float near, float far) {
    const float scale = 1.f / (float)tan(fov / 2.f);

    result->data[0] = scale / aspect;
    result->data[1] = 0.f;
    result->data[2] = 0.f;
    result->data[3] = 0.f;

    result->data[4] = 0.f;
    result->data[5] = scale;
    result->data[6] = 0.f;
    result->data[7] = 0.f;

    result->data[8] = 0.f;
    result->data[9] = 0.f;
    result->data[10] = -far / (far - near);
    result->data[11] = -1.f;

    result->data[12] = 0.f;
    result->data[13] = 0.f;
    result->data[14] = -far * near / (far - near);
    result->data[15] = 0.f;
}

void mul(const Matrix* a, const Matrix* b, Matrix* result) {
    result->data[0] = a->data[0] * b->data[0] + a->data[1] * b->data[4] + a->data[2] * b->data[8] + a->data[3] * b->data[12];
    result->data[1] = a->data[0] * b->data[1] + a->data[1] * b->data[5] + a->data[2] * b->data[9] + a->data[3] * b->data[13];
    result->data[2] = a->data[0] * b->data[2] + a->data[1] * b->data[6] + a->data[2] * b->data[10] + a->data[3] * b->data[14];
    result->data[3] = a->data[0] * b->data[3] + a->data[1] * b->data[7] + a->data[2] * b->data[11] + a->data[3] * b->data[15];
    result->data[4] = a->data[4] * b->data[0] + a->data[5] * b->data[4] + a->data[6] * b->data[8] + a->data[7] * b->data[12];
    result->data[5] = a->data[4] * b->data[1] + a->data[5] * b->data[5] + a->data[6] * b->data[9] + a->data[7] * b->data[13];
    result->data[6] = a->data[4] * b->data[2] + a->data[5] * b->data[6] + a->data[6] * b->data[10] + a->data[7] * b->data[14];
    result->data[7] = a->data[4] * b->data[3] + a->data[5] * b->data[7] + a->data[6] * b->data[11] + a->data[7] * b->data[15];
    result->data[8] = a->data[8] * b->data[0] + a->data[9] * b->data[4] + a->data[10] * b->data[8] + a->data[11] * b->data[12];
    result->data[9] = a->data[8] * b->data[1] + a->data[9] * b->data[5] + a->data[10] * b->data[9] + a->data[11] * b->data[13];
    result->data[10] = a->data[8] * b->data[2] + a->data[9] * b->data[6] + a->data[10] * b->data[10] + a->data[11] * b->data[14];
    result->data[11] = a->data[8] * b->data[3] + a->data[9] * b->data[7] + a->data[10] * b->data[11] + a->data[11] * b->data[15];
    result->data[12] = a->data[12] * b->data[0] + a->data[13] * b->data[4] + a->data[14] * b->data[8] + a->data[15] * b->data[12];
    result->data[13] = a->data[12] * b->data[1] + a->data[13] * b->data[5] + a->data[14] * b->data[9] + a->data[15] * b->data[13];
    result->data[14] = a->data[12] * b->data[2] + a->data[13] * b->data[6] + a->data[14] * b->data[10] + a->data[15] * b->data[14];
    result->data[15] = a->data[12] * b->data[3] + a->data[13] * b->data[7] + a->data[14] * b->data[11] + a->data[15] * b->data[15];
}

void potato_update() {
    for (unsigned int i = 0; i < backbuffer.height; i++) {
        for (unsigned int j = 0; j < backbuffer.width; j++) {
            *(unsigned int*)(backbuffer.data + i * backbuffer.width + j) = 0xFF303030;
            backbuffer.depth[i * backbuffer.width + j] = -1000.f;
        }
    }

    static float angle = 0.f;
    angle += 0.02f;

    Matrix model_rotation = { 0 };
    build_rotation_matrix(&model_rotation, 0.f, 1.f, 0.f, 3.141592653f / 2.f);

    Matrix model_rotation_2 = { 0 };
    build_rotation_matrix(&model_rotation_2, 0.f, 0.f, 1.f, 3.141592653f / 2.f);

    Matrix model_rotation_3 = { 0 };
    mul(&model_rotation, &model_rotation_2, &model_rotation_3);

    Matrix model_rotation_4 = { 0 };
    build_rotation_matrix(&model_rotation_4, 0.f, 1.f, 0.f, angle);

    Matrix model_rotation_5 = { 0 };
    mul(&model_rotation_3, &model_rotation_4, &model_rotation_5);

    Matrix model_scale = { 0 };
    build_scale_matrix(&model_scale, 0.015f, 0.015f, 0.015f);

    Matrix model = { 0 };
    mul(&model_scale, &model_rotation_5, &model);

    Matrix view = { 0 };
    build_translation_matrix(&view, 0.f, -0.25f, 2.f);

    Matrix model_view = { 0 };
    mul(&model, &view, &model_view);

    Matrix projection = { 0 };
    build_projection_matrix(&projection, 0.942478f, (float)BACKBUFFER_WIDTH / BACKBUFFER_HEIGHT, 0.01f, 100.f);

    Matrix model_view_projection = { 0 };
    mul(&model_view, &projection, &model_view_projection);

    rasterize_vertices(&vertex_buffer, &model_view_projection, &texture_buffer, &backbuffer);
}

void potato_destroy() {
    free(backbuffer.data);
    free(backbuffer.depth);
}
