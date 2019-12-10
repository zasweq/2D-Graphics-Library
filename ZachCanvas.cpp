#include "GBitmap.h"
#include "GPixel.h"
#include "GCanvas.h"
#include "GColor.h"
#include "GPaint.h"
#include "GPoint.h"
#include "Blend.h"
#include <vector>
#include <stack>
#include <iostream>
#include "ZachUtils.h"
#include "GShader.h"
#include "GPath.h"
#include "ZachProxyShader.cpp"
#include "ZachTriangleShader.cpp"
#include "ZachCompositeShader.cpp"

class Canvas : public GCanvas {
public:
    Canvas(const GBitmap& device) : fDevice(device) {
        stackOfMatricies.push(GMatrix()); //initial state of canvas is an identity matrix
    }

    /**
     * Save off a copy of canvas state to be later used if balancing call to restore is made.
     */
    void save() override {
        GMatrix& ctm = stackOfMatricies.top();
        GMatrix push(ctm[0], ctm[1], ctm[2], ctm[3], ctm[4], ctm[5]);
        stackOfMatricies.push(push);
    }

    /**
     * Copy canvas state (Current Trans. Matr) that was recorded in corresponding call to save() back into Canvas.
     * Error to call this if there has been no previous call to save().
     */
    void restore() override {
        //could possibly check for error conditions, however would slow down this library
        stackOfMatricies.pop();
    }

    /**
     * Modifies the CTM by preconcatinating the specified Matrix with the CTM. The canvas is constructed with an
     * Identity CTM.
     */

    void concat(const GMatrix& matrix) override {
        stackOfMatricies.top().setConcat(stackOfMatricies.top(), matrix);
    }

    /**
     * Clears Canvas by setting the whole Canvas to one color. Affects the entire underlying bitmap.
     */
    void drawPaint(const GPaint& paint) override {
        GPixel pixel = convertColorToPixel(paint.getColor());

        for (int row = 0; row < fDevice.height(); row++) {
            blitRow(row, 0, fDevice.width(), paint);
        }
    }

    /**
     * Fills a polygon shape on the bitmap by blending a rectangle with a bitmap based on a pre defined blend type.
     */
    void drawRect(const GRect& rect, const GPaint& paint) override {
        GPoint a = GPoint::Make(rect.left(), rect.top());
        GPoint b = GPoint::Make(rect.right(), rect.top());
        GPoint c = GPoint::Make(rect.right(), rect.bottom());
        GPoint d = GPoint::Make(rect.left(), rect.bottom());
        GPoint rectPoints[] = {a, b, c, d};
        drawConvexPolygon(rectPoints, 4, paint);
    }

    /**
     * Fills a polygon shape on the bitmap by blending the polygon with the bitmap based on a pre defined blend type.
     */
    void drawConvexPolygon(const GPoint points[], int count, const GPaint& paint) override {
        //Hardcode this check, however I thought he wouldn't pass in bad inputs?
        if (count < 0)
            return;

        GIRect bounds = GIRect::MakeWH(fDevice.width(), fDevice.height());
        std::vector<Edge> edges;

        //convert GPoints to new GPoint array using ctm, which will use the rest of way. Doesn't modify in place as to
        //not mess with clients memory
        GPoint convertedPoints[count];
        stackOfMatricies.top().mapPoints(convertedPoints, points, count);

        for (int i = 0; i < count; i++) {
            GPoint a = convertedPoints[i];
            GPoint b = convertedPoints[(i + 1) % count];
            std::vector<Edge> edgesToAdd = clip(bounds, a, b);
            for (int j = 0; j < edgesToAdd.size(); j++) {
                edges.push_back(edgesToAdd.at(j));
            }
        }

        if (edges.size() == 0)
            return;

        std::sort(edges.begin(), edges.end(), edgeComparison);

        assert(edges.size() >= 2); //If less than 2, it means clipped wrong

        Edge left = edges.at(0);
        Edge right = edges.at(1);
        int pos = 2;
        int y = left.top;
        int bottomY = edges.at(edges.size() - 1).bottom;
        while (y < bottomY) {
            if (left.bottom <= y) {
                left = edges.at(pos);
                pos++;
            }
            if (right.bottom <= y) {
                right = edges.at(pos);
                pos++;
            }

            //Blit row after picking up new edge
            blitRow(y, GRoundToInt(left.curr_x), GRoundToInt(right.curr_x), paint);
            left.curr_x += left.slope;
            right.curr_x += right.slope;
            y++;
        }
    }

    void drawPath(const GPath& path, const GPaint& paint) override {
        //Iterate through iterator, construct edges with an up and down state
        GPath::Edger edger(path);
        GPoint points[4];
        GPoint curvePoints[2];

        GIRect bounds = GIRect::MakeWH(fDevice.width(), fDevice.height());

        std::vector<Edge> edges;
        GPath::Verb verb = edger.next(points);
        int n;
        float dt, t;
        std::vector<Edge> edgesToAdd;
        //see if you can do the thing where you go (initzalization) !=

        while (verb != GPath::kDone) { //the points array can be anything make it case by case basis
            switch (verb) {
                case GPath::kLine: {
                    GPoint convertedPoints[2];
                    stackOfMatricies.top().mapPoints(convertedPoints, points, 2);

                    std::vector<Edge> edgesToAdd = clip(bounds, convertedPoints[0], convertedPoints[1]);
                    for (int j = 0; j < edgesToAdd.size(); j++) {
                        edges.push_back(edgesToAdd.at(j));
                    }
                    break;
                }
                case GPath::kQuad: {
                    GPoint convertedQuadraticCurve[3]; //maybe can get rid of this for optimization
                    stackOfMatricies.top().mapPoints(convertedQuadraticCurve, points, 3);
                    //construct equation - I don't know whether to do this before or after
                    //Solve for n
                    //n = sqrt(|d| / tol) tol = 1/4
                    float dX = (convertedQuadraticCurve[0].x() - 2 * convertedQuadraticCurve[1].x() +
                                convertedQuadraticCurve[2].x()) / 4.0f;
                    float dY = (convertedQuadraticCurve[0].y() - 2 * convertedQuadraticCurve[1].y() +
                                convertedQuadraticCurve[2].y()) / 4.0f;
                    n = sqrt(sqrt(pow(dX, 2) + pow(dY, 2)) * 4);
                    dt = 1.0f / n;
                    t = 0;
                    for (int i = 0; i < n; i++) {
                        curvePoints[0] = quadBezierCurve(t, convertedQuadraticCurve);
                        curvePoints[1] = quadBezierCurve(t + dt, convertedQuadraticCurve);
                        t += dt;
                        edgesToAdd = clip(bounds, curvePoints[0], curvePoints[1]);
                        for (int j = 0; j < edgesToAdd.size(); j++) {
                            edges.push_back(edgesToAdd.at(j));
                        }
                    }

                    break;
                }
                case GPath::kCubic: {
                    GPoint convertedCubicCurve[4];
                    stackOfMatricies.top().mapPoints(convertedCubicCurve, points, 4);
                    //construct equation - I don't know whether to do this before or after
                    //Solve for n
                    float dX1 = (convertedCubicCurve[0].x() - 2 * convertedCubicCurve[1].x() +
                                 convertedCubicCurve[2].x()) / 4.0f;
                    float dY1 = (convertedCubicCurve[0].y() - 2 * convertedCubicCurve[1].y() +
                                 convertedCubicCurve[2].y()) / 4.0f;
                    float dX2 = (convertedCubicCurve[1].x() - 2 * convertedCubicCurve[2].x() +
                                 convertedCubicCurve[3].x()) / 4.0f;
                    float dY2 = (convertedCubicCurve[1].y() - 2 * convertedCubicCurve[2].y() +
                                 convertedCubicCurve[3].y()) / 4.0f;
                    float d = std::max(sqrt(pow(dX1, 2) + pow(dY1, 2)),
                                       sqrt(pow(dX2, 2) + pow(dY2, 2)));
                    n = sqrt(d * 12);
                    dt = 1.0f / n;
                    t = 0;
                    for (int i = 0; i < n; i++) {
                        curvePoints[0] = cubicBezierCurve(t, convertedCubicCurve);
                        curvePoints[1] = cubicBezierCurve(t + dt, convertedCubicCurve);
                        t += dt;
                        edgesToAdd = clip(bounds, curvePoints[0], curvePoints[1]);
                        for (int j = 0; j < edgesToAdd.size(); j++) {
                            edges.push_back(edgesToAdd.at(j));
                        }
                    }

                    break;
                }
            }
            verb = edger.next(points);
        }

        if (edges.size() == 0)
            return;

        std::sort(edges.begin(), edges.end(), edgeComparison);

        complexScan(edges, paint);

        return;
    }

    void drawMesh(const GPoint verts[], const GColor colors[], const GPoint texs[],
                  int count, const int indices[], const GPaint& paint) override {
        //Make a proxy shader based on basis vectors
        int n = 0;
        GPoint a, b, c;
        GColor d, e, f;
        GPoint* meshVerts = nullptr;
        GColor* meshColors = nullptr;
        GPoint* meshTexs = nullptr;
        for (int i = 0; i < count; i++) {
            if (verts) {
                a = verts[indices[n + 0]];
                b = verts[indices[n + 1]];
                c = verts[indices[n + 2]];
                meshVerts = new GPoint[3];
                meshVerts[0] = a;
                meshVerts[1] = b;
                meshVerts[2] = c;
            }
            if (colors) {
                d = colors[indices[n + 0]];
                e = colors[indices[n + 1]];
                f = colors[indices[n + 2]];
                meshColors = new GColor[3];
                meshColors[0] = d;
                meshColors[1] = e;
                meshColors[2] = f;
            }
            if (texs) {
                a = texs[indices[n + 0]];
                b = texs[indices[n + 1]];
                c = texs[indices[n + 2]];
                meshTexs = new GPoint[3];
                meshTexs[0] = a;
                meshTexs[1] = b;
                meshTexs[2] = c;
            }
            n += 3;
            drawTriangle(meshVerts, meshColors, meshTexs, paint.getShader());
            delete(meshVerts);
            delete(meshColors);
            delete(meshTexs);
        }
    }

    void drawTriangle(const GPoint verts[3], const GColor colors[3], const GPoint texs[3], GShader* sh) {
        //Declare on stack because allocating with new is slow
        ZachTriangleShader* ts = nullptr;
        ZachProxyShader* ps = nullptr;
        ZachCompositeShader* cs = nullptr;
        GShader* s;
        if (colors) {
            ts = new ZachTriangleShader(verts, colors);
            s = ts;
        }
        if (texs) {
            //Have it here
            GMatrix P = computeBasis(verts[0], verts[1], verts[2]);
            GMatrix T = computeBasis(texs[0], texs[1], texs[2]);
            T.invert(&T);
            GMatrix proxyAddition;
            proxyAddition.setConcat(P, T);
            ps = new ZachProxyShader(sh, proxyAddition);
            s = ps;
        }
        if (colors && texs) {
            cs = new ZachCompositeShader(ts, ps);
            s = cs;
        }
        GPaint p(s);
        drawConvexPolygon(verts, 3, p);
        if (ts)
            delete(ts);
        if (ps)
            delete(ps);
        if (cs)
            delete(cs);
    }

    /**
     *  Draw the quad, with optional color and/or texture coordinate at each corner. Tesselate
     *  the quad based on "level":
     *      level == 0 --> 1 quad  -->  2 triangles
     *      level == 1 --> 4 quads -->  8 triangles
     *      level == 2 --> 9 quads --> 18 triangles
     *      ...
     *  The 4 corners of the quad are specified in this order:
     *      top-left --> top-right --> bottom-right --> bottom-left
     *  Each quad is triangulated on the diagonal top-right --> bottom-left
     *      0---1
     *      |  /|
     *      | / |
     *      |/  |
     *      3---2
     *
     *  colors and/or texs can be null. The resulting triangles should be passed to drawMesh(...).
     */
    void drawQuad(const GPoint verts[4], const GColor colors[4], const GPoint texs[4],
                  int level, const GPaint& paint) override {
        for (int i = 0; i <= level; i++) {
            for (int j = 0; j <= level; j++) {
                //Want to get the points, also want to get the new Colors and texs for each part of Quad, calculate u and v here
                float u1 = ((float)i)/(level + 1);
                float u2 = ((float)i + 1)/(level + 1);
                float v1 = ((float)j)/(level + 1);
                float v2 = ((float)j + 1)/(level + 1);
                GPoint* newVerts = nullptr;
                GColor* newColors = nullptr;
                GPoint* newTexs = nullptr;
                GPoint a = calculateQuadPoint(verts[0], verts[1], verts[2], verts[3], u1, v1);
                GPoint b = calculateQuadPoint(verts[0], verts[1], verts[2], verts[3], u2, v1);
                GPoint c = calculateQuadPoint(verts[0], verts[1], verts[2], verts[3], u2, v2);
                GPoint d = calculateQuadPoint(verts[0], verts[1], verts[2], verts[3], u1, v2);
                newVerts = new GPoint[4];
                newVerts[0] = a;
                newVerts[1] = b;
                newVerts[2] = c;
                newVerts[3] = d;

                if (colors) {
                    GColor e = calculateQuadColor(colors[0], colors[1], colors[2], colors[3], u1, v1);
                    GColor f = calculateQuadColor(colors[0], colors[1], colors[2], colors[3], u2, v1);
                    GColor g = calculateQuadColor(colors[0], colors[1], colors[2], colors[3], u2, v2);
                    GColor h = calculateQuadColor(colors[0], colors[1], colors[2], colors[3], u1, v2);
                    newColors = new GColor[4];
                    newColors[0] = e;
                    newColors[1] = f;
                    newColors[2] = g;
                    newColors[3] = h;
                }
                if (texs) {
                    a = calculateQuadPoint(texs[0], texs[1], texs[2], texs[3], u1, v1);
                    b = calculateQuadPoint(texs[0], texs[1], texs[2], texs[3], u2, v1);
                    c = calculateQuadPoint(texs[0], texs[1], texs[2], texs[3], u2, v2);
                    d = calculateQuadPoint(texs[0], texs[1], texs[2], texs[3], u1, v2);
                    newTexs = new GPoint[4];
                    newTexs[0] = a;
                    newTexs[1] = b;
                    newTexs[2] = c;
                    newTexs[3] = d;
                }
                int indices[6] = {0, 1, 3, 1, 3, 2};
                drawMesh(newVerts, newColors, newTexs, 2, indices, paint);
            }
        }
    }

private:
    const GBitmap fDevice;
    std::stack<GMatrix> stackOfMatricies;

    //(1 - u)(1 - v)P0 + (1-u)(v)P3 + u(1 - v)P1 + uvP2
    GPoint calculateQuadPoint(GPoint p0, GPoint p1, GPoint p2, GPoint p3, float u, float v) {
        float x = (1 - u) * (1 - v) * p0.x() + (1 - u) * v * p3.x() + u * (1 - v) * p1.x() + u * v * p2.x();
        float y = (1 - u) * (1 - v) * p0.y() + (1 - u) * v * p3.y() + u * (1 - v) * p1.y() + u * v * p2.y();
        return {x, y};
    }

    GColor calculateQuadColor(GColor c0, GColor c1, GColor c2, GColor c3, float u, float v) {
        float a = (1 - u) * (1 - v) * c0.fA + (1 - u) * v * c3.fA + u * (1 - v) * c1.fA + u * v * c2.fA;
        float r = (1 - u) * (1 - v) * c0.fR + (1 - u) * v * c3.fR + u * (1 - v) * c1.fR + u * v * c2.fR;
        float g = (1 - u) * (1 - v) * c0.fG + (1 - u) * v * c3.fG + u * (1 - v) * c1.fG + u * v * c2.fG;
        float b = (1 - u) * (1 - v) * c0.fB + (1 - u) * v * c3.fB + u * (1 - v) * c1.fB + u * v * c2.fB;
        return {a, r, g, b};
    }

    GMatrix computeBasis(GPoint one, GPoint two, GPoint three) {
        GMatrix matrix;
        matrix.set6(two.x() - one.x(), three.x() - one.x(), one.x(),
                    two.y() - one.y(), three.y() - one.y(), one.y());
        return matrix;
    }

    void blitRow(int y, int start, int end, const GPaint& paint) { //paint encapsulates shader now, make this function agnostic
        GPixel pixel = convertColorToPixel(paint.getColor().pinToUnit());
        BlendMode blendMode = getBlendMode(paint.getBlendMode());
        if (paint.getShader() == nullptr) { ////Previously, src was known (just a pixel), and dst was what changed. Now, source could also change.
            if (end <= start) //Hard code this, maybe remove this later
                return;
            GPixel *address = fDevice.getAddr(start, y);
            *address = blendMode(pixel, *address);
            for (int i = start + 1; i < end; i++) {
                address++;
                *address = blendMode(pixel, *address);
            }

            /*for (int i = start; i < end; i++) {
                GPixel *address = fDevice.getAddr(i, y); //Compute once, add it
                *address = blendMode(pixel, *address); //slow, use switch
            }*/
        } else { //Shader, source also changes
            GShader* shader = paint.getShader();
            if (!shader->setContext(stackOfMatricies.top()))
                return;
            if (end <= start) //Hard code this, maybe remove this later
                return;
            int count = end - start;
            GPixel pixelsFromShader[count];
            //GPixel* pixelsFromShader = new GPixel[count]; //inefficient, currently making it from Spock clock start 1 end 0
            shader->shadeRow(start, y, count, pixelsFromShader);
            GPixel* address = fDevice.getAddr(start, y);
            *address = blendMode(pixelsFromShader[0], *address);
            for (int i = start + 1; i < end; i++) {
                address++;
                *address = blendMode(pixelsFromShader[i - start], *address);
            }

            /*for (int i = start; i < end; i++) { //same thing as before, however source pixels also change
                GPixel* address = fDevice.getAddr(i, y); //If Opaque, can add optimization
                *address = blendMode(pixelsFromShader[i - start], *address); //Compute once, then add
            }*/
            //delete[] pixelsFromShader;
        }
    }

    /**
     * This function takes a set of edges and draws them based on the winding fill specification.
     */
    void complexScan(std::vector<Edge> edges, const GPaint& paint) {
        int count = edges.size();
        assert(count > 0);
        //Algorithm is same as before, start at a top y, go downward, however this one is determined by count decrementing
        int y = edges[0].top;
        while (count > 0) { //new condition is where there still is edges in vector
            //Figure out what these represent
            int index = 0;
            int winding = 0; //represents whether algorithm will draw or not

            int left;
            int right;

            // loop through active edges - this allows you to visit as many edges as you need
            // Already prepared for us, even from the first sort
            // Represents the edges still active from sort
            while (index < count && edges[index].top <= y) { //Every y
                // work with edge[index];
                Edge& edge = edges[index];

                if (edge.bottom <= y) { //I don't know if it's y or not
                    edges.erase(edges.begin() + index);
                    count--;
                    continue;
                }

                // check w (for left) → did we go from 0 to non-0
                if (winding == 0) {
                    left = GRoundToInt(edge.curr_x);
                }
                // update w → w += edge[index].winding
                winding += edge.clockDirection;
                // check w (for right and blit) → did we go from non-0 to 0
                if (winding == 0) {
                    right = GRoundToInt(edge.curr_x);
                    blitRow(y, left, right, paint);
                }
                //Update the current edge by slope
                edge.curr_x += edge.slope;
                index++;
            }

            y++;

            // move index to include edges that will be valid
            while (index < count && y == edges[index].top) {
                index += 1;
            }

            // sort edge[] from [0...index) based solely on currX
            std::sort(edges.begin(), edges.begin() + index, edgeComparisonBasedOnX);
        }
    }

    //What IDE do you use
    //Thank him for the class
    //Ask him about my SDL

    //Adds to a path from p0 to p1 with the specified width, roundCap determines whether it has a round cap attached to it
    //Draw everything clockwise
    void final_strokeLine(GPath* dst, GPoint p0, GPoint p1, float width, bool roundCap) override {
        //v = u transpose * r/|u|
        GPoint u;
        u.set(p1.x() - p0.x(), p1.y() - p0.y());
        GPoint uTranspose;
        uTranspose.set(-u.y(), u.x());
        GPoint v;
        float length = sqrt(u.x() * u.x() + u.y() * u.y());
        v.set(uTranspose.x() * width/(2.0f * length), uTranspose.y() * width/(2.0f * length));
        dst->moveTo(p0 + v);
        dst->lineTo(p1 + v);
        dst->lineTo(p1 - v);
        dst->lineTo(p0 - v);
        if (roundCap) {
            dst->addCircle(p0, width/2.0f, GPath::kCW_Direction);
            dst->addCircle(p1, width/2.0f, GPath::kCW_Direction);
        }
        return;
    }
};

/**
* A smart pointer that owns and manages another object through a pointer and disposes of that object when the unique_ptr
* goes out of scope.
*/
std::unique_ptr<GCanvas> GCreateCanvas(const GBitmap& device) {
    if (!device.pixels()) {
        return nullptr;
    }
    return std::unique_ptr<GCanvas>(new Canvas(device));
}

//Draw whatever you want
void GDrawSomething_rects(GCanvas* canvas) {
    GColor color = GColor::MakeARGB(0.8f,
                                    0.8f,
                                    0.8f,
                                    0.8f);
    canvas->drawPaint(GPaint(color));
}