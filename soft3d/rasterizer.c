// soft3d by Andrej Suvorau, 2019

#include "soft3d.h"

#include <assert.h>
#include <stdlib.h>

inline RasterizedVertex convert_vertex(const Vertex* vertex, const Matrix* transform, float screen_w, float screen_h) {
    const float x = vertex->x * transform->data[0] + vertex->y * transform->data[4] + vertex->z * transform->data[8]  + transform->data[12];
    const float y = vertex->x * transform->data[1] + vertex->y * transform->data[5] + vertex->z * transform->data[9]  + transform->data[13];
    const float z = vertex->x * transform->data[2] + vertex->y * transform->data[6] + vertex->z * transform->data[10] + transform->data[14];
    const float w = vertex->x * transform->data[3] + vertex->y * transform->data[7] + vertex->z * transform->data[11] + transform->data[15];

    assert(w != 0.f);

    RasterizedVertex result;
    result.x = (x / w + 0.5f) * screen_w;
    result.y = (y / w + 0.5f) * screen_h;
    result.z = z / w;
    result.u = vertex->u;
    result.v = vertex->v;
    return result;
}

inline void sort_vertices(RasterizedTriangle* triangle) {
    if (triangle->a.y > triangle->b.y) {
        const RasterizedVertex temp = triangle->a;
        triangle->a = triangle->b;
        triangle->b = temp;
    }

    if (triangle->b.y > triangle->c.y) {
        const RasterizedVertex temp = triangle->b;
        triangle->b = triangle->c;
        triangle->c = temp;
    }

    if (triangle->a.y > triangle->b.y) {
        const RasterizedVertex temp = triangle->a;
        triangle->a = triangle->b;
        triangle->b = temp;
    }
}

inline void rasterize_pixel(unsigned int x, unsigned int y, const RasterizedTriangle* triangle,
                            const ColorBuffer* source_buffer, DepthColorBuffer* target_buffer,
                            const Barycentric* barycentric) {
    assert(x >= 0 && x < target_buffer->width && y >= 0 && y < target_buffer->height);

    const float ba = x * barycentric->ax + y * barycentric->ay - barycentric->ac;
    const float bb = x * barycentric->bx + y * barycentric->by - barycentric->bc;
    const float bc = 1.f - ba - bb;

    const float z = triangle->a.z * ba + triangle->b.z * bb + triangle->c.z * bc;
    if (target_buffer->depth[y * target_buffer->width + x] < z) {
        const float u = triangle->a.u * ba + triangle->b.u * bb + triangle->c.u * bc;
        const float v = triangle->a.v * ba + triangle->b.v * bb + triangle->c.v * bc;
        
        const unsigned int du = (unsigned int)(u * source_buffer->width) & (source_buffer->width - 1);
        const unsigned int dv = (unsigned int)(v * source_buffer->height) & (source_buffer->height - 1);

        target_buffer->data[y * target_buffer->width + x] = source_buffer->data[dv * source_buffer->width + du];
        target_buffer->depth[y * target_buffer->width + x] = z;
    }
}

void rasterize_triangle(const RasterizedTriangle* triangle, const ColorBuffer* source_buffer, DepthColorBuffer* target_buffer) {
    assert(triangle->b.y >= triangle->a.y && triangle->c.y >= triangle->b.y);

    const float ax = triangle->a.x;
    const unsigned int ay = (unsigned int)triangle->a.y;

    const float bx = triangle->b.x;
    const unsigned int by = (unsigned int)triangle->b.y;

    const float cx = triangle->c.x;
    const unsigned int cy = (unsigned int)triangle->c.y;

    if (cy > ay) {
        const float det = 1.f / ((triangle->b.y - triangle->c.y) * (ax - cx) + (cx - bx) * (triangle->a.y - triangle->c.y));
        assert(det == det);

        Barycentric barycentric;
        barycentric.ax = (triangle->b.y - triangle->c.y) * det;
        barycentric.ay = (cx - bx) * det;
        barycentric.ac = (triangle->c.y * (cx - bx) + cx * (triangle->b.y - triangle->c.y)) * det;
        barycentric.bx = (triangle->c.y - triangle->a.y) * det;
        barycentric.by = (ax - cx) * det;
        barycentric.bc = (cx * (triangle->c.y - triangle->a.y) + triangle->c.y * (ax - cx)) * det;

        const unsigned int dy_ab = by - ay;
        if (dy_ab > 0) {
            const float dx_ab = (bx - ax) / dy_ab;
            const float dx_ac = (cx - ax) / (cy - ay);

            unsigned int i = 0;
            do {
                unsigned int x_left = (unsigned int)(ax + dx_ab * i);
                unsigned int x_right = (unsigned int)(ax + dx_ac * i);

                if (x_right < x_left) {
                    const float temp = x_left;
                    x_left = x_right;
                    x_right = temp;
                }
                
                const float y = ay + i;
                for (unsigned int x = x_left; x < x_right; x++) {
                    rasterize_pixel(x, y, triangle, source_buffer, target_buffer, &barycentric);
                }
            } while (++i < dy_ab);
        }

        const unsigned int dy_bc = cy - by;
        if (dy_bc > 0) {
            const float mx = ax + (float)(by - ay) / (cy - ay) * (cx - ax);

            const float dx_bc = (cx - bx) / dy_bc;
            const float dx_ec = (cx - mx) / dy_bc;

            unsigned int i = 0;
            do {
                unsigned int x_left = (unsigned int)(bx + dx_bc * i);
                unsigned int x_right = (unsigned int)(mx + dx_ec * i);

                if (x_right < x_left) {
                    const float temp = x_left;
                    x_left = x_right;
                    x_right = temp;
                }

                const float y = by + i;
                for (unsigned int x = x_left; x < x_right; x++) {
                    rasterize_pixel(x, y, triangle, source_buffer, target_buffer, &barycentric);
                }
            } while (++i <= dy_bc);
        }
    }
}

void rasterize_vertices(const VertexBuffer* buffer, const Matrix* transform, const ColorBuffer* source_buffer, DepthColorBuffer* target_buffer) {
    assert(buffer != NULL && target_buffer != NULL && target_buffer->data != NULL && source_buffer != NULL && source_buffer->data != NULL);
    assert(buffer->length % 3 == 0);

    for (size_t i = 0; i < buffer->length; i += 3) {
        RasterizedTriangle triangle = { convert_vertex(buffer->data + i,     transform, (float)target_buffer->width, (float)target_buffer->height),
                                        convert_vertex(buffer->data + i + 1, transform, (float)target_buffer->width, (float)target_buffer->height),
                                        convert_vertex(buffer->data + i + 2, transform, (float)target_buffer->width, (float)target_buffer->height) };

        sort_vertices(&triangle);
        rasterize_triangle(&triangle, source_buffer, target_buffer);
    }
}
