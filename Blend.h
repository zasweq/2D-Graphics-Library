#include "GPaint.h"
#include "GPixel.h"
#include "GMath.h"

#ifndef PA2_ZASWEQ_BLEND_H
#define PA2_ZASWEQ_BLEND_H

/*(GPixel source, GPixel* destination, int sourceAlpha, int destinationAlpha) {

}

void Blend(GPixel source, GPixel* address, GBlendMode blendMode) {
    switch (blendMode) {
        case kClear:
            *address = 0;
            break;
        case kSrc:
            *address = ;
            break;
        case kDst:
            *address = 0;
            break;
        case kSrcOver:

            break;
        case kDstOver:

            break;
        case kSrcIn:

            break;
        case kDstIn:

            break;
        case kSrcOut:

            break;
        case kDstOut:

            break;
        case kSrcATop:

            break;
        case kDstATop:

            break;
        case kXor:

            break;
    }
}*/

/*
    next four methods written by Mike Reed for COMP475
*/
//Figure out what this does
uint64_t expand(uint32_t x) {
    uint64_t hi = x & 0xFF00FF00;  // the A and G components
    uint64_t lo = x & 0x00FF00FF;  // the R and B components
    return (hi << 24) | lo;
}
// turn 0xXX into 0x00XX00XX00XX00XX
uint64_t replicate(uint64_t x) {
    return (x << 48) | (x << 32) | (x << 16) | x;
}
// turn 0x..AA..CC..BB..DrD into 0xAABBCCDD
uint32_t compact(uint64_t x) {
    return ((x >> 24) & 0xFF00FF00) | (x & 0xFF00FF);
}
uint32_t quad_mul_div255(uint32_t x, uint8_t invA) {
    uint64_t prod = expand(x) * invA;
    prod += replicate(128);
    prod += (prod >> 8) & replicate(0xFF);
    prod >>= 8;
    return compact(prod);
}

static inline int floatTo8BitUnsignedInt(float f) {
    assert(f >= 0 && f <=1); //Needs to be in this range to be fully represented by 8bit int
    return GFloorToInt(f * 255 + .5);
}

/**
* This function takes a color and returns a pixel with that color. Has to pack floats from color into a 32 bit pixel.
* This is done by converting the float in color to unsigned int in pixel.
*/
static inline GPixel convertColorToPixel(const GColor& color) {
    GColor gColor = color.pinToUnit(); //Do we need this?
    return GPixel_PackARGB(floatTo8BitUnsignedInt(gColor.fA), floatTo8BitUnsignedInt(gColor.fR * gColor.fA),
                           floatTo8BitUnsignedInt(gColor.fG * gColor.fA), floatTo8BitUnsignedInt(gColor.fB * gColor.fA));
}

static inline GPixel Clear(GPixel& source, GPixel& destination) {
    return 0;
}

static inline GPixel Src(GPixel& source, GPixel& destination) {
    return source;
}

static inline GPixel Dst(GPixel& source, GPixel& destination) {
    return destination;
}

//[Sa + Da * (1 - Sa), Sc + Dc * (1 - Sa)]
static inline GPixel SrcOver(GPixel& source, GPixel& destination) {
    return (source + quad_mul_div255(destination, 255 - GPixel_GetA(source)));
}

//[Da + Sa * (1 - Da), Dc + Sc * (1 - Da)]
static inline GPixel DstOver(GPixel& source, GPixel& destination) {
    return (destination + quad_mul_div255(source, 255 - GPixel_GetA(destination)));
}

//[Sa * Da, Sc * Da]
static inline GPixel SrcIn(GPixel& source, GPixel& destination) {
    return quad_mul_div255(source, GPixel_GetA(destination));
}

//!< [Da * Sa, Dc * Sa]
static inline GPixel DstIn(GPixel& source, GPixel& destination) {
    return (quad_mul_div255(destination, GPixel_GetA(source)));
}

//[Sa * (1 - Da), Sc * (1 - Da)]
static inline GPixel SrcOut(GPixel& source, GPixel& destination) {
    return quad_mul_div255(source, 255-GPixel_GetA(destination));
}

//[Da * (1 - Sa), Dc * (1 - Sa)]
static inline GPixel DstOut(GPixel& source, GPixel& destination) {
    return quad_mul_div255(destination, 255-GPixel_GetA(source));
}

//[Da, Sc * Da + Dc * (1 - Sa)]
static inline GPixel SrcATop(GPixel& source, GPixel& destination) {
    return quad_mul_div255(source,GPixel_GetA(destination))+quad_mul_div255(destination,255-GPixel_GetA(source));
}

//[Sa, Dc * Sa + Sc * (1 - Da)]
static inline GPixel DstATop(GPixel& source, GPixel& destination) {
    return quad_mul_div255(destination, GPixel_GetA(source))+quad_mul_div255(source,255-GPixel_GetA(destination));
}

//[Sa + Da - 2 * Sa * Da, Sc * (1 - Da) + Dc * (1 - Sa)]
static inline GPixel Xor(GPixel& source, GPixel& destination) {
    return quad_mul_div255(destination,255-GPixel_GetA(source))+quad_mul_div255(source,255-GPixel_GetA(destination));
}

typedef GPixel (*BlendMode)(GPixel&, GPixel&);

BlendMode BLEND_MODE[] = {
        Clear,
        Src,
        Dst,
        SrcOver,
        DstOver,
        SrcIn,
        DstIn,
        SrcOut,
        DstOut,
        SrcATop,
        DstATop,
        Xor
};

static inline BlendMode getBlendMode(const GBlendMode mode) {
    return BLEND_MODE[static_cast<int>(mode)];
}


#endif //PA2_ZASWEQ_BLEND_H