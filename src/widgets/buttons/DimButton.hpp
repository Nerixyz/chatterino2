#pragma once

#include "widgets/buttons/Button.hpp"

namespace chatterino {

/// @brief A button with a #dim() setting for controlling the opacity of its
/// content.
///
/// This button doesn't paint anything.
///
/// @sa #currentContentOpacity()
class DimButton : public Button
{
public:
    enum class Dim {
        /// Fully opaque (100% opcaity)
        None,
        /// Slightly transparent (70% opacity)
        Some,
        /// Almost transparent (15% opacity)
        Lots,
    };

    DimButton(BaseWidget *parent = nullptr);

    /// Returns the current dim level.
    [[nodiscard]] Dim dim() const
    {
        return this->dim_;
    }

    /// Returns the current opacity based on the current dim level.
    [[nodiscard]] qreal currentContentOpacity() const;

    /// Setter for #dim()
    void setDim(Dim value);

private:
    Dim dim_ = Dim::Some;
};

}  // namespace chatterino
