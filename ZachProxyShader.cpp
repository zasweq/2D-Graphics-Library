#include "GMatrix.h"
#include "GShader.h"

class ZachProxyShader: public GShader {
public:
    ZachProxyShader(GShader* shader, const GMatrix& extraTransform):
    fRealShader(shader),
    fExtraTransform(extraTransform)
    {

    }

    bool isOpaque() override {
        return fRealShader->isOpaque();
    }

    bool setContext(const GMatrix& ctm) override {
        GMatrix total;
        total.setConcat(ctm, fExtraTransform);
        return fRealShader->setContext(total);
    }

    void shadeRow(int x, int y, int count, GPixel row[]) override {
        fRealShader->shadeRow(x, y, count, row);
    }

private:
    GShader* fRealShader;
    GMatrix fExtraTransform;
};