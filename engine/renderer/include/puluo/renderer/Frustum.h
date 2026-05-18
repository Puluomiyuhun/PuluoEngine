#pragma once

#include "puluo/core/Math.h"

namespace Puluo {

// Frustum defined by 6 planes extracted from a View-Projection matrix.
// Uses the Gribb-Hartmann method for fast extraction.
struct Frustum {
    // Each plane stored as Vec4(a, b, c, d) where ax + by + cz + d = 0
    // Normal (a,b,c) points inward (toward the visible region).
    Vec4 planes[6]; // Left, Right, Bottom, Top, Near, Far
    int planeCount = 6; // 5 for reversed-Z infinite projection (no far plane)

    // Extract frustum planes from a View-Projection matrix.
    // Set infiniteFar = true when using reversed-Z infinite far projection,
    // which makes the far plane degenerate and should be skipped.
    static Frustum FromVPMatrix(const Mat4& vp, bool infiniteFar = false) {
        Frustum f;

        // Left:   row3 + row0
        f.planes[0] = Vec4(
            vp[0][3] + vp[0][0],
            vp[1][3] + vp[1][0],
            vp[2][3] + vp[2][0],
            vp[3][3] + vp[3][0]
        );
        // Right:  row3 - row0
        f.planes[1] = Vec4(
            vp[0][3] - vp[0][0],
            vp[1][3] - vp[1][0],
            vp[2][3] - vp[2][0],
            vp[3][3] - vp[3][0]
        );
        // Bottom: row3 + row1
        f.planes[2] = Vec4(
            vp[0][3] + vp[0][1],
            vp[1][3] + vp[1][1],
            vp[2][3] + vp[2][1],
            vp[3][3] + vp[3][1]
        );
        // Top:    row3 - row1
        f.planes[3] = Vec4(
            vp[0][3] - vp[0][1],
            vp[1][3] - vp[1][1],
            vp[2][3] - vp[2][1],
            vp[3][3] - vp[3][1]
        );
        // Near:   row3 + row2
        f.planes[4] = Vec4(
            vp[0][3] + vp[0][2],
            vp[1][3] + vp[1][2],
            vp[2][3] + vp[2][2],
            vp[3][3] + vp[3][2]
        );
        // Far:    row3 - row2
        f.planes[5] = Vec4(
            vp[0][3] - vp[0][2],
            vp[1][3] - vp[1][2],
            vp[2][3] - vp[2][2],
            vp[3][3] - vp[3][2]
        );

        // With reversed-Z infinite far, the far plane is degenerate — skip it
        f.planeCount = infiniteFar ? 5 : 6;

        // Normalize all planes
        for (int i = 0; i < f.planeCount; i++) {
            float len = glm::length(Vec3(f.planes[i]));
            if (len > 0.0f)
                f.planes[i] /= len;
        }

        return f;
    }

    // Test if an AABB is at least partially inside the frustum.
    // Returns true if visible (inside or intersecting), false if completely outside.
    bool TestAABB(const AABB& aabb) const {
        for (int i = 0; i < planeCount; i++) {
            Vec3 normal(planes[i]);
            float d = planes[i].w;

            // Find the "positive vertex" — the corner most in the direction of the normal
            Vec3 pVertex(
                (normal.x >= 0.0f) ? aabb.max.x : aabb.min.x,
                (normal.y >= 0.0f) ? aabb.max.y : aabb.min.y,
                (normal.z >= 0.0f) ? aabb.max.z : aabb.min.z
            );

            // If the positive vertex is outside this plane, the entire AABB is outside
            if (glm::dot(normal, pVertex) + d < 0.0f)
                return false;
        }
        return true;
    }

    // Test if a bounding sphere is at least partially inside the frustum.
    bool TestSphere(const Vec3& center, float radius) const {
        for (int i = 0; i < planeCount; i++) {
            Vec3 normal(planes[i]);
            float d = planes[i].w;
            if (glm::dot(normal, center) + d < -radius)
                return false;
        }
        return true;
    }
};

} // namespace Puluo
