#include "GMatrix.h"
#include "GShader.h"

class ZachCompositeShader: public GShader {
public:
    ZachCompositeShader(GShader* s1, GShader* s2): fS1(s1),
    fS2(s2)
    {

    }

    bool isOpaque() override {
        return fS1->isOpaque() && fS2->isOpaque();
    }

    bool setContext(const GMatrix& ctm) override {
        return fS1->setContext(ctm) && fS2->setContext(ctm);
    }

    void shadeRow(int x, int y, int count, GPixel row[]) override {
        GPixel temp[count];
        fS1->shadeRow(x, y, count, temp);
        fS2->shadeRow(x, y, count, row);
        for (int i = 0; i < count; i++) {
            row[i] = mul(temp[i], row[i]);
        }
    }

private:
    GShader* fS1;
    GShader* fS2;

    GPixel mul(GPixel a, GPixel b) {
        float alpha = GRoundToInt((GPixel_GetA(a) * GPixel_GetA(b))/255.0f);
        float red = GRoundToInt((GPixel_GetR(a) * GPixel_GetR(b))/255.0f);
        float green = GRoundToInt((GPixel_GetG(a) * GPixel_GetG(b))/255.0f);
        float blue = GRoundToInt((GPixel_GetB(a) * GPixel_GetB(b))/255.0f);
        return GPixel_PackARGB(alpha, red, green, blue);
    }
};