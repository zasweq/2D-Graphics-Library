#include "GBitmap.h"
#include "GMatrix.h"
#include "GShader.h"

class ZachShader: public GShader {
public:

    ZachShader(const GBitmap& bitmap, const GMatrix& localM, TileMode tileMode):
        fDevice(bitmap)
        , fLM(localM),
        fTileMode(tileMode) {} //constructor optimization

    bool isOpaque() override { //For optimization so you don't have to do as much computation.
        return fDevice.isOpaque();
    }


    //LM is Local Matrix, fCTM is fInverse
    //My logic: invert local Matrix into a, invert parameter into b
    //Set fCTM state to the concatination of inversion of local Matrix and inversion of parameter as well
    bool setContext(const GMatrix& m) override {
        GMatrix a;
        GMatrix b;
        fLM.invert(&a);
        if (!m.invert(&b)) {
            return false;
        }
        fCTM.setConcat(a, b);
        //Need to scale downward to 0 - 1
        //need to add a scale matrix here to bring coordinates to 0, 1, scale upwards later
        GMatrix c;
        c.setScale(1.0f/fDevice.width(), 1.0f/fDevice.height());
        fCTM.setConcat(c, fCTM);
        return true;
    }

    void shadeRow(int x, int y, int count, GPixel row[]) override {
        GPoint p = fCTM.mapPt({(float)x + .5f, (float)y + .5f});
        for (int i = 0; i < count; i++) {
            float sx = p.x();
            float sy = p.y();
            if (fTileMode == kClamp) {
                sx = clampX(sx);
                sy = clampY(sy);
            } else if (fTileMode == kRepeat) {
                sx = sx - GFloorToInt(sx);
                sy = sy - GFloorToInt(sy);
            } else if (fTileMode == kMirror) {
                float tmp = sx * .5;
                tmp = tmp - GFloorToInt(tmp);
                if (tmp > .5)
                    tmp = 1 - tmp;
                sx = tmp * 2;
                tmp = sy * .5;
                tmp = tmp - GFloorToInt(tmp);
                if (tmp > .5)
                    tmp = 1 - tmp;
                sy = tmp * 2;
            }
            sx = sx * fDevice.width();
            sy = sy * fDevice.height();
            row[i] = *fDevice.getAddr((int)sx, (int)sy);
            p.fX += fCTM[0];
            p.fY += fCTM[3];
        }
        return;
    }

private:
    GMatrix fCTM; //Represents (CTM * fLM) ^ -1
    GMatrix fLM;
    GBitmap fDevice;
    TileMode fTileMode;

    float clampX(float x) { //assume (0, 0) represents top left point
        if (x < 0) {
            return 0;
        }
        if (x > 1) {
            return .9999f;
        }
        return x;
        /*if (x < 0) {
            return 0;
        }
        if (x > fDevice.width() - 1) {
            return fDevice.width() - 1;
        }
        return floor(x);*/
    }

    float clampY(float y) {
        if (y < 0) {
            return 0;
        }
        if (y > 1) {
            return .9999f;
        }
        return y;
        /*if (y < 0) {
            return 0;
        }
        if (y > fDevice.height() - 1) {
            return fDevice.height() - 1;
        }
        return floor(y);*/
    }
};

std::unique_ptr<GShader> GCreateBitmapShader(const GBitmap& bitmap, const GMatrix& localM, GShader::TileMode tileMode) {
    if (!bitmap.pixels()) {
        return nullptr;
    }
    return std::unique_ptr<GShader>(new ZachShader(bitmap, localM, tileMode));
}