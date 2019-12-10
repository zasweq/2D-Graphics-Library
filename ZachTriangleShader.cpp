#include "GMatrix.h"
#include "GShader.h"
#include <vector>

static inline int floatTo8BitUnsignedIntt(float f) {
    assert(f >= 0 && f <=1); //Needs to be in this range to be fully represented by 8bit int
    return GFloorToInt(f * 255 + .5);
}

static inline GPixel convertColorToPixell(const GColor& color) {
    GColor gColor = color.pinToUnit(); //Do we need this? .pinToUnit()
    return GPixel_PackARGB(floatTo8BitUnsignedIntt(gColor.fA), floatTo8BitUnsignedIntt(gColor.fR * gColor.fA),
                           floatTo8BitUnsignedIntt(gColor.fG * gColor.fA), floatTo8BitUnsignedIntt(gColor.fB * gColor.fA));
}

class ZachTriangleShader: public GShader {
public:
    //Define points arbitrarly, mine will be P0, P1, and P2
    ZachTriangleShader(const GPoint points[3], const GColor colors[3]) {
        for (int i = 2; i >= 0; i--) {
            fPoints.push_back(points[i]);
            fColors.push_back(colors[i]);
        }
    }

    bool isOpaque() override {
        for (int i = 0; i < 3; i++) {
            if (fColors.at(i).fA != 1.0f) {
                return false;
            }
        }
        return true;
    }

    bool setContext(const GMatrix& ctm) override { //figure out when to return false
        GMatrix a;
        a.set6(fPoints.at(1).x() - fPoints.at(0).x(), fPoints.at(2).x() - fPoints.at(0).x(), fPoints.at(0).x(),
        fPoints.at(1).y() - fPoints.at(0).y(), fPoints.at(2).y() - fPoints.at(0).y(), fPoints.at(0).y());
        fCTM.setConcat(ctm, a);
        fCTM.invert(&fCTM);
        return true;
    }

    void shadeRow(int x, int y, int count, GPixel row[]) override {
        if (count == 0) //Assuming you can get passed in?
            return;
        GPoint p = fCTM.mapXY(x + .5f, y + .5f); //0 - 1
        GColor DC1 = GColor::MakeARGB(fColors.at(1).fA - fColors.at(0).fA, fColors.at(1).fR - fColors.at(0).fR,
                fColors.at(1).fG - fColors.at(0).fG, fColors.at(1).fB - fColors.at(0).fB);
        GColor DC2 = GColor::MakeARGB(fColors.at(2).fA - fColors.at(0).fA, fColors.at(2).fR - fColors.at(0).fR,
                                      fColors.at(2).fG - fColors.at(0).fG, fColors.at(2).fB - fColors.at(0).fB);
        float a = p.x() * DC1.fA + p.y() * DC2.fA + fColors.at(0).fA;
        float r = p.x() * DC1.fR + p.y() * DC2.fR + fColors.at(0).fR;
        float g = p.x() * DC1.fG + p.y() * DC2.fG + fColors.at(0).fG;
        float b = p.x() * DC1.fB + p.y() * DC2.fB + fColors.at(0).fB;
        GColor color = GColor::MakeARGB(a, r, g, b);
        row[0] = convertColorToPixell(color);
        //a * DC1 + d * DC2 difference amongst
        float a1 = fCTM[0] * DC1.fA + fCTM[3] * DC2.fA;
        float r1 = fCTM[0] * DC1.fR + fCTM[3] * DC2.fR;
        float g1 = fCTM[0] * DC1.fG + fCTM[3] * DC2.fG;
        float b1 = fCTM[0] * DC1.fB + fCTM[3] * DC2.fB;
        GColor DC = GColor::MakeARGB(a1, r1, g1, b1);
        for (int i = 1; i < count; i++) {
            a = a + DC.fA;
            r = r + DC.fR;
            g = g + DC.fG;
            b = b + DC.fB;
            /*a = GPixel_GetA(row[i - 1]) + DC.fA ;
            r = GPixel_GetR(row[i - 1]) + DC.fR;
            g = GPixel_GetG(row[i - 1]) + DC.fG;
            b = GPixel_GetB(row[i - 1]) + DC.fB;*/
            color = GColor::MakeARGB(a, r, g, b);
            row[i] = convertColorToPixell(color);
        }
    }

private:
    GMatrix fCTM;
    std::vector<GColor> fColors;
    std::vector<GPoint> fPoints;
};