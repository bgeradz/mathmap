#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <libgimp/gimp.h>

#include "builtins.h"
#include "postfix.h"
#include "tags.h"
#include "overload.h"

extern gint preview_width, preview_height;
extern guchar *fast_image_source;
extern int imageWidth,
    imageHeight,
    previewing;
extern gint sel_x1, sel_y1,
    sel_width, sel_height;
extern double middleX,
    middleY;
extern unsigned char *imageData;
extern int intersamplingEnabled,
    oversamplingEnabled;
extern double user_curve_values[];
extern int user_curve_points;
extern tuple_t gradient_samples[];
extern int num_gradient_samples;
extern int edge_behaviour_color, edge_behaviour_wrap, edge_behaviour_reflect;
extern int edge_behaviour_mode;
extern unsigned char edge_color[4];

builtin *firstBuiltin = 0;

void mathmap_get_pixel (int x, int y, unsigned char *pixel);

extern int originX,
    originY,
    wholeImageWidth,
    wholeImageHeight;

static void
get_pixel (int x, int y, guchar *pixel)
{ 
    if (edge_behaviour_mode == edge_behaviour_wrap)
    {
	if (x < 0)
	    x = x % wholeImageWidth + wholeImageWidth;
	else if (x >= wholeImageWidth)
	    x %= wholeImageWidth;
	if (y < 0)
	    y = y % wholeImageHeight + wholeImageHeight;
	else if (y >= wholeImageHeight)
	    y %= wholeImageHeight;
    }
    else if (edge_behaviour_mode == edge_behaviour_reflect)
    {
	if (x < 0)
	    x = -x % wholeImageWidth;
	else if (x >= wholeImageWidth)
	    x = (wholeImageWidth - 1) - (x % wholeImageWidth);
	if (y < 0)
	    y = -y % wholeImageHeight;
	else if (y >= wholeImageHeight)
	    y = (wholeImageHeight - 1) - (y % wholeImageHeight);
    }

    if (previewing)
    {
	x = (x - sel_x1) * preview_width / sel_width;
	y = (y - sel_y1) * preview_height / sel_height;

	if (x < 0 || x > preview_width || y < 0 || y >= preview_height)
	    memcpy(pixel, edge_color, 4);
	else
	    memcpy(pixel, fast_image_source + (x + y * preview_width) * 4, 4);
    }
    else
	mathmap_get_pixel(x, y, pixel);
}

void
getOrigValPixel (float x, float y, unsigned char *pixel)
{
    x += originX + middleX;
    y = -y + originY + middleY;

    if (!oversamplingEnabled)
    {
	x += 0.5;
	y += 0.5;
    }

    get_pixel(floor(x), floor(y), pixel);
}

void
getOrigValIntersamplePixel (float x, float y, unsigned char *pixel)
{
    int x1,
	x2,
	y1,
	y2;
    float x2fact,
	y2fact,
	x1fact,
	y1fact,
	p1fact,
	p2fact,
	p3fact,
	p4fact;
    unsigned char pixel1a[4],
	pixel2a[4],
	pixel3a[4],
	pixel4a[4];
    unsigned char *pixel1 = pixel1a,
	*pixel2 = pixel2a,
	*pixel3 = pixel3a,
	*pixel4 = pixel4a;
    int i;

    x += middleX + originX;
    y = -y + middleY + originY;

    x1 = floor(x);
    x2 = x1 + 1;
    y1 = floor(y);
    y2 = y1 + 1;
    x2fact = (x - x1);
    y2fact = (y - y1);
    x1fact = 1.0 - x2fact;
    y1fact = 1.0 - y2fact;
    p1fact = x1fact * y1fact;
    p2fact = x1fact * y2fact;
    p3fact = x2fact * y1fact;
    p4fact = x2fact * y2fact;

    get_pixel(x1, y1, pixel1);
    get_pixel(x1, y2, pixel2);
    get_pixel(x2, y1, pixel3);
    get_pixel(x2, y2, pixel4);

    for (i = 0; i < 4; ++i)
	pixel[i] = pixel1[i] * p1fact
	    + pixel2[i] * p2fact
	    + pixel3[i] * p3fact
	    + pixel4[i] * p4fact;
}

#define MAX_LINEAR_DIM       10
#define MAT(r,c)             (a[exch[r] * dim + (c)])
#define RHS(r)               (b[exch[r]])

void
solve_linear_equations (int dim, float *a, float *b)
{
    float r[MAX_LINEAR_DIM];
    int exch[MAX_LINEAR_DIM];
    int i;

    assert(dim <= MAX_LINEAR_DIM);

    for (i = 0; i < dim; ++i)
	exch[i] = i;

    for (i = 0; i < dim - 1; ++i)
    {
	int p;

	for (p = i; p < dim; ++p) /* find pivot element */
	    if (MAT(p, i) != 0.0)
		break;

	if (p != dim)
	{
	    int j;

	    if (p != i)
	    {
		int tmp;

		tmp = exch[p];
		exch[p] = exch[i];
		exch[i] = tmp;
	    }

	    for (j = i + 1; j < dim; ++j)
	    {
		if (MAT(j, i) != 0.0)
		{
		    float f = MAT(i, i) / MAT(j, i);
		    int k;

		    MAT(j, i) = 0.0;
		    for (k = i + 1; k < dim; ++k)
			MAT(j, k) = MAT(j, k) * f - MAT(i, k);

		    RHS(j) = RHS(j) * f - RHS(i);
		}
	    }
	}
    }

    for (i = dim - 1; i >= 0; --i)
    {
	if (MAT(i, i) == 0.0)
	    RHS(i) = 0.0;	/* this should be an error condition */
	else
	{
	    int j;
	    float v = 0.0;

	    for (j = i + 1; j < dim; ++j)
		v += MAT(i, j) * r[j];

	    r[i] = (RHS(i) - v) / MAT(i, i);
	}
    }

    for (i = 0; i < dim; ++i)
	b[i] = r[i];

    /*
    if (dim == 2)
    {
	float r[2];

	if (a[2] != 0)
	{
	    r[1] = (b[0] - a[0] * b[1] / a[2]) / (a[1] - a[3] * a[0] / a[2]);
	    r[0] = (b[1] - a[3] * r[1]) / a[2];
	}
	else if (a[0] != 0)
	{
	    r[1] = (b[1] - a[2] * b[0] / a[0]) / (a[3] - a[1] * a[2] / a[0]);
	    r[0] = (b[0] - a[1] * r[1]) / a[0];
	}
	else
	    r[0] = r[1] = 0;

	b[0] = r[0];
	b[1] = r[1];
    }
    else
	assert(0);
    */

    /*
    integer n = dim;
    integer nrhs = 1;
    integer lda = dim;
    integer ldb = dim;
    integer info;
    integer ipiv[dim];

    sgesv_(&n, &nrhs, a, &lda, ipiv, b, &ldb, &info);
    */
}

#include "builtins_interpreter.c"

builtin_function_t
builtin_with_name (const char *name)
{
    if (strcmp(name, "origVal") == 0)
    {
	if (intersamplingEnabled)
	    return builtin_origValXYIntersample;
	else
	    return builtin_origValXY;
    }

    return 0;
}
