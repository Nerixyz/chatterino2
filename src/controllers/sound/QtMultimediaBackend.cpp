#ifdef CHATTERINO_WITH_QT_MULTIMEDIA

#    include "controllers/sound/QtMultimediaBackend.hpp"

#    include "common/QLogging.hpp"

#    include <QAudioDevice>
#    include <QAudioOutput>
#    include <QMediaDevices>
#    include <QMediaPlayer>

#    include <array>

namespace {
constexpr size_t NUM_SOUNDS = 4;
}  // namespace

namespace chatterino {

class QtMultimediaBackendPrivate : public QObject
{
public:
    QtMultimediaBackendPrivate() = default;

    void play(const QUrl &sound);

private:
    QMediaPlayer *nextPlayer();
    QMediaPlayer *makePlayer();

    void clearPlayers();

    std::array<QMediaPlayer *, NUM_SOUNDS> players{};
    QAudioOutput *audioOutput = nullptr;
    QMediaDevices *mediaDevices = nullptr;
};

QMediaPlayer *QtMultimediaBackendPrivate::nextPlayer()
{
    qint64 maxPos = 0;
    QMediaPlayer *maxPlayer = nullptr;
    for (auto &player : this->players)
    {
        if (!player)
        {
            player = makePlayer();
            return player;
        }
        if (player->playbackState() == QMediaPlayer::PlayingState)
        {
            if (player->position() > maxPos)
            {
                maxPos = player->position();
                maxPlayer = player;
            }
            continue;
        }
        return player;
    }
    return maxPlayer;
}

QMediaPlayer *QtMultimediaBackendPrivate::makePlayer()
{
    if (!this->audioOutput)
    {
        assert(this->mediaDevices == nullptr);
        this->mediaDevices = new QMediaDevices(this);
        this->audioOutput =
            new QAudioOutput(QMediaDevices::defaultAudioOutput(), this);
        QObject::connect(
            this->mediaDevices, &QMediaDevices::audioOutputsChanged, [this] {
                qCDebug(chatterinoSound) << "Audio outputs changed";
                auto nextDefault = QMediaDevices::defaultAudioOutput();
                if (nextDefault == this->audioOutput->device())
                {
                    qCDebug(chatterinoSound) << "Default device unchanged";
                    return;
                }
                qCDebug(chatterinoSound) << "Default device changed";

                this->clearPlayers();
                this->audioOutput->setDevice(nextDefault);
            });
    }
    auto *player = new QMediaPlayer(this);
    player->setAudioOutput(this->audioOutput);
    QObject::connect(player, &QMediaPlayer::errorOccurred,
                     [](QMediaPlayer::Error err, const QString &msg) {
                         qCWarning(chatterinoSound)
                             << "Failed to play sound:" << msg
                             << "code:" << err;
                     });
    return player;
}

void QtMultimediaBackendPrivate::clearPlayers()
{
    for (auto &player : this->players)
    {
        if (!player)
        {
            continue;
        }
        player->deleteLater();
        player = nullptr;
    }
}

void QtMultimediaBackendPrivate::play(const QUrl &sound)
{
    auto *player = this->nextPlayer();
    if (!player)
    {
        qCWarning(chatterinoSound) << "No player found";
        return;
    }
    player->stop();
    if (player->source() != sound)
    {
        player->setSource(sound);
    }
    player->setPosition(0);
    player->play();
}

QtMultimediaBackend::QtMultimediaBackend()
    : private_(std::make_unique<QtMultimediaBackendPrivate>())
{
    qCDebug(chatterinoSound) << "Initialized Qt Multimedia backend";
}

void QtMultimediaBackend::play(const QUrl &sound)
{
    auto resolved = sound;
    if (!resolved.isLocalFile())
    {
        resolved = QUrl("qrc:/sounds/ping2.wav");
    }
    this->private_->play(resolved);
}

}  // namespace chatterino

#endif
