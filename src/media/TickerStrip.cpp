#include "media/TickerStrip.h"

#include "core/Log.h"
#include "core/TickerMotion.h"
#include "media/QImageFrame.h"

#include <QFile>
#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QStringDecoder>

#include <algorithm>
#include <cmath>

namespace led {

namespace {

constexpr int kMaxStripLength = 400000; // 메모리 보호용 최대 띠 길이(픽셀)

QString decodeText(const QByteArray& data)
{
    // UTF-8 우선, 깨지면 시스템 기본 인코딩(한국어 Windows: CP949)으로 해석
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString text = utf8.decode(data);
    if (!utf8.hasError())
        return text;
    return QString::fromLocal8Bit(data);
}

struct Phrase {
    QPainterPath path;   // 기준선(baseline) 좌표 0 기준
    double advance = 0;  // 흐르는 방향 길이
    double position = 0; // 띠 안에서의 시작 위치
};

} // namespace

std::vector<std::string> tickerLines(const TickerConfig& config, std::string* error)
{
    std::vector<std::string> out;
    if (!config.textFile.empty()) {
        QFile file(QString::fromStdString(config.textFile));
        if (!file.open(QIODevice::ReadOnly)) {
            if (error)
                *error = "자막 파일을 열 수 없음: " + config.textFile;
            return out;
        }
        const QString text = decodeText(file.readAll());
        for (const QString& line : text.split(QLatin1Char('\n'))) {
            const QString t = line.trimmed();
            if (!t.isEmpty())
                out.push_back(t.toStdString());
        }
        return out;
    }
    for (const std::string& l : config.lines) {
        const QString t = QString::fromStdString(l).trimmed();
        if (!t.isEmpty())
            out.push_back(t.toStdString());
    }
    return out;
}

std::shared_ptr<const TickerStrip> buildTickerStrip(const TickerConfig& config, const std::vector<std::string>& lines,
                                                    int regionWidth, int regionHeight)
{
    if (lines.empty() || regionWidth <= 0 || regionHeight <= 0)
        return nullptr;

    QFont font(QString::fromStdString(config.fontFamily));
    font.setPixelSize(std::max(4, config.fontSize));
    font.setBold(config.bold);
    font.setStyleStrategy(QFont::PreferAntialias);
    const QFontMetricsF metrics(font);
    const double ascent = metrics.ascent();
    const double lineHeight = metrics.ascent() + metrics.descent();

    auto strip = std::make_shared<TickerStrip>();
    strip->vertical = isVerticalTicker(config.direction);
    strip->thickness = strip->vertical ? regionWidth : regionHeight;

    // 문구 배치
    std::vector<Phrase> phrases;
    double cursor = 0.0;
    const double gap = std::max(0, config.gap);
    for (const std::string& line : lines) {
        Phrase p;
        const QString text = QString::fromStdString(line);
        p.path.addText(0, 0, font, text);
        const double width = metrics.horizontalAdvance(text);
        p.advance = strip->vertical ? lineHeight : width;
        p.position = cursor;
        cursor += p.advance + gap;
        phrases.push_back(std::move(p));
    }
    const double totalLength = std::ceil(cursor);
    if (totalLength > kMaxStripLength) {
        logWarning("자막이 너무 깁니다. 문구 수나 글자 크기를 줄이세요 (" +
                   std::to_string(static_cast<long long>(totalLength)) + "px)");
        return nullptr;
    }
    strip->length = std::max(1, static_cast<int>(totalLength));

    const QColor textColor = QColor::fromRgba(config.textColor);
    const QColor outlineColor = QColor::fromRgba(config.outlineColor);
    const QColor shadowColor = QColor::fromRgba(config.shadowColor);
    const double pad = config.outlineWidth + (config.shadow ? config.shadowOffset : 0) + 2.0;

    for (int offset = 0; offset < strip->length; offset += kTickerChunkSize) {
        const int chunkLength = std::min(kTickerChunkSize, strip->length - offset);
        const int w = strip->vertical ? strip->thickness : chunkLength;
        const int h = strip->vertical ? chunkLength : strip->thickness;
        QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
        if (image.isNull()) {
            logError("자막 이미지 메모리 할당 실패");
            return nullptr;
        }
        image.fill(Qt::transparent);

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        for (const Phrase& p : phrases) {
            // 이 조각과 겹치는 문구만 그림
            if (p.position + p.advance + pad < offset || p.position - pad > offset + chunkLength)
                continue;

            double x;
            double y;
            if (strip->vertical) {
                const double width = p.path.boundingRect().width();
                x = (strip->thickness - width) / 2.0 - p.path.boundingRect().left();
                y = p.position - offset + ascent;
            } else {
                x = p.position - offset;
                y = (strip->thickness - lineHeight) / 2.0 + ascent;
            }

            painter.save();
            painter.translate(x, y);
            if (config.shadow) {
                painter.fillPath(p.path.translated(config.shadowOffset, config.shadowOffset), shadowColor);
                if (config.outlineWidth > 0) {
                    QPen shadowPen(shadowColor, config.outlineWidth * 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                    painter.strokePath(p.path.translated(config.shadowOffset, config.shadowOffset), shadowPen);
                }
            }
            if (config.outlineWidth > 0) {
                QPen outlinePen(outlineColor, config.outlineWidth * 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                painter.strokePath(p.path, outlinePen);
            }
            painter.fillPath(p.path, textColor);
            painter.restore();
        }
        painter.end();

        TickerChunk chunk;
        chunk.frame = frameFromQImage(image);
        chunk.offset = offset;
        strip->chunks.push_back(std::move(chunk));
    }
    return strip;
}

std::shared_ptr<const Frame> solidColorFrame(std::uint32_t argb)
{
    auto frame = std::make_shared<Frame>();
    frame->width = 1;
    frame->height = 1;
    frame->stride = 4;
    frame->premultiplied = true;
    const unsigned a = (argb >> 24) & 0xFF;
    const unsigned r = (argb >> 16) & 0xFF;
    const unsigned g = (argb >> 8) & 0xFF;
    const unsigned b = argb & 0xFF;
    // B, G, R, A (premultiplied)
    frame->pixels = {static_cast<std::uint8_t>(b * a / 255), static_cast<std::uint8_t>(g * a / 255),
                     static_cast<std::uint8_t>(r * a / 255), static_cast<std::uint8_t>(a)};
    frame->serial = nextFrameSerial();
    return frame;
}

} // namespace led
