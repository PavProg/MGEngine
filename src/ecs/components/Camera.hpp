#pragma once
#include <cmath>
enum class ProjectionType { Perspective, Orthographic };
struct Camera {
    ProjectionType projection = ProjectionType::Perspective;
    float fovYDegrees = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 200.0f;
    float orthoSize = 10.0f;
    bool IsValid() const {
        return std::isfinite(nearPlane) && std::isfinite(farPlane) && nearPlane > 0 && farPlane > nearPlane
            && std::isfinite(fovYDegrees) && fovYDegrees > 0 && fovYDegrees < 180
            && std::isfinite(orthoSize) && orthoSize > 0;
    }
};
