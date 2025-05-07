#ifndef PLANE_H
#define PLANE_H

#include "math_utils.h"
#include "OFFReader.h"
#include "file_utils.h"
#include <vector>



typedef struct Plane{
    float a;
    float b;
    float c;
    float d;
    bool enabled;

    Vector3f getNormal() const {
        return Vector3f(a, b, c);
    }
    
    void normalize() {
        float magnitude = sqrtf(a*a + b*b + c*c);
        printf("Pre-normalize: (%.2f, %.2f, %.2f, %.2f), magnitude = %.2f\n", a, b, c, d, magnitude);
        if (magnitude > 0.0001f) {
            a /= magnitude;
            b /= magnitude;
            c /= magnitude;
            d /= magnitude;
            printf("Post-normalize: (%.2f, %.2f, %.2f, %.2f)\n", a, b, c, d);
        } else {
            printf("Magnitude too small (%.6f), not normalizing\n", magnitude);
        }
    }
    
    float evaluate(const Vector3f& point) const {
        return a * point.x + b * point.y + c * point.z + d;
    }
} Plane;

extern float planeSize;
void updatePlaneBuffers(const std::vector<Plane>& planes, float size);


void updateActivePlanes(const Plane& plane1, const Plane& plane2, const Plane& plane3, const Plane& plane4, std::vector<Plane>& active_planes) {
    printf("\n======== UPDATING ACTIVE PLANES ========\n");
    printf("Before update: active_planes size = %zu\n", active_planes.size());
    active_planes.clear();
    
    if (plane1.enabled) {
        Plane p = plane1;
        float magnitude = sqrtf(p.a*p.a + p.b*p.b + p.c*p.c);
        if (magnitude > 0.0001f) {
            p.a /= magnitude;
            p.b /= magnitude;
            p.c /= magnitude;
            p.d /= magnitude;
        }
        active_planes.push_back(p);
        printf("Plane 1 added: (%.2f, %.2f, %.2f, %.2f)\n", p.a, p.b, p.c, p.d);
    }
    
    if (plane2.enabled) {
        Plane p = plane2;
        float magnitude = sqrtf(p.a*p.a + p.b*p.b + p.c*p.c);
        if (magnitude > 0.0001f) {
            p.a /= magnitude;
            p.b /= magnitude;
            p.c /= magnitude;
            p.d /= magnitude;
        }
        active_planes.push_back(p);
        printf("Plane 2 added: (%.2f, %.2f, %.2f, %.2f)\n", p.a, p.b, p.c, p.d);
    }
    
    if (plane3.enabled) {
        Plane p = plane3;
        float magnitude = sqrtf(p.a*p.a + p.b*p.b + p.c*p.c);
        if (magnitude > 0.0001f) {
            p.a /= magnitude;
            p.b /= magnitude;
            p.c /= magnitude;
            p.d /= magnitude;
        }
        active_planes.push_back(p);
        printf("Plane 3 added: (%.2f, %.2f, %.2f, %.2f)\n", p.a, p.b, p.c, p.d);
    }
    
    if (plane4.enabled) {
        Plane p = plane4;
        float magnitude = sqrtf(p.a*p.a + p.b*p.b + p.c*p.c);
        if (magnitude > 0.0001f) {
            p.a /= magnitude;
            p.b /= magnitude;
            p.c /= magnitude;
            p.d /= magnitude;
        }
        active_planes.push_back(p);
        printf("Plane 4 added: (%.2f, %.2f, %.2f, %.2f)\n", p.a, p.b, p.c, p.d);
    }
    
    printf("After update: active_planes size = %zu\n", active_planes.size());
    printf("=========================================\n\n");
    

}

#endif



