#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>

#include "vischeck/vis_types.h"

namespace vis {

struct Ray {
    Vec3 origin{};
    Vec3 dir{};
    Vec3 inv_dir{};
};

inline Vec3 operator+(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 operator-(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 operator*(const Vec3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline float length(const Vec3& v) {
    return std::sqrt(dot(v, v));
}

inline Vec3 normalize(const Vec3& v) {
    const float len = length(v);
    if (len <= 1e-8f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return v * (1.0f / len);
}

inline Aabb make_empty_aabb() {
    return {
        {FLT_MAX, FLT_MAX, FLT_MAX},
        {-FLT_MAX, -FLT_MAX, -FLT_MAX},
    };
}

inline bool is_empty(const Aabb& box) {
    return box.min.x > box.max.x || box.min.y > box.max.y || box.min.z > box.max.z;
}

inline Aabb union_aabb(const Aabb& a, const Aabb& b) {
    if (is_empty(a)) {
        return b;
    }
    if (is_empty(b)) {
        return a;
    }
    return {
        {
            std::min(a.min.x, b.min.x),
            std::min(a.min.y, b.min.y),
            std::min(a.min.z, b.min.z),
        },
        {
            std::max(a.max.x, b.max.x),
            std::max(a.max.y, b.max.y),
            std::max(a.max.z, b.max.z),
        },
    };
}

inline Aabb grow_aabb(const Aabb& box, const Vec3& p) {
    if (is_empty(box)) {
        return {p, p};
    }
    return {
        {
            std::min(box.min.x, p.x),
            std::min(box.min.y, p.y),
            std::min(box.min.z, p.z),
        },
        {
            std::max(box.max.x, p.x),
            std::max(box.max.y, p.y),
            std::max(box.max.z, p.z),
        },
    };
}

inline Vec3 centroid(const Aabb& box) {
    return {
        (box.min.x + box.max.x) * 0.5f,
        (box.min.y + box.max.y) * 0.5f,
        (box.min.z + box.max.z) * 0.5f,
    };
}

inline Ray make_ray(const Vec3& start, const Vec3& end) {
    const Vec3 dir = normalize(end - start);
    const float safe_x = std::abs(dir.x) < 1e-8f ? (dir.x >= 0.0f ? 1e-8f : -1e-8f) : dir.x;
    const float safe_y = std::abs(dir.y) < 1e-8f ? (dir.y >= 0.0f ? 1e-8f : -1e-8f) : dir.y;
    const float safe_z = std::abs(dir.z) < 1e-8f ? (dir.z >= 0.0f ? 1e-8f : -1e-8f) : dir.z;
    return {
        start,
        dir,
        {1.0f / safe_x, 1.0f / safe_y, 1.0f / safe_z},
    };
}

inline bool ray_aabb_intersect(const Ray& ray, const Aabb& box, float t_max, float* out_t_near = nullptr) {
    float tmin = 0.0f;
    float tmax = t_max;

    const auto slab = [&](float origin, float inv_dir, float bmin, float bmax) {
        float t1 = (bmin - origin) * inv_dir;
        float t2 = (bmax - origin) * inv_dir;
        if (t1 > t2) {
            std::swap(t1, t2);
        }
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
    };

    slab(ray.origin.x, ray.inv_dir.x, box.min.x, box.max.x);
    slab(ray.origin.y, ray.inv_dir.y, box.min.y, box.max.y);
    slab(ray.origin.z, ray.inv_dir.z, box.min.z, box.max.z);

    if (tmax >= tmin && tmax >= 0.0f) {
        if (out_t_near != nullptr) {
            *out_t_near = tmin;
        }
        return true;
    }
    return false;
}

inline bool ray_triangle_intersect(
    const Ray& ray,
    const Vec3& v0,
    const Vec3& v1,
    const Vec3& v2,
    float t_max,
    float epsilon,
    float* out_t) {
    const Vec3 e1 = v1 - v0;
    const Vec3 e2 = v2 - v0;
    const Vec3 pvec = cross(ray.dir, e2);
    const float det = dot(e1, pvec);
    if (std::abs(det) < 1e-8f) {
        return false;
    }
    const float inv_det = 1.0f / det;
    const Vec3 tvec = ray.origin - v0;
    const float u = dot(tvec, pvec) * inv_det;
    if (u < -epsilon || u > 1.0f + epsilon) {
        return false;
    }
    const Vec3 qvec = cross(tvec, e1);
    const float v = dot(ray.dir, qvec) * inv_det;
    if (v < -epsilon || (u + v) > 1.0f + epsilon) {
        return false;
    }
    const float t = dot(e2, qvec) * inv_det;
    if (t < 0.0f || t > t_max) {
        return false;
    }
    if (out_t != nullptr) {
        *out_t = t;
    }
    return true;
}

inline Vec3 transform_point(const Mat3x4& m, const Vec3& p) {
    return {
        m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2] * p.z + m.m[0][3],
        m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2] * p.z + m.m[1][3],
        m.m[2][0] * p.x + m.m[2][1] * p.y + m.m[2][2] * p.z + m.m[2][3],
    };
}

inline Vec3 inverse_rotate(const Mat3x4& m, const Vec3& v) {
    return {
        m.m[0][0] * v.x + m.m[1][0] * v.y + m.m[2][0] * v.z,
        m.m[0][1] * v.x + m.m[1][1] * v.y + m.m[2][1] * v.z,
        m.m[0][2] * v.x + m.m[1][2] * v.y + m.m[2][2] * v.z,
    };
}

inline Vec3 inverse_transform_point(const Mat3x4& m, const Vec3& p) {
    const Vec3 t{m.m[0][3], m.m[1][3], m.m[2][3]};
    return inverse_rotate(m, p - t);
}

inline Aabb transform_aabb(const Mat3x4& m, const Aabb& local) {
    std::array<Vec3, 8> corners = {
        Vec3{local.min.x, local.min.y, local.min.z},
        Vec3{local.min.x, local.min.y, local.max.z},
        Vec3{local.min.x, local.max.y, local.min.z},
        Vec3{local.min.x, local.max.y, local.max.z},
        Vec3{local.max.x, local.min.y, local.min.z},
        Vec3{local.max.x, local.min.y, local.max.z},
        Vec3{local.max.x, local.max.y, local.min.z},
        Vec3{local.max.x, local.max.y, local.max.z},
    };
    Aabb world = make_empty_aabb();
    for (const Vec3& c : corners) {
        world = grow_aabb(world, transform_point(m, c));
    }
    return world;
}

inline bool ray_obb_intersect(const Ray& world_ray, const Mat3x4& world_from_local, const Aabb& local_bounds, float t_max, float* out_t) {
    const Vec3 local_origin = inverse_transform_point(world_from_local, world_ray.origin);
    const Vec3 local_dir = inverse_rotate(world_from_local, world_ray.dir);
    const float safe_x = std::abs(local_dir.x) < 1e-8f ? (local_dir.x >= 0.0f ? 1e-8f : -1e-8f) : local_dir.x;
    const float safe_y = std::abs(local_dir.y) < 1e-8f ? (local_dir.y >= 0.0f ? 1e-8f : -1e-8f) : local_dir.y;
    const float safe_z = std::abs(local_dir.z) < 1e-8f ? (local_dir.z >= 0.0f ? 1e-8f : -1e-8f) : local_dir.z;
    const Ray local_ray{
        local_origin,
        local_dir,
        {1.0f / safe_x, 1.0f / safe_y, 1.0f / safe_z},
    };
    return ray_aabb_intersect(local_ray, local_bounds, t_max, out_t);
}

} // namespace vis
