#pragma once

#include <array>

#include "ConfigDefinition.h"
#include "Transforms.h"
#include "common/MatrixUtils.h"

namespace mods_config::model
{
    /**
     * What the controllers give the tuning of a transform in a frame: how far each thumbstick is pushed, from -1 to 1,
     * and which modifier is held, the offhand grip for a rotation and the offhand A for a scale.
     */
    struct TuningInput
    {
        float primaryX = 0.0f;
        float primaryY = 0.0f;
        float offhandX = 0.0f;
        float offhandY = 0.0f;
        bool rotating = false;
        bool scaling = false;
    };

    /**
     * A transform as Adjust in world tunes it, with the controls of the framework's DebugAdjuster: the sticks move it,
     * and with a modifier held they rotate or scale it. A part its config spec turns off does not change. The sliders'
     * ranges do not stop it, so an author's range cannot trap the object short of where the player can see it belongs.
     * It keeps its numbers unrounded, so a slow push adds up, and its table has them as a file does.
     */
    class TransformTuning
    {
    public:
        TransformTuning(const ValueOptions& options, const TransformTable& table);
        bool update(const TuningInput& input, float seconds);

        const TransformTable& table() const
        {
            return _table;
        }

    private:
        void updateRotation(const TuningInput& input, float seconds);
        void updateScale(const TuningInput& input, float seconds);
        void updatePosition(const TuningInput& input, float seconds);

        // the parts that are on
        bool _position = false;
        bool _rotation = false;
        bool _scale = false;

        // what is tuned, unrounded
        std::array<float, 3> _tunedPosition{};
        RE::NiMatrix3 _tunedRotation;
        float _tunedScale = 1.0f;

        TransformTable _table;
    };
}
