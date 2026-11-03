#include "TransformTuning.h"

#include <algorithm>
#include <cmath>

namespace
{
    using namespace mods_config;

    // a stick does nothing until it is pushed this far, so a hand that rests on it moves nothing
    constexpr float DEADZONE = 0.5f;

    // the speeds at a full push, per second: units, degrees and a scale. Per second and not per frame, as a headset's
    // refresh rate differs: they are the DebugAdjuster's at 90 frames a second
    constexpr float MOVE_SPEED = 13.5f;
    constexpr float TURN_SPEED = 22.5f;
    constexpr float SCALE_SPEED = 0.225f;

    // the scale a tuning stops at, as a transform of no size cannot be seen to tune it back
    constexpr float LEAST_SCALE = 0.05f;

    // the decimals a tuned number has in a table, as the DebugAdjuster saves one
    constexpr float TUNED_NUMBER_PLACE = 100.0f;

    /**
     * How far a stick pushes past its deadzone, from -1 to 1.
     */
    float pushOf(const float stick)
    {
        const float past = (std::abs(stick) - DEADZONE) / (1.0f - DEADZONE);
        return past > 0.0f ? std::copysign((std::min)(past, 1.0f), stick) : 0.0f;
    }

    /**
     * A tuned number as a table has it.
     */
    float rounded(const float number)
    {
        return std::round(number * TUNED_NUMBER_PLACE) / TUNED_NUMBER_PLACE;
    }
}

namespace mods_config::model
{
    /**
     * Starts tuning a table, of the parts that are on in the options of its setting.
     */
    TransformTuning::TransformTuning(const ValueOptions& options, const TransformTable& table)
        : _position(options.transformAxes[TRANSFORM_POSITION].on),
          _rotation(options.transformAxes[TRANSFORM_ROTATION].on),
          _scale(options.transformAxes[TRANSFORM_SCALE].on),
          _tunedScale(axisValue(table, TRANSFORM_SCALE)),
          _table(table)
    {
        for (std::size_t axis = 0; axis < _tunedPosition.size(); ++axis) {
            _tunedPosition[axis] = axisValue(table, TRANSFORM_POSITION + axis);
        }
        _tunedRotation = common::MatrixUtils::getMatrixFromEulerAnglesDegrees(axisValue(table, TRANSFORM_ROTATION),
            axisValue(table, TRANSFORM_ROTATION + 1),
            axisValue(table, TRANSFORM_ROTATION + 2));
    }

    /**
     * Tunes the transform by what the controllers give for a time: the modifier that is held says what the sticks change,
     * the rotation first. A modifier of a part that is off changes nothing, so the transform does not move while the
     * player means to turn it. Returns whether the table changed.
     */
    bool TransformTuning::update(const TuningInput& input, const float seconds)
    {
        const auto before = _table;
        if (input.rotating) {
            updateRotation(input, seconds);
        } else if (input.scaling) {
            updateScale(input, seconds);
        } else {
            updatePosition(input, seconds);
        }
        return _table != before;
    }

    /**
     * Turns the transform: the primary stick's Y by its heading and its X by its attitude, and the offhand stick's X by
     * its roll. The turn is put before the rotation in the matrix, so it is around fixed axes, and a stick can change all
     * three angles.
     */
    void TransformTuning::updateRotation(const TuningInput& input, const float seconds)
    {
        const float heading = -pushOf(input.primaryY);
        const float roll = pushOf(input.offhandX);
        const float attitude = pushOf(input.primaryX);
        if (!_rotation || (heading == 0.0f && roll == 0.0f && attitude == 0.0f)) {
            return;
        }
        const float turn = TURN_SPEED * seconds;
        _tunedRotation = common::MatrixUtils::getMatrixFromEulerAnglesDegrees(heading * turn, roll * turn, attitude * turn) * _tunedRotation;
        std::array<float, 3> angles{};
        common::MatrixUtils::getEulerAnglesFromMatrixDegrees(_tunedRotation, &angles[0], &angles[1], &angles[2]);
        for (std::size_t axis = 0; axis < angles.size(); ++axis) {
            setAxis(_table, TRANSFORM_ROTATION + axis, rounded(angles[axis]));
        }
    }

    /**
     * Scales the transform by the primary stick's Y.
     */
    void TransformTuning::updateScale(const TuningInput& input, const float seconds)
    {
        const float push = pushOf(input.primaryY);
        if (!_scale || push == 0.0f) {
            return;
        }
        _tunedScale = (std::max)(LEAST_SCALE, _tunedScale + push * SCALE_SPEED * seconds);
        setAxis(_table, TRANSFORM_SCALE, rounded(_tunedScale));
    }

    /**
     * Moves the transform: the primary stick's Y along X and its X along Y, and the offhand stick's Y along Z. Only the
     * axes that move are set, and a part with none has its key left as it is.
     */
    void TransformTuning::updatePosition(const TuningInput& input, const float seconds)
    {
        const std::array pushes{ pushOf(input.primaryY), pushOf(input.primaryX), pushOf(input.offhandY) };
        if (!_position) {
            return;
        }
        for (std::size_t axis = 0; axis < pushes.size(); ++axis) {
            if (pushes[axis] != 0.0f) {
                _tunedPosition[axis] += pushes[axis] * MOVE_SPEED * seconds;
                setAxis(_table, TRANSFORM_POSITION + axis, rounded(_tunedPosition[axis]));
            }
        }
    }
}
