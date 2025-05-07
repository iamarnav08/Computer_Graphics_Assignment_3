#include "math_utils.h"
extern Vector3f cameraPos;
extern Vector3f cameraFront;
extern Vector3f cameraUp;

// Camera state
struct Camera {
    Vector3f position;
    Vector3f target;
    Vector3f up;
    float yaw;
    float pitch;
    float speed;
} camera;

// Mouse state
struct MouseState {
    bool firstMouse;
    float lastX;
    float lastY;
} mouseState;

Matrix4f createViewMatrix(Vector3f position, Vector3f target, Vector3f up) {
    Matrix4f view;
    
    // Calculate camera direction vectors
    Vector3f zaxis = target.Normalize();
    // Vector3f zaxis = (target-position).Normalize();
    // Vector3f xaxis = up.Cross(zaxis).Normalize();
    Vector3f xaxis = zaxis.Cross(up).Normalize();
    Vector3f yaxis = zaxis.Cross(xaxis);
    
    // Create view matrix
    view.m[0][0] = xaxis.x;
    view.m[0][1] = yaxis.x;
    view.m[0][2] = zaxis.x;
    view.m[0][3] = 0.0f;
    view.m[1][0] = xaxis.y;
    view.m[1][1] = yaxis.y;
    view.m[1][2] = zaxis.y;
    view.m[1][3] = 0.0f;
    view.m[2][0] = xaxis.z;
    view.m[2][1] = yaxis.z;
    view.m[2][2] = zaxis.z;
    view.m[2][3] = 0.0f;
    view.m[3][0] = -xaxis.Dot(position);
    view.m[3][1] = -yaxis.Dot(position);
    view.m[3][2] = -zaxis.Dot(position);
    view.m[3][3] = 1.0f;
    
    return view;
}

Matrix4f calculateViewMatrix() {
    // Recalculate the camera basis vectors
    Vector3f forward = cameraFront.Normalize();
    Vector3f right = cameraUp.Cross(forward).Normalize();
    // Vector3f right = forward.Cross(cameraUp).Normalize();
    Vector3f up = forward.Cross(right);

    Matrix4f viewMatrix;
    
    // First row
    viewMatrix.m[0][0] = right.x;
    viewMatrix.m[0][1] = right.y;
    viewMatrix.m[0][2] = right.z;
    viewMatrix.m[0][3] = -right.Dot(cameraPos);
    
    // Second row
    viewMatrix.m[1][0] = up.x;
    viewMatrix.m[1][1] = up.y;
    viewMatrix.m[1][2] = up.z;
    viewMatrix.m[1][3] = -up.Dot(cameraPos);
    
    // Third row
    viewMatrix.m[2][0] = forward.x;
    viewMatrix.m[2][1] = forward.y;
    viewMatrix.m[2][2] = forward.z;
    viewMatrix.m[2][3] = -forward.Dot(cameraPos);
    
    // Fourth row
    viewMatrix.m[3][0] = 0.0f;
    viewMatrix.m[3][1] = 0.0f;
    viewMatrix.m[3][2] = 0.0f;
    viewMatrix.m[3][3] = 1.0f;
    
    return viewMatrix;
}
