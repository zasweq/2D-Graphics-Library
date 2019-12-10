#ifndef PA3_ZASWEQ_ZACHUTILS_H
#define PA3_ZASWEQ_ZACHUTILS_H

#include <vector>
#include "GRect.h"
#include "GMath.h"
//Why no includes here?

struct Edge {
    float slope;
    float curr_x; //rounded as you pass into drawRect
    int top;
    int bottom;
    int clockDirection; //Variable that represents whether the edge is counterclockwise or clockwise 1 represents clockwise, -1 counterclockwise
};

/**
 * This function takes in two points and returns a rounded edge for top and bottom. Assumes client won't pass in two
 * points which would make slope undefined, client can handle that case.
 */
Edge makeEdgeFromTwoPoints(GPoint a, GPoint b, bool clockwise) {

    if (a.y() > b.y()) {
        GPoint temp = a;
        a = b;
        b = temp;
    }

    assert(GRoundToInt(a.y()) != GRoundToInt(b.y()));

    float slope = (b.x() - a.x())/(b.y() - a.y());
    float triangleHeight = GRoundToInt(a.y()) - a.y() + .5;

    Edge edge;
    edge.top = GRoundToInt(a.y());
    edge.bottom = GRoundToInt(b.y());
    edge.slope = slope;
    edge.curr_x = a.x() + triangleHeight * slope;
    if (clockwise)
        edge.clockDirection = 1;
    else
        edge.clockDirection = -1;
    return edge;
}

/**
 * A function that takes in a rectangle which represents the bounds for the edge, and two points, and returns a clipped
 * edge.
 */
std::vector<Edge> clip(GIRect rect, GPoint a, GPoint b) {
    //First check in y - can return 0 or 1 edges, then check in x, can return 1, 2, 3 edges
    //Make sure b is below y for constant slope calculations
    bool clockwise;
    if (a.y() > b.y()) {
        GPoint temp = a;
        a = b;
        b = temp;
        clockwise = false; //Added clockwise condition here
    } else {
        clockwise = true;
    }

    std::vector<Edge> edges;
    //If completly out of bounds, nothing to return.
    if (a.y() >= rect.bottom() || b.y() <= rect.top()) {
        return edges;
    }

    float slope = (b.x() - a.x())/(b.y() - a.y());

    if (a.y() < rect.top()) { //TODO: Where to round properly?
        float x = a.x() + (rect.top() - a.y()) * slope;
        a.set(x, rect.top());
    }

    if (b.y() > rect.bottom()) {
        float x = b.x() - (b.y() - rect.bottom()) * slope;
        b.set(x, rect.bottom());
    }

    //Make sure b is to the right of y for constant slope calculations
    if (a.x() > b.x()) {
        GPoint temp = a;
        a = b;
        b = temp;
    }

    //If completly outside of x boundary, make one projected edge and return
    if (b.x() <= rect.left()) {
        GPoint c = GPoint::Make(rect.left(), b.y());
        GPoint d = GPoint::Make(rect.left(), a.y());
        if (GRoundToInt(c.y()) != GRoundToInt(d.y()))
            edges.push_back(makeEdgeFromTwoPoints(c, d, clockwise));
        return edges;
    }

    if (a.x() >= rect.right()) {
        GPoint c = GPoint::Make(rect.right(), b.y());
        GPoint d = GPoint::Make(rect.right(), a.y());
        if (GRoundToInt(c.y()) != GRoundToInt(d.y()))
            edges.push_back(makeEdgeFromTwoPoints(c, d, clockwise));
        return edges;
    }
    //Change the slope to standard delta y/delta x
    slope = (b.y() - a.y())/(b.x() - a.x());

    //Edges get sorted anyway, so don't need for a specific ordering from projection

    //If part of the edge is outside, project both left and right ones
    if (a.x() < rect.left()) {
        float y = a.y() + (rect.left() - a.x()) * slope;
        GPoint c = GPoint::Make(rect.left(), y);
        GPoint d = GPoint::Make(rect.left(), a.y());
        if (GRoundToInt(c.y()) != GRoundToInt(d.y()))
            edges.push_back(makeEdgeFromTwoPoints(c, d, clockwise));
        a.set(rect.left(), y);
    }

    if (b.x() > rect.right()) {
        float y = b.y() - (b.x() - rect.right()) * slope;
        GPoint c = GPoint::Make(rect.right(), y);
        GPoint d = GPoint::Make(rect.right(), b.y());
        if (GRoundToInt(c.y()) != GRoundToInt(d.y()))
            edges.push_back(makeEdgeFromTwoPoints(c, d, clockwise));
        b.set(rect.right(), y);
    }

    if (GRoundToInt(a.y()) != GRoundToInt(b.y()))
        edges.push_back(makeEdgeFromTwoPoints(a, b, clockwise));

    return edges;
}

/**
 * A comparison function used for sorting purposes.
 */
bool edgeComparison(Edge a, Edge b) {
    if (a.top < b.top) {
        return true;
    } else if (a.top > b.top) {
        return false;
    }

    if (a.curr_x < b.curr_x) {
        return true;
    } else if (a.curr_x > b.curr_x) {
        return false;
    }

    return a.slope < b.slope;
}

static inline GPoint quadBezierCurve(float t, GPoint* convertedPoints) {
    //p(t) = (1 - t)^2*P0 + 2t(1 - t)*P1 + t^2*P2
    float x = pow(1 - t, 2) * convertedPoints[0].x() + 2 * t * (1 - t) * convertedPoints[1].x() + pow(t, 2) * convertedPoints[2].x();
    float y = pow(1 - t, 2) * convertedPoints[0].y() + 2 * t * (1 - t) * convertedPoints[1].y() + pow(t, 2) * convertedPoints[2].y();
    return {x, y};
}

static inline GPoint cubicBezierCurve(float t, GPoint* convertedPoints) {
    //p(t) = (1 - t)^3*P0 + 3t(1-t)^2*P1 + 3t^2(1 - t)P2 + T^3P3
    float x = pow(1 - t, 3) * convertedPoints[0].x() + 3 * t * pow(1 - t, 2) * convertedPoints[1].x() + 3 * pow(t, 2) * (1 - t) * convertedPoints[2].x()
              + pow(t, 3) * convertedPoints[3].x();
    float y = pow(1 - t, 3) * convertedPoints[0].y() + 3 * t * pow(1 - t, 2) * convertedPoints[1].y() + 3 * pow(t, 2) * (1 - t) * convertedPoints[2].y()
              + pow(t, 3) * convertedPoints[3].y();
    return {x, y};
}

#endif //PA3_ZASWEQ_ZACHUTILS_H

bool edgeComparisonBasedOnX(Edge a, Edge b) {
    return a.curr_x < b.curr_x;
}