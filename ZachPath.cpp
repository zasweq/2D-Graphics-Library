#include <math.h>
#include "GPath.h"
#include "GMatrix.h"

/**
*  Append a new contour, made up of the 4 points of the specified rect, in the specified
*  direction. The contour will begin at the top-left corner of the rect.
*/

GPath& GPath::addRect(const GRect& rect, Direction direction) {
    float left = rect.left();
    float right = rect.right();
    float top = rect.top();
    float bottom = rect.bottom();
    moveTo(left, top);
    if (direction == kCW_Direction) {
        lineTo(right, top);
        lineTo(right, bottom);
        lineTo(left, bottom);
    } else {
        lineTo(left, bottom);
        lineTo(right, bottom);
        lineTo(right, top);
    }
    return *this;
}

/**
*  Append a new contour with the specified polygon. Calling this is equivalent to calling
*  moveTo(pts[0]), lineTo(pts[1..count-1]).
*/

GPath& GPath::addPolygon(const GPoint pts[], int count) {
    //Again, not error checking for speed
    moveTo(pts[0]);
    for (int i = 1; i < count; i++) {
        lineTo(pts[i]);
    }
    return *this;
}

/**
*  Return the bounds of all of the control-points in the path.
*
*  If there are no points, return {0, 0, 0, 0}
*/

GRect GPath::bounds() const {
    if (fPts.size() == 0)
        return GRect::MakeLTRB(0, 0, 0, 0);
    float left = fPts.at(0).x();
    float right = fPts.at(0).x();
    float top = fPts.at(0).y();
    float bottom = fPts.at(0).y();
    for (int i = 1; i < fPts.size(); i++) {
        left = std::min(left, fPts.at(i).x());
        top = std::min(top, fPts.at(i).y());
        right = std::max(right, fPts.at(i).x());
        bottom = std::max(bottom, fPts.at(i).y());
    }
    return GRect::MakeLTRB(left, top, right, bottom);
}

/**
*  Transform the path in-place by the specified matrix.
*/
void GPath::transform(const GMatrix& matrix) {
    //Don't use array, map each point indvidually
    for (int i = 0; i < fPts.size(); i++) {
        fPts[i] = matrix.mapPt(fPts.at(i));
    }
}


GPath& GPath::addCircle(GPoint center, float radius, Direction direction) {
    //Define constants so won't recompute variables
    float offset1 = tan(M_PI/8) * radius; //this is the same thing as offset 3
    float offset2 = (sqrt(2)/2) * radius;
    float offset3 = radius / tan((3 * M_PI) / 8);
    if (direction == kCW_Direction) {
        moveTo({center.x() + radius, center.y()});
        quadTo({center.x() + radius, center.y() - offset1}, {center.x() + offset2, center.y() - offset2});
        quadTo({center.x() + offset1, center.y() - radius}, {center.x(), center.y() - radius});
        quadTo({center.x() - offset1, center.y() - radius}, {center.x() - offset2, center.y() - offset2});
        quadTo({center.x() - radius, center.y() - offset1}, {center.x() - radius, center.y()});
        quadTo({center.x() - radius, center.y() + offset1}, {center.x() - offset2, center.y() + offset2});
        quadTo({center.x() - offset1, center.y() + radius}, {center.x(), center.y() + radius});
        quadTo({center.x() + offset1, center.y() + radius}, {center.x() + offset2, center.y() + offset2});
        quadTo({center.x() + radius, center.y() + offset1}, {center.x() + radius, center.y()});
    }
    else if (direction == kCCW_Direction) {
        moveTo({center.x() + radius, center.y()});
        quadTo({center.x() + radius, center.y() + offset1}, {center.x() + offset2, center.y() + offset2});
        quadTo({center.x() + offset1, center.y() + radius}, {center.x(), center.y() + radius});
        quadTo({center.x() - offset1, center.y() + radius}, {center.x() - offset2, center.y() + offset2});
        quadTo({center.x() - radius, center.y() + offset1}, {center.x() - radius, center.y()});
        quadTo({center.x() - radius, center.y() - offset1}, {center.x() - offset2, center.y() - offset2});
        quadTo({center.x() - offset1, center.y() - radius}, {center.x(), center.y() - radius});
        quadTo({center.x() + offset1, center.y() - radius}, {center.x() + offset2, center.y() - offset2});
        quadTo({center.x() + radius, center.y() - offset1}, {center.x() + radius, center.y()});
    }
    return *this;
}

//Three points, chop into two segments 1 2 (3) 4 5
void GPath::ChopQuadAt(const GPoint src[3], GPoint dst[5], float t) {
    //Construct three GPoints that represent t on each line segment
    GPoint a, b, c;
    float x, y;
    x = src[0].x() * (1.0f - t) + src[1].x() * t;
    y = src[0].y() * (1.0f - t) + src[1].y() * t;
    a.set(x, y);
    x = src[1].x() * (1.0f - t) + src[2].x() * t;
    y = src[1].y() * (1.0f - t) + src[2].y() * t;
    b.set(x, y);
    x = a.x() * (1.0f - t) + b.x() * t;
    y = a.y() * (1.0f - t) + b.y() * t;
    c.set(x, y);
    dst[0] = src[0];
    dst[1] = a;
    dst[2] = c;
    dst[3] = b;
    dst[4] = src[2];
}

//Four points, chop into two segements 1 2 3 (4) 5 6 7
void GPath::ChopCubicAt(const GPoint src[4], GPoint dst[7], float t) {
    //Construct three GPoints that represent t on each line segment
    GPoint a, b, c, d, e, f; //represents weighted average with respect to t
    float x, y;
    x = src[0].x() * (1.0f - t) + src[1].x() * t;
    y = src[0].y() * (1.0f - t) + src[1].y() * t;
    a.set(x, y);
    dst[1] = a;
    x = src[1].x() * (1.0f - t) + src[2].x() * t;
    y = src[1].y() * (1.0f - t) + src[2].y() * t;
    b.set(x, y);
    x = src[2].x() * (1.0f - t) + src[3].x() * t;
    y = src[2].y() * (1.0f - t) + src[3].y() * t;
    c.set(x, y);
    x = a.x() * (1.0f - t) + b.x() * t;
    y = a.y() * (1.0f - t) + b.y() * t;
    d.set(x, y);
    x = b.x() * (1.0f - t) + c.x() * t;
    y = b.y() * (1.0f - t) + c.y() * t;
    f.set(x, y);
    x = d.x() * (1.0f - t) + f.x() * t;
    y = d.y() * (1.0f - t) + f.y() * t;
    e.set(x, y);

    dst[0] = src[0];
    dst[1] = a;
    dst[2] = d;
    dst[3] = e;
    dst[4] = f;
    dst[5] = c;
    dst[6] = src[3];
}