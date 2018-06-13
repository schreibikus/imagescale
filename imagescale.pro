TARGET = imagescale
CONFIG += console
SOURCES += src/scaler.cpp \
           src/imagescale.cpp \
           src/dynlink_cuda.c \

HEADERS += src/scaler.h \
           src/dynlink_cuda.h \
           src/dynlink_cuda_cuda.h \

OBJECTS += kernelcode.o

LIBS += -ldl

PRE_TARGETDEPS += kernelcode.o
kernelcode.target = kernelcode.o
kernelcode.commands = /usr/local/cuda/bin/nvcc -cubin -gencode arch=compute_61,code=sm_61 -O2 src/scaler_kernel.cu -o scaler_kernel.cubin && ld -r -b binary -o kernelcode.o scaler_kernel.cubin && rm -f scaler_kernel.cubin
kernelcode.depends = src/scaler_kernel.cu

QMAKE_EXTRA_TARGETS += kernelcode
