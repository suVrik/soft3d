// soft3d by Andrej Suvorau, 2019

typedef struct {
	float x;
	float y;
	float z;
	float u;
	float v;
} Vertex;

typedef struct {
	unsigned int length;
	Vertex* data;
} VertexBuffer;

typedef struct {
	float data[16];
} Matrix;

typedef struct {
	unsigned char b;
	unsigned char g;
	unsigned char r;
	unsigned char a;
} Color;

typedef struct {
	unsigned int width;
	unsigned int height;
	Color* data;
} ColorBuffer;

typedef struct {
	unsigned int width;
	unsigned int height;
	Color* data;
	float* depth;
} DepthColorBuffer;

extern void rasterize_vertices(const VertexBuffer* buffer, const Matrix* transform, const ColorBuffer* source_buffer, DepthColorBuffer* target_buffer);

typedef struct {
	int x;
	int y;
	float z;
	float u;
	float v;
} RasterizedVertex;

typedef struct {
	RasterizedVertex a;
	RasterizedVertex b;
	RasterizedVertex c;
} RasterizedTriangle;

extern void rasterize_triangle(const RasterizedTriangle* triangle, const ColorBuffer* source_buffer, DepthColorBuffer* target_buffer);
