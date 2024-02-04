#include "widgets/buttons/DimButton.hpp"

namespace chatterino {

DimButton::DimButton(BaseWidget *parent)
    : Button(parent)
{
}

void DimButton::setDim(Dim value)
{
    this->dim_ = value;

    this->invalidateContent();
}

qreal DimButton::currentContentOpacity() const
{
    if (this->dim_ == Dim::None || this->mouseOver())
    {
        return 1;
    }
    if (this->dim_ == Dim::Some)
    {
        return 0.7;
    }

    return 0.15;
}

}  // namespace chatterino
