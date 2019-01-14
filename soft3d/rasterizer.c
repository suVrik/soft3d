// soft3d by Andrej Suvorau, 2019

#include "soft3d.h"

#include <assert.h>
#include <stdlib.h>

static inline RasterizedVertex convert_vertex(const Vertex* vertex, const Matrix* transform, float screen_w, float screen_h) {
	const float x = vertex->x * transform->data[0] + vertex->y * transform->data[4] + vertex->z * transform->data[8]  + transform->data[12];
	const float y = vertex->x * transform->data[1] + vertex->y * transform->data[5] + vertex->z * transform->data[9]  + transform->data[13];
	const float z = vertex->x * transform->data[2] + vertex->y * transform->data[6] + vertex->z * transform->data[10] + transform->data[14];
	const float w = vertex->x * transform->data[3] + vertex->y * transform->data[7] + vertex->z * transform->data[11] + transform->data[15];

	assert(w != 0.f);

	RasterizedVertex result;
	result.x = (int)((x / w + 0.5f) * screen_w);
	result.y = (int)((y / w + 0.5f) * screen_h);
	result.z = z / w;
	result.u = vertex->u;
	result.v = vertex->v;
	return result;
}

static inline void sort_vertices(RasterizedTriangle* triangle) {
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

static inline void draw_pixel(int x, int y, const RasterizedVertex* a, const RasterizedVertex* b, const RasterizedVertex* c,
							  float ba, float bb, float bc, const ColorBuffer* source_buffer, DepthColorBuffer* target_buffer) {
	if (x >= 0 && y >= 0 && x < (int)target_buffer->width && y < (int)target_buffer->height) {
		const float z = a->z * ba + b->z * bb + c->z * bc;

		if (z >= 0.f && target_buffer->depth[y * target_buffer->width + x] < z) {
			const float u = a->u * ba + b->u * bb + c->u * bc;
			const float v = a->v * ba + b->v * bb + c->v * bc;

			const int du = min(max((int)(u * source_buffer->width), 0), (int)source_buffer->width - 1);
			const int dv = min(max((int)(v * source_buffer->height), 0), (int)source_buffer->height - 1);

			target_buffer->data[y * target_buffer->width + x] = source_buffer->data[dv * source_buffer->width + du];
			target_buffer->depth[y * target_buffer->width + x] = z;
		}
	}
}

void rasterize_triangle(const RasterizedTriangle* triangle, const ColorBuffer* source_buffer, DepthColorBuffer* target_buffer) {
	// shortcuts.
	const RasterizedVertex* const ta = &triangle->a;
	const RasterizedVertex* const tb = &triangle->b;
	const RasterizedVertex* const tc = &triangle->c;

	const int ax = triangle->a.x;
	const int ay = triangle->a.y;
	const int bx = triangle->b.x;
	const int by = triangle->b.y;
	const int cx = triangle->c.x;
	const int cy = triangle->c.y;

	int scy = ay; // shared current Y.
	int sty = by; // shared target Y.

	const RasterizedVertex* vc = ta; // current variant.
	const RasterizedVertex* vn = tb; // next edge.

	int vax; // variant (AB) X distance.
	int vad; // variant (AB) X delta.
	if (ax > bx) {
		vax = ax - bx;
		vad = -1;
	} else {
		vax = bx - ax;
		vad = 1;
	}
	int vay = by - ay; // variant (AB) Y distance (delta is 1).
	int vac = ax; // variant (AB) current X.
	int vacp = ax; // variant (AB) previous X.
	int vat = bx; // variant (AB) target X.
	int vae = vax - vay; // variant (AB) error.
	float vab = 1.f; // variant (AB) barycentric.
	float vabp = 1.f; // variant (AB) previous barycentric.
	float vabd = -1.f / (vax + vay + 1); // variant (AB) barycentric delta.
	int var = 0; // defines whether variant (AB) is finished on line.

	int acx; // AC X distance.
	int acd; // AC X delta.
	if (ax > cx) {
		acx = ax - cx;
		acd = -1;
	} else {
		acx = cx - ax;
		acd = 1;
	}
	const int acy = cy - ay; // AC Y distance (delta is 1).
	int acc = ax; // AC current X.
	int accp = ax; // AC previous X.
	const int act = cx; // AC target X.
	int ace = acx - acy; // AC error.
	float acb = 0.f; // AC barycentric.
	float acbp = 0.f; // AC previous barycentric.
	const float acbd = 1.f / (acx + acy + 1); // AC barycentric delta.
	int acr = 0; // defines whether AC is finished on line.

	int pdir = 1; // direction from vacp to accp.
	float pcnt = 0.f; // the inverse number of steps from vacp to accp.

draw_lines:
	// calculate fill helpers.
	accp = acc; // fill until this X position.
	acbp = acb; // fill until this barycentric value.

	if (vac < acc) {
		const int diff = acc - vac;
		vacp = vac + 1; // start from this X position.
		vabp = vab + vabd; // start from this barycentric value.
		pcnt = 1.f / diff; // delta applied to vabp until it gets to acbp.
		pdir = 1; // direction for vacp.
	} else if (vac > acc) {
		const int diff = vac - acc;
		vacp = vac - 1; // start from this X position.
		vabp = vab - vabd; // start from this barycentric value.
		pcnt = 1.f / diff; // delta applied to vabp until it gets to acbp.
		pdir = -1; // direction for vacp.
	} else {
		vacp = accp = 0; // do not fill anything.
	}

	while (scy < sty) {
		if (var == 0) { // variant edge is not finished on this line.
			if (2 * vae > -vay) { // there's more pixels on this line.
				draw_pixel(vac, scy, vc, vn, tc, vab, 1.f - vab, 0.f, source_buffer, target_buffer);

				vae -= vay; // update error.
				vac += vad; // move variant X.
				vab += vabd; // update barycentric.
			}

			if (2 * vae < vax) {
				vae += vax; // update error.
				vab += vabd; // update barycentric.
				var = 1; // variant edge is finished on this line.
			}
		} else {
			if (acr == 0) { // AC is not finished on this line.
				if (2 * ace > -acy) { // there's more pixels on this line.
					draw_pixel(acc, scy, ta, tb, tc, 1.f - acb, 0.f, acb, source_buffer, target_buffer);

					ace -= acy; // update error.
					acc += acd; // move AC X.
					acb += acbd; // update barycentric.
				}

				if (2 * ace < acx) {
					ace += acx; // update error.
					acb += acbd; // update barycentric.
					acr = 1; // AC is finished on this line.
				}
			} else { // both variant and AC are finished on this line.
				var = acr = 0; // reset finished state.

				//if (vc == tb) { // BC edge is compressing the triangle.
				//	if (vac < accp) {
				//		const int diff = accp - vac;
				//		vacp = vac; // start from this X position.
				//		vabp = vab + vabd; // start from this barycentric value.
				//		pcnt = 1.f / diff; // delta applied to vabp until it gets to acbp.
				//		pdir = 1; // direction for vacp.
				//	} else if (vac > accp) {
				//		const int diff = vac - accp;
				//		vacp = vac; // start from this X position.
				//		vabp = vab - vabd; // start from this barycentric value.
				//		pcnt = 1.f / diff; // delta applied to vabp until it gets to acbp.
				//		pdir = -1; // direction for vacp.
				//	} else {
				//		vacp = accp = 0; // do not fill anything.
				//	}
				//}

				// fill the triangle.
				for (float factor = 1.f; vacp != accp; vacp += pdir, factor -= pcnt) {
					const float inverse_factor = 1.f - factor;

					if (vc == ta) {
						// here A in the middle.
						const float af = vabp * factor + (1.f - acbp) * inverse_factor;
						const float cf = acbp * inverse_factor;
						const float bf = 1.f - af - cf;

						draw_pixel(vacp, scy, ta, tb, tc, af, bf, cf, source_buffer, target_buffer);
					} else {
						// here C in the middle.
						const float cf = (1.f - vabp) * factor + acbp * inverse_factor;
						const float bf = vabp * factor;
						const float af = 1.f - bf - cf;

						draw_pixel(vacp, scy, ta, tb, tc, af, bf, cf, source_buffer, target_buffer);
					}
				}

				scy++; // increase Y for both edges.

				draw_pixel(vac, scy, vc, vn, tc, vab, 1.f - vab, 0.f, source_buffer, target_buffer);
				draw_pixel(acc, scy, ta, tb, tc, 1.f - acb, 0.f, acb, source_buffer, target_buffer);

				goto draw_lines; // calculate fill helpers and draw following lines.
			}
		}
	}

	// faster algorithm for the last line.
	while (vac != vat) {
		vac += vad; // move variant X.
		vab += vabd; // update barycentric.

		draw_pixel(vac, scy, vc, vn, tc, vab, 1.f - vab, 0.f, source_buffer, target_buffer);
	}

	if (vc == ta) {
		scy = by;
		sty = cy;

		vc = tb; // current variant edge.
		vn = tc; // next edge.

		if (bx > cx) {
			vax = bx - cx; // variant (BC) X distance.
			vad = -1; // variant (BC) X delta.
		} else {
			vax = cx - bx; // variant (BC) X distance.
			vad = 1; // variant (BC) X delta.
		}
		vay = cy - by; // variant (BC) Y distance (delta is 1).
		vac = bx; // variant (BC) current X.
		vat = cx; // variant (BC) target X.
		vae = vax - vay; // variant (BC) error.
		vab = 1.f; // variant (BC) barycentric.
		vabp = 1.f; // variant (AB) previous barycentric.
		vabd = -1.f / (vax + vay + 1); // variant (BC) barycentric delta.

		goto draw_lines; // calculate fill helpers and draw second half of a triangle.
	}

	// faster algorithm for the last line.
	while (acc != act) {
		acc += acd; // move AC X.
		acb += acbd; // update barycentric.

		draw_pixel(acc, scy, ta, tb, tc, 1.f - acb, 0.f, acb, source_buffer, target_buffer);
	}
}

void rasterize_vertices(const VertexBuffer* buffer, const Matrix* transform, const ColorBuffer* source_buffer, DepthColorBuffer* target_buffer) {
	assert(buffer != NULL && target_buffer != NULL && target_buffer->data != NULL && source_buffer != NULL && source_buffer->data != NULL);
	assert(buffer->length % 3 == 0);

	for (size_t i = 0; i < buffer->length; i += 3) {
		RasterizedTriangle triangle = { convert_vertex(buffer->data + i, transform, (float)target_buffer->width, (float)target_buffer->height),
										convert_vertex(buffer->data + i + 1, transform, (float)target_buffer->width, (float)target_buffer->height),
										convert_vertex(buffer->data + i + 2, transform, (float)target_buffer->width, (float)target_buffer->height) };

		sort_vertices(&triangle);
		rasterize_triangle(&triangle, source_buffer, target_buffer);
	}
}
