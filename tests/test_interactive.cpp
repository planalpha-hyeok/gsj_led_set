// 인터랙티브 검사: 명령 해석, OSC 해석, 실행 중 덮어쓰기, 실제 UDP/TCP/OSC 수신

#include "TestUtil.h"

#include "core/ControlCommand.h"
#include "core/OscParser.h"
#include "interactive/InteractiveHub.h"
#include "playback/RuntimeOverrides.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTcpSocket>
#include <QThread>
#include <QUdpSocket>

#include <cstdio>
#include <cstring>

using namespace led;

static void pump(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(2);
    }
}

static ControlCommand parse(const char* line, bool expectOk = true)
{
    ControlCommand c;
    std::string error;
    const bool ok = parseCommand(line, c, error);
    if (ok != expectOk)
        std::fprintf(stderr, "parse '%s' -> %d (%s)\n", line, ok, error.c_str());
    CHECK(ok == expectOk);
    return c;
}

static void testCommands()
{
    ControlCommand c = parse("scene 오전");
    CHECK(c.type == CommandType::Scene && c.target == "오전");
    c = parse("  SCENE \"오전 장면\"  ");
    CHECK(c.type == CommandType::Scene && c.target == "오전 장면");
    c = parse("장면 오후 장면"); // 따옴표 없이 공백 포함 이름
    CHECK(c.type == CommandType::Scene && c.target == "오후 장면");
    CHECK(parse("play").type == CommandType::Play);
    CHECK(parse("정지").type == CommandType::Stop);
    CHECK(parse("Reset").type == CommandType::Reset);
    c = parse("next 메인 영상");
    CHECK(c.type == CommandType::Next && c.target == "메인 영상");
    c = parse("이전 2");
    CHECK(c.type == CommandType::Prev && c.target == "2");
    c = parse("hide \"하단 자막\"");
    CHECK(c.type == CommandType::Hide && c.target == "하단 자막");
    c = parse("표시 하단");
    CHECK(c.type == CommandType::Show && c.target == "하단");
    c = parse("trigger touch:버튼1");
    CHECK(c.type == CommandType::Trigger && c.target == "touch:버튼1");

    c = parse("text \"하단 자막\" 환영합니다 | 두 번째 문구 |  ");
    CHECK(c.type == CommandType::Text && c.target == "하단 자막");
    CHECK(c.lines.size() == 2);
    if (c.lines.size() == 2)
        CHECK(c.lines[0] == "환영합니다" && c.lines[1] == "두 번째 문구");
    c = parse("자막 하단 \"따옴표 문구\"");
    CHECK(c.lines.size() == 1 && c.lines[0] == "따옴표 문구");
    c = parse("text 하단"); // 문구 없음 = 자막 비움
    CHECK(c.type == CommandType::Text && c.lines.empty());

    parse("", false);
    parse("scene", false);
    parse("next", false);
    parse("text", false);
    parse("jump 3", false);
}

// OSC 패킷 만들기 (검사용)
static void putString(std::vector<std::uint8_t>& b, const std::string& s)
{
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
    while (b.size() % 4)
        b.push_back(0);
}
static void putU32(std::vector<std::uint8_t>& b, std::uint32_t v)
{
    for (int i = 3; i >= 0; --i)
        b.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
}
static std::vector<std::uint8_t> oscMessage(const std::string& addr, const std::string& types,
                                            const std::vector<std::string>& strings, std::int32_t i = 0, float f = 0)
{
    std::vector<std::uint8_t> b;
    putString(b, addr);
    putString(b, "," + types);
    std::size_t si = 0;
    for (char t : types) {
        if (t == 's')
            putString(b, strings[si++]);
        else if (t == 'i')
            putU32(b, static_cast<std::uint32_t>(i));
        else if (t == 'f') {
            std::uint32_t bits;
            std::memcpy(&bits, &f, 4);
            putU32(b, bits);
        }
    }
    return b;
}

static void testOsc()
{
    auto m1 = oscMessage("/scene", "s", {"오전 장면"});
    auto msgs = parseOscPacket(m1.data(), m1.size());
    CHECK(msgs.size() == 1);
    if (msgs.size() == 1) {
        CHECK(msgs[0].address == "/scene" && msgs[0].args.size() == 1 && msgs[0].args[0] == "오전 장면");
        CHECK(oscToCommand(msgs[0]) == "scene \"오전 장면\"");
    }

    auto m2 = oscMessage("/scene", "i", {}, 2);
    msgs = parseOscPacket(m2.data(), m2.size());
    CHECK(msgs.size() == 1 && msgs[0].args.size() == 1 && msgs[0].args[0] == "2");

    auto m3 = oscMessage("/value", "f", {}, 0, 0.5f);
    msgs = parseOscPacket(m3.data(), m3.size());
    CHECK(msgs.size() == 1 && msgs[0].args.size() == 1 && msgs[0].args[0] == "0.5");

    // 번들 안의 메시지 2개
    std::vector<std::uint8_t> bundle;
    putString(bundle, "#bundle");
    putU32(bundle, 0);
    putU32(bundle, 1);
    auto a = oscMessage("/play", "", {});
    auto b = oscMessage("/text", "ss", {"하단", "안녕"});
    putU32(bundle, static_cast<std::uint32_t>(a.size()));
    bundle.insert(bundle.end(), a.begin(), a.end());
    putU32(bundle, static_cast<std::uint32_t>(b.size()));
    bundle.insert(bundle.end(), b.begin(), b.end());
    msgs = parseOscPacket(bundle.data(), bundle.size());
    CHECK(msgs.size() == 2);
    if (msgs.size() == 2) {
        CHECK(oscToCommand(msgs[0]) == "play");
        ControlCommand c;
        std::string e;
        CHECK(parseCommand(oscToCommand(msgs[1]), c, e) && c.type == CommandType::Text && c.target == "하단");
        CHECK(c.lines.size() == 1 && c.lines[0] == "안녕");
    }

    // 잘린 패킷/쓰레기 데이터: 죽지 않고 무시
    auto cut = oscMessage("/scene", "s", {"abcdefgh"});
    cut.resize(cut.size() - 6);
    msgs = parseOscPacket(cut.data(), cut.size());
    CHECK(msgs.size() <= 1);
    const std::uint8_t junk[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    CHECK(parseOscPacket(junk, sizeof(junk)).empty());
    CHECK(parseOscPacket(nullptr, 0).empty());
}

static void testOverrides()
{
    Project p;
    Region r1;
    r1.id = "r1";
    r1.name = "메인";
    Region r2;
    r2.id = "r2";
    r2.name = "자막";
    p.regions = {r1, r2};
    Scene s1;
    s1.id = "s1";
    RegionContent t;
    t.regionId = "r2";
    t.kind = ContentKind::Ticker;
    t.ticker.lines = {"원래 문구"};
    t.ticker.textFile = "C:/a.txt";
    s1.contents.push_back(t);
    Scene s2;
    s2.id = "s2";
    p.scenes = {s1, s2};

    RuntimeOverrides o;
    CHECK(o.empty());
    o.setVisible("r1", false);
    o.setTickerLines("r2", {"새 문구"});
    const Project q = o.apply(p);
    CHECK(!q.findRegion("r1")->visible);
    CHECK(q.findRegion("r2")->visible);
    const RegionContent* c1 = q.scenes[0].contentFor("r2");
    CHECK(c1 && c1->ticker.lines.size() == 1 && c1->ticker.lines[0] == "새 문구" && c1->ticker.textFile.empty());
    const RegionContent* c2 = q.scenes[1].contentFor("r2"); // 내용이 없던 장면에도 자막 적용
    CHECK(c2 && c2->kind == ContentKind::Ticker && c2->ticker.lines.size() == 1);
    CHECK(p.findRegion("r1")->visible); // 원본은 그대로
    o.clear();
    CHECK(o.empty());
}

static void testHub()
{
    InteractiveHub hub;
    QStringList received;
    hub.setHandler([&](const QString& cmd, const QString& source) -> QString {
        received << cmd + QStringLiteral(" @") + source.section(QLatin1Char(' '), 0, 0);
        return cmd.startsWith(QLatin1String("bad")) ? QStringLiteral("잘못된 명령") : QString();
    });

    InteractiveConfig c;
    c.udpEnabled = true;
    c.udpPort = 47123;
    c.tcpEnabled = true;
    c.tcpPort = 47124;
    c.oscEnabled = true;
    c.oscPort = 47125;
    hub.apply(c);
    CHECK(hub.statusText().contains(QStringLiteral("UDP 47123")));

    // UDP: 한 패킷에 여러 줄
    QUdpSocket udp;
    udp.writeDatagram(QByteArray("scene 오전\nplay\n"), QHostAddress::LocalHost, 47123);
    // OSC
    auto osc = oscMessage("/next", "s", {"메인"});
    udp.writeDatagram(reinterpret_cast<const char*>(osc.data()), static_cast<qint64>(osc.size()), QHostAddress::LocalHost,
                      47125);
    pump(300);

    // TCP: 응답 확인
    QTcpSocket tcp;
    tcp.connectToHost(QHostAddress::LocalHost, 47124);
    CHECK(tcp.waitForConnected(2000));
    tcp.write("stop\r\nbad 명령\n");
    QByteArray reply;
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 2000 && reply.count('\n') < 2) {
        pump(20);
        if (tcp.bytesAvailable())
            reply += tcp.readAll();
        tcp.waitForReadyRead(10);
        reply += tcp.readAll();
    }
    CHECK(reply.startsWith("OK\n"));
    CHECK(reply.contains("ERR "));

    std::printf("받은 명령: %s\n", received.join(QStringLiteral(" | ")).toUtf8().constData());
    CHECK(received.contains(QStringLiteral("scene 오전 @UDP")));
    CHECK(received.contains(QStringLiteral("play @UDP")));
    CHECK(received.contains(QStringLiteral("next \"메인\" @OSC")));
    CHECK(received.contains(QStringLiteral("stop @TCP")));

    // 같은 포트를 또 열면 실패를 보고해야 함
    InteractiveHub other;
    other.apply(c);
    CHECK(other.statusText().contains(QStringLiteral("실패")));

    hub.stopAll();
    // 끈 뒤에는 수신하지 않음
    received.clear();
    udp.writeDatagram(QByteArray("play\n"), QHostAddress::LocalHost, 47123);
    pump(200);
    CHECK(received.isEmpty());
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    testCommands();
    testOsc();
    testOverrides();
    testHub();
    return test::finish("test_interactive");
}
