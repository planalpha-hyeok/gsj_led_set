#pragma once

// 화면 색 정의 (시안 A: 미디어 서버형 프로 다크)
// 테마 스타일시트, 캔버스, 타임라인, 운영 화면이 같은 색을 쓰도록 한곳에 모음

#include <QColor>
#include <QString>

namespace led::palette {

// 바탕
inline const QColor kBackground(11, 12, 15);      // #0b0c0f 가장 어두운 바탕 (캔버스/입력칸)
inline const QColor kPanel(17, 19, 24);           // #111318 패널
inline const QColor kRaised(22, 25, 32);          // #161920 버튼/그룹 머리
inline const QColor kHover(30, 34, 43);           // #1e222b 마우스 올림
inline const QColor kBorder(31, 35, 43);          // #1f232b 구분선
inline const QColor kBorderStrong(38, 43, 52);    // #262b34 입력칸 테두리

// 글자
inline const QColor kText(230, 232, 236);         // #e6e8ec
inline const QColor kTextSecondary(201, 205, 212);// #c9cdd4
inline const QColor kTextDim(139, 146, 158);      // #8b929e 보조 글자 (패널 위 대비 6:1)
inline const QColor kTextDisabled(96, 102, 113);  // #606671

// 강조
inline const QColor kLive(245, 165, 36);          // #f5a524 송출 중 / 재생 위치
inline const QColor kSelect(76, 141, 255);        // #4c8dff 선택 / 다음
inline const QColor kSelectBg(34, 50, 79);        // #22324f 선택 바탕
inline const QColor kOk(79, 195, 138);            // #4fc38a 정상
inline const QColor kDanger(255, 107, 99);        // #ff6b63 오류 / 비상
inline const QColor kDangerBg(42, 15, 15);        // #2a0f0f

inline QString css(const QColor& c) { return c.name(QColor::HexRgb); }

// 숫자(좌표, 시간) 표시용 고정폭 글꼴
inline QString monoFontFamily()
{
#ifdef Q_OS_WIN
    return QStringLiteral("Consolas");
#else
    return QStringLiteral("monospace");
#endif
}

} // namespace led::palette
