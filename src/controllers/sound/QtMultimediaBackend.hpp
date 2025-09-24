#pragma once

#ifdef CHATTERINO_WITH_QT_MULTIMEDIA
#    include "controllers/sound/ISoundController.hpp"

#    include <memory>

namespace chatterino {

class QtMultimediaBackendPrivate;
class QtMultimediaBackend : public ISoundController
{
public:
    QtMultimediaBackend();

    void play(const QUrl &sound) override;

private:
    std::unique_ptr<QtMultimediaBackendPrivate> private_;
};

}  // namespace chatterino

#endif
