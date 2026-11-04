#pragma once

// 도구모음 아이콘 (어두운 배경에서 잘 보이도록 직접 그림, 외부 이미지 파일 불필요)

#include <QIcon>

namespace led::icons {

QIcon newProject();
QIcon open();
QIcon save();
QIcon play();
QIcon stop();
QIcon output();
QIcon testPattern();
QIcon fit();
QIcon schedule();
QIcon magnet();

} // namespace led::icons
