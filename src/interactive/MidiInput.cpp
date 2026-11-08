#include "interactive/MidiInput.h"

#include "core/Log.h"

#include <QMetaObject>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

namespace led {

#ifdef Q_OS_WIN
namespace {

void CALLBACK midiCallback(HMIDIIN, UINT msg, DWORD_PTR instance, DWORD_PTR param1, DWORD_PTR)
{
    if (msg != MIM_DATA)
        return;
    auto* self = reinterpret_cast<MidiInput*>(instance);
    const DWORD data = static_cast<DWORD>(param1);
    self->deliver(data & 0xFF, (data >> 8) & 0x7F, (data >> 16) & 0x7F);
}

QString deviceName(UINT id)
{
    MIDIINCAPSW caps{};
    if (midiInGetDevCapsW(id, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
        return {};
    return QString::fromWCharArray(caps.szPname);
}

} // namespace
#endif

MidiInput::MidiInput(QObject* parent)
    : QObject(parent)
{
}

MidiInput::~MidiInput()
{
    close();
}

QStringList MidiInput::deviceNames()
{
    QStringList names;
#ifdef Q_OS_WIN
    const UINT count = midiInGetNumDevs();
    for (UINT i = 0; i < count; ++i)
        names << deviceName(i);
#endif
    return names;
}

bool MidiInput::open(const QString& name, QString* error)
{
    close();
#ifdef Q_OS_WIN
    const UINT count = midiInGetNumDevs();
    for (UINT i = 0; i < count; ++i) {
        if (deviceName(i) != name)
            continue;
        HMIDIIN handle = nullptr;
        if (midiInOpen(&handle, i, reinterpret_cast<DWORD_PTR>(&midiCallback), reinterpret_cast<DWORD_PTR>(this),
                       CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
            if (error)
                *error = QStringLiteral("MIDI 장치를 열 수 없음: %1").arg(name);
            return false;
        }
        midiInStart(handle);
        m_handle = handle;
        return true;
    }
    if (error)
        *error = QStringLiteral("MIDI 장치를 찾을 수 없음: %1").arg(name);
    return false;
#else
    if (error)
        *error = QStringLiteral("이 운영체제에서는 MIDI 입력을 지원하지 않음");
    Q_UNUSED(name);
    return false;
#endif
}

void MidiInput::close()
{
#ifdef Q_OS_WIN
    if (m_handle) {
        HMIDIIN handle = static_cast<HMIDIIN>(m_handle);
        midiInStop(handle);
        midiInReset(handle);
        midiInClose(handle);
    }
#endif
    m_handle = nullptr;
}

void MidiInput::deliver(unsigned status, unsigned data1, unsigned data2)
{
    const unsigned kind = status & 0xF0;
    QString trigger;
    if (kind == 0x90 && data2 > 0)
        trigger = QStringLiteral("midi:note:%1").arg(data1);
    else if (kind == 0xB0 && data2 > 0)
        trigger = QStringLiteral("midi:cc:%1").arg(data1);
    else if (kind == 0xC0)
        trigger = QStringLiteral("midi:pc:%1").arg(data1);
    if (trigger.isEmpty())
        return;
    // GUI 스레드에서 신호 발생
    QMetaObject::invokeMethod(
        this, [this, trigger] { emit triggered(trigger); }, Qt::QueuedConnection);
}

} // namespace led
