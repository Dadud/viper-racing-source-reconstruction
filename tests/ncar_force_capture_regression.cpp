#include "viper/ncar_state.hpp"
#include "viper/vehicle_setup.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>

namespace {

struct CapturedForceWrite {
    std::array<float, 3> force;
    std::array<float, 3> point;
    std::array<float, 3> center;
    std::array<float, 3> before_force;
    std::array<float, 3> before_moment;
    std::array<float, 3> after_force;
    std::array<float, 3> after_moment;
};

struct CapturedSuspensionForceComposition {
    float caller_magnitude_local;
    float wheel_response_0x178;
    float caller_direction_x;
    float force_x;
};

struct CapturedTireForceWrite {
    std::size_t integrator;
    std::size_t wheel;
    std::array<float, 3> force;
    std::array<float, 3> tire_vector_0x100;
    std::array<float, 3> tire_response_vector_0xf4;
    std::array<float, 3> wheel_basis_0x110;
    std::array<float, 3> point;
    std::array<float, 3> center;
    std::array<float, 3> before_force;
    std::array<float, 3> before_moment;
    std::array<float, 3> after_force;
    std::array<float, 3> after_moment;
};

// The first captured PCAR update, wrapper return 0x43113F. The native
// caller's stack is sampled at 0x43E6F0 entry: +0x38 is the clamped force
// magnitude local, and +0x7C is its X direction multiplier. The wheel's
// +0x178 response and emitted force X are captured independently. Source:
// external pcar_wheel_response_input_probe_20261025.json,
// SHA-256 DEC4580293D4BCBAC4B6920597D65D3FBB0D60160D88C8E94F1540887FCA0264,
// REA evidence ev_a0a9aad6b86a89686831f8598ea09008e3e53d216e1718968b9718a5c7822.
constexpr std::array<CapturedSuspensionForceComposition, 4>
    captured_suspension_force_compositions{{
        {3807.7324F, 3807.732421875F, 3.0602332e-05F,
         0.11652549356222153F},
        {3988.6409F, 3988.64086914062F, 3.0602332e-05F,
         0.12206171452999115F},
        {4167.5356F, 4167.53564453125F, 3.0602332e-05F,
         0.12753631174564362F},
        {4008.4482F, 4008.4482421875F, 3.0602332e-05F,
         0.12266786396503448F},
    }};

// Eight selected PCAR integrator updates, four wheels each; all rows are
// observed direct 0x443C09 calls derived from the external capture
// pcar_wheel_response_input_probe_20261025.json (SHA-256
// DEC4580293D4BCBAC4B6920597D65D3FBB0D60160D88C8E94F1540887FCA0264).
// The test fixture contains only the values needed to replay known formulas,
// never the raw wheel snapshots or private capture payload.
#include "ncar_force_capture_wheel_samples.inc"

// First fixed-step sample, captured at the original PCAR's 0x43E6F0 writer
// for both nonzero wheel-force callsites on each of its four wheels.
constexpr std::array<CapturedForceWrite, 8> captured_wheel_writes{{
    {{-0.07859688997268677F, 3891.785400390625F, 0.20219586789608002F},
     {78.33381652832031F, -4.0495285987854F, -313.67626953125F},
     {79.27783966064453F, -3.541802167892456F, -312.542236328125F},
     {0.0F, 0.0F, 0.0F},
     {-1.641030192375183F, -0.00029981753323227167F, 5.132870674133301F},
     {-0.07859688997268677F, 3891.785400390625F, 0.20219586789608002F},
     {4411.67041015625F, 0.27970924973487854F, -3668.842529296875F}},
    {{-99.93509674072266F, 1.211816668510437F, 401.23846435546875F},
     {78.33381652832031F, -4.0495285987854F, -313.67626953125F},
     {79.27783966064453F, -3.541802167892456F, -312.542236328125F},
     {-0.07859688997268677F, 3891.785400390625F, 0.20219586789608002F},
     {4411.67041015625F, 0.27970924973487854F, -3668.842529296875F},
     {-100.0136947631836F, 3892.997314453125F, 401.440673828125F},
     {4209.3251953125F, 492.3878173828125F, -3720.726318359375F}},
    {{-0.07885623723268509F, 3904.62744140625F, 0.20286306738853455F},
     {77.84500122070312F, -4.049615859985352F, -312.18994140625F},
     {79.27783966064453F, -3.541802167892456F, -312.542236328125F},
     {-100.0136947631836F, 3892.997314453125F, 401.440673828125F},
     {4208.8857421875F, 492.3877258300781F, -3719.426025390625F},
     {-100.0925521850586F, 7797.625F, 401.6435241699219F},
     {2833.2021484375F, 492.6506042480469F, -9314.166015625F}},
    {{141.06642150878906F, 0.40612733364105225F, -398.7611083984375F},
     {77.84500122070312F, -4.049615859985352F, -312.18994140625F},
     {79.27783966064453F, -3.541802167892456F, -312.542236328125F},
     {-100.0925521850586F, 7797.625F, 401.6435241699219F},
     {2833.2021484375F, 492.6506042480469F, -9314.166015625F},
     {40.97386932373047F, 7798.03125F, 2.882415771484375F},
     {3035.555419921875F, -29.012664794921875F, -9243.1123046875F}},
    {{-0.08245532214641571F, 4082.8388671875F, 0.21212194859981537F},
     {80.6589584350586F, -4.052007675170898F, -312.9249572753906F},
     {79.27783966064453F, -3.541802167892456F, -312.542236328125F},
     {40.97386932373047F, 7798.03125F, 2.882415771484375F},
     {3034.9990234375F, -29.01276397705078F, -9241.4208984375F},
     {40.891414642333984F, 11880.8701171875F, 3.0945377349853516F},
     {4597.478515625F, -29.274171829223633F, -3602.57763671875F}},
    {{-92.79043579101562F, -0.2751481831073761F, 301.4117736816406F},
     {80.6589584350586F, -4.052007675170898F, -312.9249572753906F},
     {79.27783966064453F, -3.541802167892456F, -312.542236328125F},
     {40.891414642333984F, 11880.8701171875F, 3.0945377349853516F},
     {4597.478515625F, -29.274171829223633F, -3602.57763671875F},
     {-51.89902114868164F, 11880.5947265625F, 304.5063171386719F},
     {4443.59130859375F, -410.0467834472656F, -3650.2998046875F}},
    {{-0.0827135443687439F, 4095.624755859375F, 0.2127862423658371F},
     {80.16220092773438F, -4.052095890045166F, -311.4145202636719F},
     {79.27783966064453F, -3.541802167892456F, -312.542236328125F},
     {-51.89902114868164F, 11880.5947265625F, 304.5063171386719F},
     {4442.9814453125F, -410.0469055175781F, -3648.446044921875F},
     {-51.98173522949219F, 15976.2197265625F, 304.7191162109375F},
     {-175.82861328125F, -410.328369140625F, -26.476318359375F}},
    {{108.4566650390625F, -0.2683998644351959F, -308.3384094238281F},
     {80.16220092773438F, -4.052095890045166F, -311.4145202636719F},
     {79.27783966064453F, -3.541802167892456F, -312.542236328125F},
     {-51.98173522949219F, 15976.2197265625F, 304.7191162109375F},
     {-175.82861328125F, -410.328369140625F, -26.476318359375F},
     {56.47492980957031F, 15975.951171875F, -3.619293212890625F},
     {-18.182785034179688F, -15.337493896484375F, 28.63107681274414F}},
}};

void require_close(const std::array<float, 3>& actual,
                   const std::array<float, 3>& expected,
                   const char* message,
                   const float tolerance = 0.02F)
{
    for (std::size_t axis = 0U; axis < actual.size(); ++axis) {
        if (!(std::fabs(actual[axis] - expected[axis]) <= tolerance)) {
            throw std::runtime_error(message);
        }
    }
}

} // namespace

int main()
{
    try {
        for (const auto& sample : captured_wheel_writes) {
            viper::NcarForceMomentAccumulator accumulator{
                sample.before_force, sample.before_moment};
            if (!viper::accumulate_ncar_force_moment(
                    accumulator, sample.force, sample.point, sample.center)) {
                throw std::runtime_error(
                    "captured nonzero native wheel force must be accumulated");
            }
            require_close(accumulator.force_0x24c, sample.after_force,
                "force accumulator differs from the original PCAR capture");
            require_close(accumulator.moment_0x258, sample.after_moment,
                "moment accumulator differs from the original PCAR capture");
        }

        for (const auto& sample : captured_suspension_force_compositions) {
            if (std::fabs(sample.caller_magnitude_local -
                          sample.wheel_response_0x178) > 0.02F) {
                throw std::runtime_error(
                    "captured suspension local must match wheel response +0x178");
            }
            const viper::WheelSuspensionFinalizeInput input{
                sample.caller_magnitude_local,
                0.0F,
                0.0F,
                0.0F,
                0.0F,
                0.0F,
                {sample.caller_direction_x, 0.0F, 0.0F},
                true};
            const auto response = viper::finalize_wheel_suspension(input);
            if (std::fabs(response.applied_force.x - sample.force_x) > 0.02F) {
                throw std::runtime_error(
                    "captured suspension force X differs from native response product");
            }
        }

        static_assert(captured_tire_force_writes.size() == 8U * 4U);
        for (std::size_t index = 0; index < captured_tire_force_writes.size(); ++index) {
            const auto& sample = captured_tire_force_writes[index];
            if (sample.integrator != index / 4U + 1U ||
                sample.wheel != index % 4U) {
                throw std::runtime_error(
                    "captured tire writes must retain eight ordered four-wheel updates");
            }
            if (sample.force != sample.tire_vector_0x100) {
                throw std::runtime_error(
                    "captured 0x443C09 force must equal wheel +0x100 vector");
            }
            const viper::WheelTireBasisUpdateInput basis_input{
                {sample.tire_response_vector_0xf4[0],
                 sample.tire_response_vector_0xf4[1],
                 sample.tire_response_vector_0xf4[2]},
                {sample.wheel_basis_0x110[0], sample.wheel_basis_0x110[1],
                 sample.wheel_basis_0x110[2]},
                {}, 0.0F};
            const auto basis_update =
                viper::apply_wheel_tire_basis_update(basis_input);
            require_close(
                {basis_update.corrected_vector_0x100_0x108.x,
                 basis_update.corrected_vector_0x100_0x108.y,
                 basis_update.corrected_vector_0x100_0x108.z},
                sample.tire_vector_0x100,
                "native tire-basis projection differs from captured +0x100",
                1.0e-4F);
            viper::NcarForceMomentAccumulator accumulator{
                sample.before_force, sample.before_moment};
            if (!viper::accumulate_ncar_force_moment(
                    accumulator, sample.tire_vector_0x100,
                    sample.point, sample.center)) {
                throw std::runtime_error(
                    "captured 0x443C09 tire vector must be accumulated");
            }
            require_close(accumulator.force_0x24c, sample.after_force,
                "tire force accumulator differs from the original capture");
            require_close(accumulator.moment_0x258, sample.after_moment,
                "tire moment accumulator differs from the original capture");
        }

        viper::NcarForceMomentAccumulator zero_accumulator{
            {-100.0136947631836F, 3892.997314453125F, 401.440673828125F},
            {4209.3251953125F, 492.3878173828125F, -3720.726318359375F}};
        const auto before_zero = zero_accumulator;
        if (viper::accumulate_ncar_force_moment(zero_accumulator,
                {0.0F, 0.0F, 0.0F},
                {78.99462890625F, -3.694119930267334F, -312.8824462890625F},
                {79.27783966064453F, -3.541802167892456F, -312.542236328125F}) ||
            zero_accumulator.force_0x24c != before_zero.force_0x24c ||
            zero_accumulator.moment_0x258 != before_zero.moment_0x258) {
            throw std::runtime_error(
                "captured zero-vector wheel call must preserve accumulators");
        }

        std::cout << "Original PCAR wheel force samples match the accumulator model "
                     "across eight integrator updates.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
