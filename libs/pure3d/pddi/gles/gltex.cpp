//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================

#include <pddi/gles/gl.hpp>
#include <pddi/gles/gldisplay.hpp>
#include <pddi/gles/gltex.hpp>
#include <string.h>
#include <pddi/gles/glcon.hpp>
#include <pddi/gles/decompress.hpp>

#include <math.h>
#include <string.h>
#include <pddi/base/debug.hpp>
#include <radmemory.hpp>

#include <microprofile.h>

static inline GLenum PickPixelFormat(pddiPixelFormat format)
{
    switch (format)
    {
    case PDDI_PIXEL_RGB888: return GL_BGRA_EXT;
    case PDDI_PIXEL_ARGB8888: return GL_BGRA_EXT;
#ifdef RAD_VITAGL
    case PDDI_PIXEL_DXT1: return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    case PDDI_PIXEL_DXT3: return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case PDDI_PIXEL_DXT5: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
    case PDDI_PIXEL_DXT1: return GL_RGBA;
    case PDDI_PIXEL_DXT3: return GL_RGBA;
    case PDDI_PIXEL_DXT5: return GL_RGBA;
#endif
    }
    PDDIASSERT(false);
    return GL_INVALID_ENUM;
};

static inline pddiPixelFormat PickPixelFormat(pddiTextureType type, int bitDepth, int alphaDepth)
{
    switch (type)
    {
    case PDDI_TEXTYPE_RGB:
        switch (alphaDepth)
        {
        case 0:
            return (bitDepth <= 16) ? PDDI_PIXEL_RGB565 : PDDI_PIXEL_RGB888;
        case 1:
            return (bitDepth <= 16) ? PDDI_PIXEL_ARGB1555 : PDDI_PIXEL_ARGB8888;
        default:
            return (bitDepth <= 16) ? PDDI_PIXEL_ARGB4444 : PDDI_PIXEL_ARGB8888;
        }
        break;

    case PDDI_TEXTYPE_PALETTIZED:
        return PDDI_PIXEL_PAL8;

    case PDDI_TEXTYPE_LUMINANCE:
        return PDDI_PIXEL_LUM8;

    case PDDI_TEXTYPE_BUMPMAP:
        return PDDI_PIXEL_DUDV88;

    case PDDI_TEXTYPE_DXT1:
        return PDDI_PIXEL_DXT1;

    case PDDI_TEXTYPE_DXT2:
        return PDDI_PIXEL_DXT2;

    case PDDI_TEXTYPE_DXT3:
        return PDDI_PIXEL_DXT3;

    case PDDI_TEXTYPE_DXT4:
        return PDDI_PIXEL_DXT4;

    case PDDI_TEXTYPE_DXT5:
        return PDDI_PIXEL_DXT5;

    case PDDI_TEXTYPE_YUV:
        return PDDI_PIXEL_YUV;
    }
    PDDIASSERT(false);
    return PDDI_PIXEL_UNKNOWN;
};

void pglTexture::SetGLState(void)
{
    if(context->contextID != contextID)
    {
        contextID = context->contextID;
        gltexture = 0;
#if defined(RAD_ANDROID)
        cachedMagFilter=cachedMinFilter=cachedWrapS=cachedWrapT=-1;
        cachedAnisotropy=-1.0f;
#endif
    }

    MICROPROFILE_SCOPEI("PDDI", "pglTexture::SetGLState", MP_RED);

    if(!valid)
    {
        glDeleteTextures(1, &gltexture);
        glGenTextures(1,&gltexture);
        glBindTexture(GL_TEXTURE_2D, gltexture);
#if defined(RAD_ANDROID)
        cachedMagFilter=cachedMinFilter=cachedWrapS=cachedWrapT=-1;
        cachedAnisotropy=-1.0f;
#endif

        if (type == PDDI_TEXTYPE_DXT1 || type == PDDI_TEXTYPE_DXT3 || type == PDDI_TEXTYPE_DXT5)
        {
#ifdef RAD_VITAGL
            const unsigned int blocksize = lock.format == PDDI_PIXEL_DXT1 ? 8 : 16;
            const GLenum internalFormat = lock.format == PDDI_PIXEL_DXT5 ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT :
                lock.format == PDDI_PIXEL_DXT3 ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            for(int level=0; level<=nMipMap; ++level)
            {
                const int width=rmt::Max(1,xSize>>level);
                const int height=rmt::Max(1,ySize>>level);
                glCompressedTexImage2D(GL_TEXTURE_2D,level,internalFormat,width,
                    height,0,static_cast<GLsizei>(ceil(width/4.0)*ceil(height/4.0)*blocksize),bits[level]);
            }
#else
            // Android cannot sample the source DXT payload directly on every
            // supported device, but the P3D file still contains all authored
            // mip levels. Decompress and upload each one rather than silently
            // discarding everything except level zero.
            for(int level=0; level<=nMipMap; ++level)
            {
                const int width=rmt::Max(1,xSize>>level);
                const int height=rmt::Max(1,ySize>>level);
                // The legacy decoder writes a complete 4x4 block even for
                // the terminal 2x2 and 1x1 mip levels. Decode those levels to
                // padded storage, then crop to a tightly packed GLES upload.
                const int decodedWidth=rmt::Max(4,width);
                const int decodedHeight=rmt::Max(4,height);
                unsigned char* decoded=new unsigned char[decodedWidth*decodedHeight*4];
                if(type==PDDI_TEXTYPE_DXT1)
                    BlockDecompressImageBC1(decodedWidth,decodedHeight,reinterpret_cast<const uint8_t*>(bits[level]),decoded);
                else if(type==PDDI_TEXTYPE_DXT3)
                    BlockDecompressImageBC2(decodedWidth,decodedHeight,reinterpret_cast<const uint8_t*>(bits[level]),decoded);
                else
                    BlockDecompressImageBC3(decodedWidth,decodedHeight,reinterpret_cast<const uint8_t*>(bits[level]),decoded);
                unsigned char* image=decoded;
                if(decodedWidth!=width || decodedHeight!=height)
                {
                    image=new unsigned char[width*height*4];
                    for(int y=0; y<height; ++y)
                        memcpy(image+y*width*4,decoded+y*decodedWidth*4,width*4);
                }
                glTexImage2D(GL_TEXTURE_2D,level,PickPixelFormat(lock.format),width,
                    height,0,GL_RGBA,GL_UNSIGNED_BYTE,image);
                if(image!=decoded) delete [] image;
                delete [] decoded;
            }
#endif
        }
#ifdef RAD_VITAGL
        else if (type == PDDI_TEXTYPE_YUV)
        {
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, VGL_YUV420P_BT601, xSize,
                ySize, 0, (xSize * ySize * 3) / 2, (GLvoid *)bits[0]);
        }
#endif
        else
        {
            for(int level=0; level<=nMipMap; ++level)
            {
                const int width=rmt::Max(1,xSize>>level);
                const int height=rmt::Max(1,ySize>>level);
                glTexImage2D(GL_TEXTURE_2D,level,PickPixelFormat(lock.format),width,
                    height,0,lock.native ? GL_BGRA_EXT : GL_RGBA,GL_UNSIGNED_BYTE,bits[level]);
            }
        }

        valid = true;
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, gltexture);
    }
}

int fastlog2(int x)
{
    int r = 0;
    int tmp = x;
    while(tmp > 1)
    {
        r++;
        tmp = tmp >> 1;

        if((tmp << r) != x)
            // not power of 2
            return -1;
    }
    return r;
}

bool pglTexture::Create(int x, int y, int bpp, int alphaDepth, int nMip, pddiTextureType textureType, pddiTextureUsageHint usageHint)
{
    xSize = x;
    ySize = y;
    nMipMap = nMip;
    type = textureType;

    log2X = fastlog2(xSize);
    log2Y = fastlog2(ySize);

#ifndef RAD_VITA
    if((log2X == -1) || (log2Y == -1))
    {
        lastError = PDDI_TEX_NOT_POW_2;
        return false;
    }
#endif

    if ((xSize > context->GetMaxTextureDimension()) ||
        (ySize > context->GetMaxTextureDimension()))
    {
        lastError = PDDI_TEX_TOO_BIG;
        return false;
    }

    // TODO palletized
    if (textureType == PDDI_TEXTYPE_PALETTIZED)
    {
        textureType = PDDI_TEXTYPE_RGB;
        bpp = 32;
    }

    bits = new char* [nMipMap + 1];
    if (type == PDDI_TEXTYPE_DXT1 || type == PDDI_TEXTYPE_DXT3 || type == PDDI_TEXTYPE_DXT5)
    {
        unsigned int blocksize = type == PDDI_TEXTYPE_DXT1 ? 8 : 16;
        for(int i = 0; i < nMipMap+1; i++)
            bits[i] = (char*)radMemoryAllocAligned(radMemoryGetCurrentAllocator(), size_t(ceil(double(xSize>>i)/4)*ceil(double(ySize>>i)/4)*blocksize), 16);
    }
    else
    {
        for(int i = 0; i < nMipMap+1; i++)
            bits[i] = (char*)radMemoryAllocAligned(radMemoryGetCurrentAllocator(), ((xSize>>i)*(ySize>>i)*bpp)/8, 16);
    }

    lock.depth = bpp;
    lock.format = PickPixelFormat(textureType, bpp, alphaDepth);

    if(context->GetDisplay()->ExtBGRA())
    {
        lock.native = true;
        lock.rgbaLShift[0] = lock.rgbaRShift[0] =
        lock.rgbaLShift[1] = lock.rgbaRShift[1] =
        lock.rgbaLShift[2] = lock.rgbaRShift[2] =
        lock.rgbaLShift[3] = lock.rgbaRShift[3] = 0;

        lock.rgbaMask[0] = 0x00ff0000;
        lock.rgbaMask[1] = 0x0000ff00;
        lock.rgbaMask[2] = 0x000000ff;
        lock.rgbaMask[3] = 0xff000000;
    }
    else
    {
        lock.native = false;
        lock.rgbaRShift[0] = 16;
        lock.rgbaLShift[2] = 16;

        lock.rgbaLShift[0] = 
        lock.rgbaLShift[1] = lock.rgbaRShift[1] =
        lock.rgbaRShift[2] =
        lock.rgbaLShift[3] = lock.rgbaRShift[3] = 0;

        lock.rgbaMask[0] = 0x000000ff;
        lock.rgbaMask[1] = 0x0000ff00;
        lock.rgbaMask[2] = 0x00ff0000;
        lock.rgbaMask[3] = 0xff000000;
    }

    context->ADD_STAT(PDDI_STAT_TEXTURE_ALLOC_32BIT, (float)((xSize * ySize * lock.depth) / 8192));
    context->ADD_STAT(PDDI_STAT_TEXTURE_COUNT_32BIT, 1);

    return true;
}

pglTexture::pglTexture(pglContext* c)
{
#if defined(RAD_ANDROID)
    sourceName[0]='\0';
    cachedMagFilter=cachedMinFilter=cachedWrapS=cachedWrapT=-1;
    cachedAnisotropy=-1.0f;
#endif
    context = c;
    contextID = c->contextID;
    bits = NULL;
    gltexture = 0;
    priority = 15;
    valid = false;
}

#if defined(RAD_ANDROID)
void pglTexture::SetSamplerState(int magFilter,int minFilter,int wrapS,int wrapT,float anisotropy)
{
    // Sampler parameters are stored by the GL texture object. Most P3D
    // materials submit them again for every primitive group, which is costly
    // on the Android driver for composite cars and state props.
    if(cachedMagFilter!=magFilter)
    {
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,magFilter);
        cachedMagFilter=magFilter;
    }
    if(cachedMinFilter!=minFilter)
    {
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,minFilter);
        cachedMinFilter=minFilter;
    }
    if(cachedWrapS!=wrapS)
    {
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrapS);
        cachedWrapS=wrapS;
    }
    if(cachedWrapT!=wrapT)
    {
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrapT);
        cachedWrapT=wrapT;
    }
    if(cachedAnisotropy!=anisotropy)
    {
        glTexParameterf(GL_TEXTURE_2D,0x84FE,anisotropy);
        cachedAnisotropy=anisotropy;
    }
}

void pglTexture::SetSourceName(const char* name)
{
    if(!name) { sourceName[0]='\0'; return; }
    strncpy(sourceName,name,sizeof(sourceName)-1);
    sourceName[sizeof(sourceName)-1]='\0';
}

void pglSetTextureSourceName(pddiTexture* texture,const char* name)
{
    if(texture) static_cast<pglTexture*>(texture)->SetSourceName(name);
}
#endif

pglTexture::~pglTexture()
{
    if(gltexture) glDeleteTextures(1, &gltexture);

    if(bits)
    {
        for(int i = 0; i < nMipMap + 1; i++)
            radMemoryFreeAligned(bits[i]);
        delete [] bits;
    }

    context->ADD_STAT(PDDI_STAT_TEXTURE_ALLOC_32BIT, -(float)((xSize * ySize * lock.depth) / 8192));
    context->ADD_STAT(PDDI_STAT_TEXTURE_COUNT_32BIT, -1);
}

pddiPixelFormat pglTexture::GetPixelFormat()
{
    return PDDI_PIXEL_ARGB8888;
}

int   pglTexture::GetWidth()
{
    return xSize;
}

int   pglTexture::GetHeight()
{
    return ySize;
}

int   pglTexture::GetDepth()
{
    return 32;
}

int   pglTexture::GetNumMipMaps()
{
    return nMipMap;
}

int pglTexture::GetAlphaDepth()
{
    return 8;
}

pddiLockInfo* pglTexture::Lock(int mipMap, pddiRect* rect)
{
    PDDIASSERT(mipMap <= nMipMap);

    lock.width = 1 << (log2X-mipMap);
    lock.height = 1 << (log2Y-mipMap);
    if (lock.format == PDDI_PIXEL_DXT1 || lock.format == PDDI_PIXEL_DXT3 || lock.format == PDDI_PIXEL_DXT5)
    {
        unsigned int blocksize = lock.format == PDDI_PIXEL_DXT1 ? 8 : 16;
        lock.pitch = ceil( double( xSize >> mipMap ) / 4 ) * blocksize;
        lock.bits = bits[mipMap];
    }
    else if (lock.format == PDDI_PIXEL_YUV)
    {
        lock.pitch = (lock.width * lock.depth) / 8;
        lock.bits = bits[mipMap];
    }
    else
    {
        lock.pitch = -(lock.width * 4);
        lock.bits = bits[mipMap] + (lock.width * (lock.height - 1) * 4);
    }

    return &lock;
}

void pglTexture::Unlock(int mipLevel)
{
    valid = false;
}

void pglTexture::SetPriority(int p)
{
    priority = p;
}

int pglTexture::GetPriority(void)
{
    return priority;
}

// paging control
void pglTexture::Prefetch(void)
{
}

void pglTexture::Discard(void)
{
}

// palette managment
int pglTexture::GetNumPaletteEntries(void)
{
    return 0;
}

void pglTexture::SetPalette(int nEntries, pddiColour* palette)
{
}

int pglTexture::GetPalette(pddiColour* palette)
{
    return 0;
}


