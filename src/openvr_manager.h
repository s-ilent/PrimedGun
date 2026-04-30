#pragma once
#include <openvr.h>
#include <string>
#include <cmath>
#include <array>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Pose {
    float px, py, pz;
    float qx, qy, qz, qw;
    float trigger;
    float stick_x;
    float stick_y;
    int stick_axis;
    std::array<float, vr::k_unControllerStateAxisCount> axis_x = {};
    std::array<float, vr::k_unControllerStateAxisCount> axis_y = {};
    bool  valid = false;
};

struct Matrix3x4 {
    float m[12];
    float& at(int row, int col) { return m[row * 4 + col]; }
    const float& at(int row, int col) const { return m[row * 4 + col]; }
};

class OpenVRManager {
public:
    ~OpenVRManager() { shutdown(); }

    bool init() {
        vr::EVRInitError err = vr::VRInitError_None;
        system_ = vr::VR_Init(&err, vr::VRApplication_Background);
        if (err != vr::VRInitError_None) {
            system_ = nullptr;
            status_ = "VR_Init failed: " + std::string(vr::VR_GetVRInitErrorAsEnglishDescription(err));
            return false;
        }
        status_ = "Connected";
        return true;
    }

    void shutdown() {
        if (system_) {
            vr::VR_Shutdown();
            system_ = nullptr;
        }
        status_ = "Disconnected";
    }

    bool is_connected() const { return system_ != nullptr; }
    const std::string& status() const { return status_; }

    bool get_latest_poses(bool right_hand, Pose& controller, Pose& left_controller, Pose& hmd) {
        controller = {};
        left_controller = {};
        hmd = {};
        if (!system_) return false;

        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
        system_->GetDeviceToAbsoluteTrackingPose(
            vr::TrackingUniverseStanding, 0, poses, vr::k_unMaxTrackedDeviceCount);

        auto make_pose = [&](vr::TrackedDeviceIndex_t idx, Pose& out) -> bool {
            if (idx == vr::k_unTrackedDeviceIndexInvalid)
                return false;

            auto& pose = poses[idx];
            if (!pose.bPoseIsValid || !pose.bDeviceIsConnected)
                return false;

            auto& m = pose.mDeviceToAbsoluteTracking.m;
            out = {};
            out.px = m[0][3];
            out.py = m[1][3];
            out.pz = m[2][3];

            float trace = m[0][0] + m[1][1] + m[2][2];
            if (trace > 0) {
                float s = 0.5f / sqrtf(trace + 1.0f);
                out.qw = 0.25f / s;
                out.qx = (m[2][1] - m[1][2]) * s;
                out.qy = (m[0][2] - m[2][0]) * s;
                out.qz = (m[1][0] - m[0][1]) * s;
            } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
                float s = 2.0f * sqrtf(1.0f + m[0][0] - m[1][1] - m[2][2]);
                out.qw = (m[2][1] - m[1][2]) / s;
                out.qx = 0.25f * s;
                out.qy = (m[0][1] + m[1][0]) / s;
                out.qz = (m[0][2] + m[2][0]) / s;
            } else if (m[1][1] > m[2][2]) {
                float s = 2.0f * sqrtf(1.0f + m[1][1] - m[0][0] - m[2][2]);
                out.qw = (m[0][2] - m[2][0]) / s;
                out.qx = (m[0][1] + m[1][0]) / s;
                out.qy = 0.25f * s;
                out.qz = (m[1][2] + m[2][1]) / s;
            } else {
                float s = 2.0f * sqrtf(1.0f + m[2][2] - m[0][0] - m[1][1]);
                out.qw = (m[1][0] - m[0][1]) / s;
                out.qx = (m[0][2] + m[2][0]) / s;
                out.qy = (m[1][2] + m[2][1]) / s;
                out.qz = 0.25f * s;
            }

            out.valid = true;
            return true;
        };

        auto find_controller = [&](vr::ETrackedControllerRole role) -> vr::TrackedDeviceIndex_t {
            for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
                if (!system_->IsTrackedDeviceConnected(i))
                    continue;
                if (system_->GetTrackedDeviceClass(i) != vr::TrackedDeviceClass_Controller)
                    continue;
                if (system_->GetControllerRoleForTrackedDeviceIndex(i) == role)
                    return i;
            }
            return vr::k_unTrackedDeviceIndexInvalid;
        };

        auto fill_controller_state = [&](vr::TrackedDeviceIndex_t idx, Pose& out) {
            if (!out.valid || idx == vr::k_unTrackedDeviceIndexInvalid)
                return;

            vr::VRControllerState_t state;
            if (!system_->GetControllerState(idx, &state, sizeof(state)))
                return;

            out.trigger = state.rAxis[1].x;
            for (int axis = 0; axis < vr::k_unControllerStateAxisCount; ++axis) {
                out.axis_x[axis] = state.rAxis[axis].x;
                out.axis_y[axis] = state.rAxis[axis].y;
            }
            out.stick_x = state.rAxis[0].x;
            out.stick_y = state.rAxis[0].y;
            out.stick_axis = 0;
            float best_mag_sq = out.stick_x * out.stick_x + out.stick_y * out.stick_y;
            for (int axis = 2; axis < vr::k_unControllerStateAxisCount; ++axis) {
                const float x = state.rAxis[axis].x;
                const float y = state.rAxis[axis].y;
                const float mag_sq = x * x + y * y;
                if (mag_sq > best_mag_sq) {
                    best_mag_sq = mag_sq;
                    out.stick_x = x;
                    out.stick_y = y;
                    out.stick_axis = axis;
                }
            }
        };

        const vr::TrackedDeviceIndex_t right_idx = find_controller(vr::TrackedControllerRole_RightHand);
        const vr::TrackedDeviceIndex_t left_idx = find_controller(vr::TrackedControllerRole_LeftHand);
        const vr::TrackedDeviceIndex_t main_idx = right_hand ? right_idx : left_idx;
        const vr::TrackedDeviceIndex_t offhand_idx = right_hand ? left_idx : right_idx;

        make_pose(main_idx, controller);
        fill_controller_state(main_idx, controller);
        make_pose(offhand_idx, left_controller);
        fill_controller_state(offhand_idx, left_controller);
        make_pose(vr::k_unTrackedDeviceIndex_Hmd, hmd);

        if (!hmd.valid) {
            status_ = "Connected - HMD not tracked";
        } else if (!controller.valid) {
            status_ = "Connected - no tracking";
        } else {
            status_ = "Connected";
        }
        return controller.valid || left_controller.valid || hmd.valid;
    }

    Pose get_hmd_pose() {
        if (!system_) return {};

        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
        system_->GetDeviceToAbsoluteTrackingPose(
            vr::TrackingUniverseStanding, 0, poses, vr::k_unMaxTrackedDeviceCount);

        auto& pose = poses[vr::k_unTrackedDeviceIndex_Hmd];
        if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
            status_ = "Connected - HMD not tracked";
            return {};
        }

        auto& m = pose.mDeviceToAbsoluteTracking.m;
        Pose result = {};
        result.px = m[0][3];
        result.py = m[1][3];
        result.pz = m[2][3];

        float trace = m[0][0] + m[1][1] + m[2][2];
        if (trace > 0) {
            float s = 0.5f / sqrtf(trace + 1.0f);
            result.qw = 0.25f / s;
            result.qx = (m[2][1] - m[1][2]) * s;
            result.qy = (m[0][2] - m[2][0]) * s;
            result.qz = (m[1][0] - m[0][1]) * s;
        } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
            float s = 2.0f * sqrtf(1.0f + m[0][0] - m[1][1] - m[2][2]);
            result.qw = (m[2][1] - m[1][2]) / s;
            result.qx = 0.25f * s;
            result.qy = (m[0][1] + m[1][0]) / s;
            result.qz = (m[0][2] + m[2][0]) / s;
        } else if (m[1][1] > m[2][2]) {
            float s = 2.0f * sqrtf(1.0f + m[1][1] - m[0][0] - m[2][2]);
            result.qw = (m[0][2] - m[2][0]) / s;
            result.qx = (m[0][1] + m[1][0]) / s;
            result.qy = 0.25f * s;
            result.qz = (m[1][2] + m[2][1]) / s;
        } else {
            float s = 2.0f * sqrtf(1.0f + m[2][2] - m[0][0] - m[1][1]);
            result.qw = (m[1][0] - m[0][1]) / s;
            result.qx = (m[0][2] + m[2][0]) / s;
            result.qy = (m[1][2] + m[2][1]) / s;
            result.qz = 0.25f * s;
        }

        result.valid = true;
        status_ = "Connected";
        return result;
    }

    Pose get_controller_pose(bool right_hand) {
        if (!system_) return {};

        // Find the controller device index
        vr::TrackedDeviceIndex_t idx = vr::k_unTrackedDeviceIndexInvalid;
        for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
            if (!system_->IsTrackedDeviceConnected(i)) continue;
            auto device_class = system_->GetTrackedDeviceClass(i);
            if (device_class != vr::TrackedDeviceClass_Controller) continue;

            auto role = system_->GetControllerRoleForTrackedDeviceIndex(i);
            if (right_hand && role == vr::TrackedControllerRole_RightHand) { idx = i; break; }
            if (!right_hand && role == vr::TrackedControllerRole_LeftHand)  { idx = i; break; }
        }

        if (idx == vr::k_unTrackedDeviceIndexInvalid) {
            status_ = "Connected - controller not found";
            return {};
        }

        // Get poses
        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
        system_->GetDeviceToAbsoluteTrackingPose(
            vr::TrackingUniverseStanding, 0, poses, vr::k_unMaxTrackedDeviceCount);

        auto& pose = poses[idx];
        if (!pose.bPoseIsValid || !pose.bDeviceIsConnected) {
            status_ = "Connected - controller not tracked";
            return {};
        }

        // Extract position and rotation from 3x4 matrix
        auto& m = pose.mDeviceToAbsoluteTracking.m;
        Pose result = {};
        result.px = m[0][3];
        result.py = m[1][3];
        result.pz = m[2][3];

        // Convert rotation matrix to quaternion
        float trace = m[0][0] + m[1][1] + m[2][2];
        if (trace > 0) {
            float s = 0.5f / sqrtf(trace + 1.0f);
            result.qw = 0.25f / s;
            result.qx = (m[2][1] - m[1][2]) * s;
            result.qy = (m[0][2] - m[2][0]) * s;
            result.qz = (m[1][0] - m[0][1]) * s;
        } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
            float s = 2.0f * sqrtf(1.0f + m[0][0] - m[1][1] - m[2][2]);
            result.qw = (m[2][1] - m[1][2]) / s;
            result.qx = 0.25f * s;
            result.qy = (m[0][1] + m[1][0]) / s;
            result.qz = (m[0][2] + m[2][0]) / s;
        } else if (m[1][1] > m[2][2]) {
            float s = 2.0f * sqrtf(1.0f + m[1][1] - m[0][0] - m[2][2]);
            result.qw = (m[0][2] - m[2][0]) / s;
            result.qx = (m[0][1] + m[1][0]) / s;
            result.qy = 0.25f * s;
            result.qz = (m[1][2] + m[2][1]) / s;
        } else {
            float s = 2.0f * sqrtf(1.0f + m[2][2] - m[0][0] - m[1][1]);
            result.qw = (m[1][0] - m[0][1]) / s;
            result.qx = (m[0][2] + m[2][0]) / s;
            result.qy = (m[1][2] + m[2][1]) / s;
            result.qz = 0.25f * s;
        }

        // Get trigger value
        vr::VRControllerState_t state;
        system_->GetControllerState(idx, &state, sizeof(state));
        // Trigger axis is axis 1 on most controllers
        result.trigger = state.rAxis[1].x;
        for (int axis = 0; axis < vr::k_unControllerStateAxisCount; ++axis) {
            result.axis_x[axis] = state.rAxis[axis].x;
            result.axis_y[axis] = state.rAxis[axis].y;
        }
        result.stick_x = state.rAxis[0].x;
        result.stick_y = state.rAxis[0].y;
        result.stick_axis = 0;
        float best_mag_sq = result.stick_x * result.stick_x + result.stick_y * result.stick_y;
        for (int axis = 2; axis < vr::k_unControllerStateAxisCount; ++axis) {
            const float x = state.rAxis[axis].x;
            const float y = state.rAxis[axis].y;
            const float mag_sq = x * x + y * y;
            if (mag_sq > best_mag_sq) {
                best_mag_sq = mag_sq;
                result.stick_x = x;
                result.stick_y = y;
                result.stick_axis = axis;
            }
        }

        result.valid = true;
        status_ = "Connected";
        return result;
    }

    // Convert OpenVR pose to Metroid Prime 3x4 matrix
    // OpenVR: right=+X, up=+Y, forward=-Z
    // Prime:  right=+X, up=+Z, forward=-Y
    static Matrix3x4 pose_to_prime_matrix(
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

        // OpenVR coordinate system: right=+X, up=+Y, forward=-Z (right-handed)
        // Metroid Prime GCN: right=+X, up=+Z, forward=+Y (left-handed Z-up)
        // Mapping: Prime_X=OpenVR_X, Prime_Y=-OpenVR_Z, Prime_Z=OpenVR_Y
        float qx=pose.qx, qy=pose.qy, qz=pose.qz, qw=pose.qw;

        // Apply a fixed local controller-to-gun alignment first.
        // This is the correct layer for the "controller points about 45 degrees
        // downward in game" issue: the controller grip pose and the gun's
        // neutral pose simply don't line up.
        // Built-in neutral gun alignment: UI pitch offset 0 means an internal
        // -43 degree controller-to-gun pitch correction (-90 + 47).
        const float local_pitch = (-43.0f + rx_deg) * static_cast<float>(M_PI / 180.0);
        const float local_yaw   = ry_deg * static_cast<float>(M_PI / 180.0);
        const float local_roll  = rz_deg * static_cast<float>(M_PI / 180.0);

        const float hp = local_pitch * 0.5f;
        const float hy = local_yaw * 0.5f;
        const float hr = local_roll * 0.5f;

        const float sx = sinf(hp), cx = cosf(hp);
        const float sy = sinf(hy), cy = cosf(hy);
        const float sz = sinf(hr), cz = cosf(hr);

        // Local correction quaternion in controller space: yaw * pitch * roll.
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

        // Full rotation matrix from quaternion
        float r00=1-2*(qy*qy+qz*qz), r01=2*(qx*qy-qw*qz), r02=2*(qx*qz+qw*qy);
        float r10=2*(qx*qy+qw*qz),   r11=1-2*(qx*qx+qz*qz), r12=2*(qy*qz-qw*qx);
        float r20=2*(qx*qz-qw*qy),   r21=2*(qy*qz+qw*qx), r22=1-2*(qx*qx+qy*qy);

        Matrix3x4 mat = {};
        // Row 0: right vector. Flip only this mapped axis so the downstream
        // world transform keeps the expected -1 when the controller faces forward.
        mat.at(0,0)=-r00; mat.at(0,1)= r02; mat.at(0,2)=-r01;
        // The gun's local forward/up axes are swapped relative to the current
        // output basis, so map controller forward into Prime forward and
        // controller up into Prime up here. Swapping two basis rows flips
        // handedness, so negate the new forward row to keep this a proper
        // rotation instead of a mirror ("inside out").
        mat.at(1,0)= r20; mat.at(1,1)=-r22; mat.at(1,2)= r21;
        mat.at(2,0)= r10; mat.at(2,1)=-r12; mat.at(2,2)= r11;
        // Translation: X stays, Y=-Z (forward/back fixed), Z=Y (up/down)
        mat.at(0,3)= (pose.px + ox) * scale;
        mat.at(1,3)=-(pose.pz + oz) * scale;
        mat.at(2,3)= (pose.py + oy) * scale;

        return mat;
    }

private:
    vr::IVRSystem* system_ = nullptr;
    std::string    status_ = "Not initialized";
};
