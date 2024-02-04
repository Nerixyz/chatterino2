#include "widgets/buttons/PixmapButton.hpp"

namespace {

// returns a new resized image or the old one if the size didn't change
QPixmap resizePixmap(const QPixmap &current, QPixmap resized, const QSize &size)
{
    if (resized.size() == size)
    {
        return resized;
    }

    return current.scaled(size, Qt::IgnoreAspectRatio,
                          Qt::SmoothTransformation);
}

}  // namespace

namespace chatterino {

PixmapButton::PixmapButton(BaseWidget *parent)
    : DimButton(parent)
{
    this->setContentCacheEnabled(false);
}

void PixmapButton::setPixmap(const QPixmap &pixmap)
{
    this->pixmap_ = pixmap;
    this->resizedPixmap_ = {};
    this->update();
}

void PixmapButton::setMarginEnabled(bool enableMargin)
{
    this->marginEnabled_ = enableMargin;
    this->update();
}

void PixmapButton::paintContent(QPainter &painter)
{
    if (!this->pixmap_.isNull())
    {
        painter.setOpacity(this->currentContentOpacity());

        QRect rect = this->rect();

        int shift = 0;
        if (this->marginEnabled_)
        {
            int margin =
                this->height() < static_cast<int>(22 * this->scale()) ? 3 : 6;

            shift =
                static_cast<int>(static_cast<float>(margin) * this->scale());
        }

        rect.moveLeft(shift);
        rect.setRight(rect.right() - 2 * shift);
        rect.moveTop(shift);
        rect.setBottom(rect.bottom() - 2 * shift);

        this->resizedPixmap_ =
            resizePixmap(this->pixmap_, this->resizedPixmap_, rect.size());

        painter.drawPixmap(rect, this->resizedPixmap_);

        painter.setOpacity(1);
    }
}

}  // namespace chatterino
