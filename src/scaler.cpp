/*
 * Copyright (C) 2018 Schreibikus https://github.com/schreibikus
 * License: http://www.gnu.org/licenses/gpl.html GPL version 3 or higher
 */
#include "scaler.h"
#include <QImageReader>
#include <stdio.h>
#include "dynlink_cuda_cuda.h"

#define DIV_UP(a, b) ( ((a) + (b) - 1) / (b) )
#define BLOCKX 32
#define BLOCKY 16

static void *gCudaDriver = 0;
static CUcontext gVideoCtx;
extern "C" const unsigned char _binary_scaler_kernel_cubin_start[];

Scaler::Scaler(QObject *parent, const char *infile, const char *outfile, int outwidth, int outheight) : QObject(parent), output(outfile), width(outwidth), height(outheight)
{
    CUresult cuStatus;
    CUdevice device;
    int ccmajor;
    int ccminor;

    cuStatus = cuInit(0, __CUDA_API_VERSION, &gCudaDriver);
    if(cuStatus != CUDA_SUCCESS)
    {
        fprintf(stderr, "Failed init CUDA: [%d]\n", cuStatus);
        exit(1);
    }
    cuStatus = cuDeviceGet(&device, 0);
    if(cuStatus != CUDA_SUCCESS)
    {
        fprintf(stderr, "Failed get CUDA device: [%d]\n", cuStatus);
        exit(1);
    }
    cuStatus = cuDeviceComputeCapability(&ccmajor, &ccminor, device);
    if(cuStatus != CUDA_SUCCESS)
    {
        fprintf(stderr, "Failed get CUDA info: [%d]\n", cuStatus);
        exit(1);
    }
    cuStatus = cuCtxCreate(&gVideoCtx, CU_CTX_SCHED_AUTO, device);
    if(cuStatus != CUDA_SUCCESS)
    {
        fprintf(stderr, "Failed create CUDA context: [%d]\n", cuStatus);
        exit(1);
    }
    cuStatus = cuCtxPopCurrent(&gVideoCtx);
    if(cuStatus != CUDA_SUCCESS)
    {
        fprintf(stderr, "Failed pop CUDA context: [%d]\n", cuStatus);
        exit(1);
    }
    printf("Created CUDA Context: device with CC %d.%d\n", ccmajor, ccminor);

    QImageReader reader(infile);
    if(reader.read(&image))
    {
    }
    else
    {
        fprintf(stderr, "Failed read image: [%s]\n", infile);
        exit(1);
    }
}

void Scaler::run()
{
    CUfunction funcResizeARGB;
    CUtexref argbTexture;
    CUmodule cudaModule;
    CUresult cuStatus;

    cuStatus = cuCtxPushCurrent(gVideoCtx);
    if(cuStatus == CUDA_SUCCESS)
    {
        cuStatus = cuModuleLoadData(&cudaModule, _binary_scaler_kernel_cubin_start);
        if(cuStatus == CUDA_SUCCESS)
        {
            cuStatus = cuModuleGetFunction(&funcResizeARGB, cudaModule, "resizeARGB");
            if(cuStatus == CUDA_SUCCESS)
            {
                cuStatus = cuModuleGetTexRef(&argbTexture, cudaModule, "argb_tex");
                if(cuStatus == CUDA_SUCCESS)
                {
                    QImage rgbImage = image.convertToFormat(QImage::Format_ARGB32);
                    CUDA_MEMCPY2D memcpy2D;
                    CUdeviceptr srcBuffer;
                    CUdeviceptr dstBuffer;
                    size_t srcPitch;
                    size_t dstPitch;

                    cuStatus = cuMemAllocPitch(&srcBuffer, &srcPitch, rgbImage.width() * 4, rgbImage.height(), 16);
                    if(cuStatus == CUDA_SUCCESS)
                    {
                        cuStatus = cuMemAllocPitch(&dstBuffer, &dstPitch, width * 4, height, 16);
                        if(cuStatus == CUDA_SUCCESS)
                        {
                            memset(&memcpy2D, 0, sizeof(memcpy2D));

                            memcpy2D.srcMemoryType = CU_MEMORYTYPE_HOST;
                            memcpy2D.srcHost = rgbImage.bits();
                            memcpy2D.srcPitch = rgbImage.bytesPerLine();
                            memcpy2D.dstMemoryType = CU_MEMORYTYPE_DEVICE;
                            memcpy2D.dstDevice = srcBuffer;
                            memcpy2D.dstPitch = srcPitch;
                            memcpy2D.WidthInBytes = rgbImage.width() * 4;
                            memcpy2D.Height = rgbImage.height();

                            cuStatus = cuMemcpy2D(&memcpy2D);
                            if(cuStatus == CUDA_SUCCESS)
                            {
                                unsigned char *outImageBuffer;
                                cuStatus = cuMemAllocHost((void**)&outImageBuffer, width * height * 4);
                                if(cuStatus == CUDA_SUCCESS)
                                {
                                    uint64_t nsecs = 0;
#if 0 // just copy
                                    memset(&memcpy2D, 0, sizeof(memcpy2D));

                                    memcpy2D.srcMemoryType = CU_MEMORYTYPE_DEVICE;
                                    memcpy2D.srcDevice = srcBuffer;
                                    memcpy2D.srcPitch = srcPitch;
                                    memcpy2D.dstMemoryType = CU_MEMORYTYPE_DEVICE;
                                    memcpy2D.dstDevice = dstBuffer;
                                    memcpy2D.dstPitch = dstPitch;
                                    memcpy2D.WidthInBytes = width * 4;
                                    memcpy2D.Height = height;

                                    cuStatus = cuMemcpy2D(&memcpy2D);
#else
                                    CUDA_ARRAY_DESCRIPTOR desc;
                                    int intDstPitch = dstPitch;
                                    float hscale = (float)rgbImage.width() / (float)width;
                                    float vscale = (float)rgbImage.height() / (float)height;
                                    void *args_uchar[] = { &dstBuffer, &width, &height, &intDstPitch, &hscale, &vscale };

                                    desc.Width  = rgbImage.width();
                                    desc.Height = rgbImage.height();
                                    desc.Format = CU_AD_FORMAT_UNSIGNED_INT8;
                                    desc.NumChannels = 4; // ARGB - 4 bytes per pixel

                                    cuStatus = cuTexRefSetFlags(argbTexture, CU_TRSF_READ_AS_INTEGER);
                                    if(cuStatus == CUDA_SUCCESS)
                                    {
                                        cuStatus = cuTexRefSetFilterMode(argbTexture, CU_TR_FILTER_MODE_LINEAR);
                                        if(cuStatus == CUDA_SUCCESS)
                                        {
                                            cuStatus = cuTexRefSetAddress2D(argbTexture, &desc, srcBuffer, srcPitch);
                                            if(cuStatus == CUDA_SUCCESS)
                                            {
                                                QElapsedTimer timer;

                                                timer.start();
                                                cuStatus = cuLaunchKernel(funcResizeARGB, DIV_UP(width, BLOCKX), DIV_UP(height, BLOCKY), 1, BLOCKX, BLOCKY, 1, 0, 0, args_uchar, NULL);
                                                nsecs = timer.nsecsElapsed();

                                            }
                                            else
                                            {
                                                fprintf(stderr, "Failed set source image address: [%d]\n", cuStatus);
                                            }
                                        }
                                        else
                                        {
                                            fprintf(stderr, "Failed set texture filter mode: [%d]\n", cuStatus);
                                        }
                                    }
                                    else
                                    {
                                        fprintf(stderr, "Failed set texture flag: [%d]\n", cuStatus);
                                    }
#endif
                                    if(cuStatus == CUDA_SUCCESS)
                                    {
                                        memset(&memcpy2D, 0, sizeof(memcpy2D));

                                        memcpy2D.srcMemoryType = CU_MEMORYTYPE_DEVICE;
                                        memcpy2D.srcDevice = dstBuffer;
                                        memcpy2D.srcPitch = dstPitch;
                                        memcpy2D.dstMemoryType = CU_MEMORYTYPE_HOST;
                                        memcpy2D.dstHost = outImageBuffer;
                                        memcpy2D.dstPitch = width * 4;
                                        memcpy2D.WidthInBytes = width * 4;
                                        memcpy2D.Height = height;

                                        cuStatus = cuMemcpy2D(&memcpy2D);
                                        if(cuStatus == CUDA_SUCCESS)
                                        {
                                            QImage outputImage(outImageBuffer, width, height, width * 4, QImage::Format_ARGB32);

                                            if(outputImage.save(output))
                                                printf("Processed image %dx%d to %dx%d %lf us\n", rgbImage.width(), rgbImage.height(), width, height, (double)nsecs / (double)1000.0);
                                            else
                                                fprintf(stderr, "Fail process image %dx%d to %dx%d\n", image.width(), image.height(), width, height);
                                        }
                                        else
                                        {
                                            fprintf(stderr, "Failed copy image from device to host: [%d]\n", cuStatus);
                                        }
                                    }
                                    else
                                    {
                                        fprintf(stderr, "Failed process image: [%d]\n", cuStatus);
                                    }

                                    cuStatus = cuMemFreeHost(outImageBuffer);
                                    if(cuStatus != CUDA_SUCCESS)
                                        fprintf(stderr, "Failed free buffer: [%d]\n", cuStatus);
                                }
                                else
                                {
                                    fprintf(stderr, "Failed alloc %d bytes for buffer: [%d]\n", cuStatus, width * height * 4);
                                }
                            }
                            else
                            {
                                fprintf(stderr, "Failed copy image from host to device: [%d]\n", cuStatus);
                            }

                            cuStatus = cuMemFree(dstBuffer);
                            if(cuStatus != CUDA_SUCCESS)
                                fprintf(stderr, "Failed free memory: [%d]\n", cuStatus);
                        }
                        else
                        {
                            fprintf(stderr, "Failed alloc %d bytes GPU memory: [%d]\n", width * height * 4, cuStatus);
                        }

                        cuStatus = cuMemFree(srcBuffer);
                        if(cuStatus != CUDA_SUCCESS)
                            fprintf(stderr, "Failed free memory: [%d]\n", cuStatus);
                    }
                    else
                    {
                        fprintf(stderr, "Failed alloc %d bytes GPU memory: [%d]\n", rgbImage.width() * rgbImage.height() * 4, cuStatus);
                    }
                }
                else
                {
                    fprintf(stderr, "Failed get argb_tex addr: [%d]\n", cuStatus);
                }
            }
            else
            {
                fprintf(stderr, "Failed get resizeARGB kernel: [%d]\n", cuStatus);
            }

            cuStatus = cuModuleUnload(cudaModule);
            if(cuStatus != CUDA_SUCCESS)
            {
                fprintf(stderr, "Failed unload CUDA module: [%d]\n", cuStatus);
            }
        }
        else
        {
            fprintf(stderr, "Failed load CUDA module: [%d]\n", cuStatus);
        }
        cuStatus = cuCtxPopCurrent(&gVideoCtx);
        if(cuStatus != CUDA_SUCCESS)
        {
            fprintf(stderr, "Failed pop CUDA context: [%d]\n", cuStatus);
        }
    }
    else
    {
        fprintf(stderr, "Failed push CUDA context: [%d]\n", cuStatus);
    }

    emit finished();
}
