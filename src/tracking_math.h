#pragma once

#include <array>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

inline constexpr int kMaxControllerAxes = 5;

struct Pose {
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
    float qx = 0.0f;
    float qy = 0.0f;
    float qz = 0.0f;
    float qw = 1.0f;
    float trigger = 0.0f;
    bool trigger_click = false;
    bool thumbstick_click = false;
    bool button_a = false;
    bool button_b = false;
    float stick_x = 0.0f;
    float stick_y = 0.0f;
    int stick_axis = 0;
    std::array<float, kMaxControllerAxes> axis_x = {};
    std::array<float, kMaxControllerAxes> axis_y = {};
    bool valid = false;
};

struct Matrix3x4 {
    float m[12] = {};
    float& at(int row, int col) { return m[row * 4 + col]; }
    const float& at(int row, int col) const { return m[row * 4 + col]; }
};

inline Matrix3x4 pose_to_prime_matrix(
    const Pose& pose,
    float ox, float oy, float oz,
    float rx_deg, float ry_deg, float rz_deg,
    float scale)
{
    auto quat_mul = [](float ax, float ay, float az, float aw,
                       float bx, float by, float bz, float bw,
                       float& ox, float& oy, float& oz, float& ow) {
        ox = aw * bx + ax * bw + ay * bz - az * by;
        oy = aw * by - ax * bz + ay * bw + az * bx;
        oz = aw * bz + ax * by - ay * bx + az * bw;
        ow = aw * bw - ax * bx - ay * by - az * bz;
    };

    float qx = pose.qx;
    float qy = pose.qy;
    float qz = pose.qz;
    float qw = pose.qw;

    const float local_pitch = rx_deg * static_cast<float>(M_PI / 180.0);
    const float local_yaw   = ry_deg * static_cast<float>(M_PI / 180.0);
    const float local_roll  = rz_deg * static_cast<float>(M_PI / 180.0);

    const float hp = local_pitch * 0.5f;
    const float hy = local_yaw * 0.5f;
    const float hr = local_roll * 0.5f;

    const float sx = sinf(hp), cx = cosf(hp);
    const float sy = sinf(hy), cy = cosf(hy);
    const float sz = sinf(hr), cz = cosf(hr);

    float cq_x, cq_y, cq_z, cq_w;
    quat_mul(0.0f, sy, 0.0f, cy,
             sx, 0.0f, 0.0f, cx,
             cq_x, cq_y, cq_z, cq_w);
    float cr_x, cr_y, cr_z, cr_w;
    quat_mul(cq_x, cq_y, cq_z, cq_w,
             0.0f, 0.0f, sz, cz,
             cr_x, cr_y, cr_z, cr_w);

    float corrected_x, corrected_y, corrected_z, corrected_w;
    quat_mul(qx, qy, qz, qw,
             cr_x, cr_y, cr_z, cr_w,
             corrected_x, corrected_y, corrected_z, corrected_w);

    qx = corrected_x;
    qy = corrected_y;
    qz = corrected_z;
    qw = corrected_w;

    const float r00 = 1 - 2 * (qy * qy + qz * qz);
    const float r01 = 2 * (qx * qy - qw * qz);
    const float r02 = 2 * (qx * qz + qw * qy);
    const float r10 = 2 * (qx * qy + qw * qz);
    const float r11 = 1 - 2 * (qx * qx + qz * qz);
    const float r12 = 2 * (qy * qz - qw * qx);
    const float r20 = 2 * (qx * qz - qw * qy);
    const float r21 = 2 * (qy * qz + qw * qx);
    const float r22 = 1 - 2 * (qx * qx + qy * qy);

    Matrix3x4 mat = {};
    mat.at(0,0) = -r00; mat.at(0,1) =  r02; mat.at(0,2) = -r01;
    mat.at(1,0) =  r20; mat.at(1,1) = -r22; mat.at(1,2) =  r21;
    mat.at(2,0) =  r10; mat.at(2,1) = -r12; mat.at(2,2) =  r11;
    mat.at(0,3) =  (pose.px + ox) * scale;
    mat.at(1,3) = -(pose.pz + oz) * scale;
    mat.at(2,3) =  (pose.py + oy) * scale;
    return mat;
}
