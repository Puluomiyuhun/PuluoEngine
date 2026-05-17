#pragma once

// Re-export glm with common extensions
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <limits>

namespace Puluo {

    using Vec2 = glm::vec2;
    using Vec3 = glm::vec3;
    using Vec4 = glm::vec4;
    using Mat3 = glm::mat3;
    using Mat4 = glm::mat4;
    using Quat = glm::quat;

    struct AABB {
        Vec3 min{std::numeric_limits<float>::max()};
        Vec3 max{std::numeric_limits<float>::lowest()};

        void Expand(const Vec3& point) {
            min = glm::min(min, point);
            max = glm::max(max, point);
        }

        void Merge(const AABB& other) {
            min = glm::min(min, other.min);
            max = glm::max(max, other.max);
        }

        bool IsValid() const {
            return min.x <= max.x && min.y <= max.y && min.z <= max.z;
        }
    };

    // Transform an AABB by a model matrix, producing a new world-space AABB.
    // Uses the 8-corner expansion method (exact for affine transforms).
    inline AABB TransformAABB(const AABB& local, const Mat4& transform) {
        AABB result;
        // Faster method: transform center + extents using abs(matrix) trick
        Vec3 center = (local.min + local.max) * 0.5f;
        Vec3 extent = (local.max - local.min) * 0.5f;

        Vec3 newCenter = Vec3(transform * Vec4(center, 1.0f));

        // For each axis, the new extent is the dot of extents with abs of that row
        Mat3 absMat = Mat3(
            glm::abs(Vec3(transform[0])),
            glm::abs(Vec3(transform[1])),
            glm::abs(Vec3(transform[2]))
        );
        Vec3 newExtent = absMat * extent;

        result.min = newCenter - newExtent;
        result.max = newCenter + newExtent;
        return result;
    }

} // namespace Puluo
