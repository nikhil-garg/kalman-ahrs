#include "ahrs/ahrs.h"

#include <chrono>
#include <cmath>

#include "ahrs/numeric.h"

namespace ahrs {

static inline double to_deg(double rad) noexcept { return rad * 180 / M_PI; }

static double calc_roll(const sensor_readout acc) {
    return std::atan2(acc.y, sqrt(std::pow(acc.x, 2) + std::pow(acc.z, 2)));
}

static double calc_pitch(const sensor_readout acc) {
    return std::atan2(-acc.x, sqrt(std::pow(acc.y, 2) + std::pow(acc.z, 2)));
}

static double calc_yaw(const double roll, const double pitch,
                       const sensor_readout mag) {
    auto horizon_plane_x = mag.x * cos(pitch) + mag.y * sin(pitch) * sin(roll) +
                           mag.z * sin(pitch) * cos(roll);
    auto horizon_plane_y = mag.y * cos(roll) - mag.z * sin(roll);

    return std::atan2(-horizon_plane_y, horizon_plane_x);
}

static inline double get_roll_from_state_vector(
    const ahrs::array_2d<double, 4, 1>& sv) noexcept {
    return sv[0][0];
}

static inline double get_pitch_from_state_vector(
    const ahrs::array_2d<double, 4, 1>& sv) noexcept {
    return sv[0][2];
}

static inline array_2d<double, 4, 4> make_A(
    const std::chrono::duration<double> dt) {
    double dtc = dt.count();
    return {{{1.0, -dtc,  0.0,  0.0},
             {0.0,  1.0,  0.0,  0.0},
             {0.0,  0.0,  1.0, -dtc},
             {0.0,  0.0,  0.0,  1.0}}};
}

static inline array_2d<double, 4, 2> make_B(
    const std::chrono::duration<double> dt) {
    double dtc = dt.count();
    return {{{dtc, 0.0}, {0.0, 0.0}, {0.0, dtc}, {0.0, 0.0}}};
}

static inline array_2d<double, 2, 4> make_H() {
    return {{{1.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0}}};
}

Ahrs::Ahrs(Sensor& gyro, Sensor& acc, Sensor& mag,
           const std::chrono::duration<double> dt)
    : gyro{gyro},
      acc{acc},
      mag{mag},
      kalman{make_A(dt),
             make_B(dt),
             make_H()} {}

void Ahrs::calibrate_imu(const size_t num_of_samples) {
    gyro.calibrate_bias(num_of_samples);
    acc.calibrate_bias(num_of_samples);
}

void Ahrs::calibrate_mag(const size_t num_of_samples) {
    mag.calibrate_bias(num_of_samples);
}

void Ahrs::set_dt(const std::chrono::duration<double> dt) noexcept {
    kalman.A[0][1] = -dt.count();
    kalman.A[2][3] = -dt.count();
    kalman.B[0][0] = dt.count();
    kalman.B[2][1] = dt.count();
}

void Ahrs::set_P_diagonal(const double val) noexcept {
    kalman.set_P_diagonal(val);
}

void Ahrs::set_Q_diagonal(const double val) noexcept {
    kalman.set_Q_diagonal(val);
}

void Ahrs::set_R_diagonal(const double val) noexcept {
    kalman.set_R_diagonal(val);
}

sensor_readout Ahrs::update() {
    auto gr = gyro.read();
    auto ar = acc.read();
    auto mr = mag.read();

    auto system_input_vector = calc_euler_angles_rates(gr);
    auto estimate_vector = calc_estimate(ar);

    auto state_vector = kalman.update(system_input_vector, estimate_vector);

    auto roll = get_roll_from_state_vector(state_vector);
    auto pitch = get_pitch_from_state_vector(state_vector);
    auto yaw = calc_yaw(roll, pitch, mr);

    state = {to_deg(roll), to_deg(pitch), to_deg(yaw)};
    return state;
}

ahrs::array_2d<double, 2, 1> Ahrs::calc_euler_angles_rates(
    sensor_readout gyro_read) const {
    auto roll_rate = gyro_read.x + sin(state.x) * tan(state.y) * gyro_read.y +
                     cos(state.x) * tan(state.y) * gyro_read.z;
    auto pitch_rate = cos(state.x) * gyro_read.y - sin(state.x) * gyro_read.z;

    return {{{roll_rate}, {pitch_rate}}};
}

ahrs::array_2d<double, 2, 1> Ahrs::calc_estimate(
    sensor_readout acc_read) const {
    auto roll_estimate = calc_roll(acc_read);
    auto pitch_estitmate = calc_pitch(acc_read);

    return {{{roll_estimate}, {pitch_estitmate}}};
}

sensor_readout Ahrs::update(const std::chrono::duration<double> dt) {
    set_dt(dt);
    return update();
}

}  // namespace ahrs
