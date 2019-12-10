#include "GMath.h"
#include "GShader.h"
#include "GMatrix.h"
#include <algorithm>
#include <vector>

class ZachLinearGradient: public GShader {
public:
    ZachLinearGradient(GPoint p0, GPoint p1, const GColor colors[], int count, TileMode tileMode) {
        for (int i = 0; i < count; i++) {
            fColors.push_back(colors[i]);
        }
        fColors.push_back(colors[count - 1]); //For end edge case
        //Array way
        /*fColors = (GColor*) malloc((count + 1) * sizeof(GColor));
        memcpy(fColors, colors, count * sizeof(GColor));
        //fColors[count + 1]; // = new GColor[count + 1];
        for (int i = 0; i < count; i++) {
            fColors[i] = colors[i];//GColor::MakeARGB(colors[i].fA, colors[i].fR, colors[i].fG, colors[i].fB);
        }
        //Dummy variable to hedge against errors
        fColors[count] = colors[count - 1]; //GColor::MakeARGB(colors[count - 1].fA, colors[count - 1].fR, colors[count - 1].fG, colors[count - 1].fB);
        */
        if (p0.x() < p1.x()) { //logic here works sometimes, sometimes it doesn't - had fP0 instead of p0
            fP0 = p0;
            fP1 = p1;
        } else if (p0.x()==p1.x() && p0.y()<p1.y()) {
            fP0 = p0;
            fP1 = p1;
        } else {
            fP0 = p1;
            fP1 = p0;
        }
        fCount = count;
        fTileMode = tileMode;
        float dx = fP1.x() - fP0.x();
        float dy = fP1.y() - fP0.y();
        //maybe c rid of these later
        fLM.set6(dx, -dy, fP0.x(), dy, dx, fP0.y());
    }

    /*~ZachLinearGradient() {
        free(fColors);
    }*/

    bool isOpaque() override {
        for (GColor color: fColors) {
            if (color.fA != 1.0f) {
                return false;
            }
        }
        return true;
    }


    bool setContext(const GMatrix& m) override {
        GMatrix a;
        GMatrix b;
        fLM.invert(&a);
        if (!m.invert(&b)) {
            return false;
        }
        fCTM.setConcat(a, b); //This is already 0 - 1
        //need to add a scale matrix here to bring coordinates to 0, 1, scale upwards later
        return true;
    }

    void shadeRow(int x, int y, int count, GPixel row[]) override {
        //Send points through CTM
        //Use that to solve it
        //Send x and y through ctm, represents new vector space 0 - 1
        GPoint p = fCTM.mapPt({(float)x + .5f, (float)y + .5f});
        for (int i = 0; i < count; i++) {
            float sx = p.x();
            if (fTileMode == kClamp) {
                sx = clamp(sx);
            } else if (fTileMode == kRepeat) {
                sx = sx - GFloorToInt(sx);
            } else if (fTileMode == kMirror) {
                float tmp = sx * .5;
                tmp = tmp - GFloorToInt(sx);
                if (tmp > .5)
                    tmp = 1 - tmp;
                sx = tmp * 2;
            }
            //If more than one color, scale upward
            sx = sx * (fCount - 1);
            int colorIndex = GFloorToInt(sx);
            float w = sx - colorIndex;
            GColor one = fColors.at(colorIndex).pinToUnit();
            GColor two = fColors.at(colorIndex + 1).pinToUnit();
            ///GColor one = fColors[colorIndex].pinToUnit();
            //GColor two = fColors[colorIndex + 1].pinToUnit();
            /*if (colorIndex + 1 == fCount) {
                two = fColors[colorIndex];
            } else {
                two = fColors[colorIndex + 1];
            }*/
            float a = one.fA * (1 - w) + two.fA * w;
            float r = one.fR * (1 - w) + two.fR * w;
            float g = one.fG * (1 - w) + two.fG * w;
            float b = one.fB * (1 - w) + two.fB * w;
            const GColor color = GColor::MakeARGB(a, r, g, b);
            row[i] = convertColorToPixel(color);
            p.fX += fCTM[0];
        }
        return;
    }

private:
    GPoint fP0;
    GPoint fP1;
    int fCount;
    GMatrix fLM; //Represents the matrix to change the x axis basis vector to the vector represented by our line
    GMatrix fCTM; //Represents concatenation of matrix space
    std::vector<GColor> fColors;
    TileMode fTileMode;
    //GColor* fColors;

    /**
     * A representation of tiling. Clamps to 0 - 1
     */
    float clamp(float sx) {
        if (sx < 0.0f) {
            return 0;
        } else if (sx > 1.0f) {
            return 1;
        } else {
            return sx;
        }
    }


    static inline int floatTo8BitUnsignedInt(float f) {
        assert(f >= 0 && f <=1); //Needs to be in this range to be fully represented by 8bit int, sprinkle in asserts throughout program
        return floor(f * 255 + .5);
    }

/**
* This function takes a color and returns a pixel with that color. Has to pack data from color into a 32 bit pixel.
* This is done by converting the float in color to unsigned int in pixel.
*/
    static inline GPixel convertColorToPixel(const GColor& color) {
        GColor gColor = color.pinToUnit();
        int alpha = floatTo8BitUnsignedInt(gColor.fA);
        int red = floatTo8BitUnsignedInt(gColor.fR * gColor.fA);
        int green = floatTo8BitUnsignedInt(gColor.fG * gColor.fA);
        int blue = floatTo8BitUnsignedInt(gColor.fB * gColor.fA);
        return GPixel_PackARGB(alpha, red, green, blue);
    }
};

//Arbitrary amount of GColors, GPoints
std::unique_ptr<GShader> GCreateLinearGradient(GPoint p0, GPoint p1, const GColor colors[], int count, GShader::TileMode tileMode) {
    //Do I need something here?
    return std::unique_ptr<GShader>(new ZachLinearGradient(p0, p1, colors, count, tileMode));
}

/*
std::unique_ptr<GShader> GCreateBitmapShader(const GBitmap& bitmap, const GMatrix& localM) {
    if (!bitmap.pixels()) {
        return nullptr;
    }
    return std::unique_ptr<GShader>(new ZachShader(bitmap, localM));
}
*/