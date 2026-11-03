#pragma once

// QImage <-> Frame 변환

#include "media/Frame.h"

#include <QImage>

#include <memory>

namespace led {

std::shared_ptr<Frame> frameFromQImage(const QImage& image);

} // namespace led
