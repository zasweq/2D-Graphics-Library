#include <math.h>
#include "GMatrix.h"

//Note, we are not subclassing, but simply implementing methods
void GMatrix::setIdentity() {
    this->set6(1, 0, 0, 0, 1, 0);
}

void GMatrix::setTranslate(float tx, float ty) {
    this->set6(1, 0, tx, 0, 1, ty);
}

void GMatrix::setScale(float sx, float sy) {
    this->set6(sx, 0, 0, 0, sy, 0);
}

void GMatrix::setRotate(float radians) {
    this->set6(cos(radians), -1 * sin(radians), 0, sin(radians), cos(radians), 0);
}

void GMatrix::setConcat(const GMatrix &secundo, const GMatrix &primo) {
    //If you first apply primo matrix to the points, then the second, you must do secundo*primo
    //Note: watch out for aliasing - store temp A's and then store in matrix
    float a = secundo[0] * primo[0] + secundo[1] * primo[3];
    float b = secundo[0] * primo[1] + secundo[1] * primo[4];
    float c = secundo[0] * primo[2] + secundo[1] * primo[5] + secundo[2];
    float d = secundo[3] * primo[0] + secundo[4] * primo[3];
    float e = secundo[3] * primo[1] + secundo[4] * primo[4];
    float f = secundo[3] * primo[2] + secundo[4] * primo[5] + secundo[5];
    this->set6(a, b, c, d, e, f);
}

/**
 * If current matrix is invertable, invert and set GMatrix parameter to that Matrix
 */
bool GMatrix::invert(GMatrix *inverse) const {\
    float a = this->fMat[0];
    float b = this->fMat[1];
    float c = this->fMat[2];
    float d = this->fMat[3];
    float e = this->fMat[4];
    float f = this->fMat[5];

    float determinant = a * e - b * d;
    if (determinant == 0) {
        return false;
    }

    float multiplier = 1 / determinant;

    inverse->set6(
            e * multiplier, -b * multiplier,  (b * f - c * e) * multiplier,
            -d * multiplier, a * multiplier, (c * d - a * f) * multiplier);

    return true;
}

void GMatrix::mapPoints(GPoint dst[], const GPoint src[], int count) const {
    //Take points in src, apply matrix, store it in points - client makes sure he passes in correct points
    for (int i = 0; i < count; i++) {
        float x = this->fMat[0] * src[i].x() + this->fMat[1] * src[i].y() + this->fMat[2]; //ax + by +c
        float y = this->fMat[3] * src[i].x() + this->fMat[4] * src[i].y() + this->fMat[5];
        dst[i].set(x, y); //Calls default constructor, hits none, so you can do this
    }
    return;
}