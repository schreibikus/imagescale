/*
 * Copyright (C) 2018 Schreibikus https://github.com/schreibikus
 * License: http://www.gnu.org/licenses/gpl.html GPL version 3 or higher
 */

texture<uchar4, 2> argb_tex;

/* This function based on Subsample_Bilinear_uchar4 function from vf_scale_cuda.cu */
extern "C" __global__ void resizeARGB(uchar4 *dst, int dst_width, int dst_height, int dst_pitch, float hscale, float vscale)
{
    int xo = blockIdx.x * blockDim.x + threadIdx.x;
    int yo = blockIdx.y * blockDim.y + threadIdx.y;

    if ((yo < dst_height) && (xo < dst_width))
    {
        float xi = (xo + 0.5f) * hscale;
        float yi = (yo + 0.5f) * vscale;

        // 3-tap filter weights are {wh,1.0,wh} and {wv,1.0,wv}
        float wh = min(max(0.5f * (hscale - 1.0f), 0.0f), 1.0f);
        float wv = min(max(0.5f * (vscale - 1.0f), 0.0f), 1.0f);

        // Convert weights to two bilinear weights -> {wh,1.0,wh} -> {wh,0.5,0} + {0,0.5,wh}
        float dx = wh / (0.5f + wh);
        float dy = wv / (0.5f + wv);

        uchar4 c0 = tex2D(argb_tex, xi-dx, yi-dy);
        uchar4 c1 = tex2D(argb_tex, xi+dx, yi-dy);
        uchar4 c2 = tex2D(argb_tex, xi-dx, yi+dy);
        uchar4 c3 = tex2D(argb_tex, xi+dx, yi+dy);
        int4 res;
        res.x =  ((int)c0.x+(int)c1.x+(int)c2.x+(int)c3.x+2) >> 2;
        res.y =  ((int)c0.y+(int)c1.y+(int)c2.y+(int)c3.y+2) >> 2;
        res.z =  ((int)c0.z+(int)c1.z+(int)c2.z+(int)c3.z+2) >> 2;
        res.w =  ((int)c0.w+(int)c1.w+(int)c2.w+(int)c3.w+2) >> 2;
        *((uchar4*)((unsigned char*)dst + yo * dst_pitch) + xo) = make_uchar4(
            (unsigned char)res.x, (unsigned char)res.y, (unsigned char)res.z, (unsigned char)res.w);
    }
}

