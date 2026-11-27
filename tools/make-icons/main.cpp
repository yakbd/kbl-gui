// Renders qml/icons/yakbd.svg into packaging/windows/yakbd.ico and packaging/macos/yakbd.icns (PNG entries).
//   cmake -S tools/make-icons -B build-icons -DCMAKE_PREFIX_PATH=<Qt> && cmake --build build-icons
//   QT_QPA_PLATFORM=offscreen build-icons/make-icons qml/icons/yakbd.svg packaging/windows/yakbd.ico packaging/macos/yakbd.icns
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QBuffer>
#include <QFile>
#include <QtEndian>
static QImage render(QSvgRenderer &r, int s) { QImage i(s, s, QImage::Format_ARGB32); i.fill(Qt::transparent); QPainter p(&i); r.render(&p); return i; }
static QByteArray png(const QImage &i) { QByteArray b; QBuffer buf(&b); buf.open(QIODevice::WriteOnly); i.save(&buf, "PNG"); return b; }
int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  QSvgRenderer r(QString::fromLocal8Bit(argv[1]));
  // ICO with PNG entries
  QList<int> sizes{16, 24, 32, 48, 64, 128, 256};
  QByteArray ico; QByteArray dir; QByteArray data;
  auto le16=[](QByteArray &b, quint16 v){ b.append(char(v&255)); b.append(char(v>>8)); };
  auto le32=[](QByteArray &b, quint32 v){ for(int i=0;i<4;++i) b.append(char((v>>(8*i))&255)); };
  le16(ico,0); le16(ico,1); le16(ico,sizes.size());
  quint32 off = 6 + 16*sizes.size();
  for (int s: sizes) { QByteArray p = png(render(r,s)); dir.append(char(s==256?0:s)); dir.append(char(s==256?0:s)); dir.append(char(0)); dir.append(char(0)); le16(dir,1); le16(dir,32); le32(dir,p.size()); le32(dir,off); off+=p.size(); data+=p; }
  QFile f(argv[2]); f.open(QIODevice::WriteOnly); f.write(ico+dir+data); f.close();
  // ICNS
  QByteArray body;
  struct E { const char *t; int s; } es[] = {{"ic07",128},{"ic08",256},{"ic09",512},{"ic10",1024},{"ic11",32},{"ic12",64},{"ic13",256},{"ic14",512}};
  auto be32=[](QByteArray &b, quint32 v){ for(int i=3;i>=0;--i) b.append(char((v>>(8*i))&255)); };
  for (auto e: es) { QByteArray p = png(render(r,e.s)); body.append(e.t,4); be32(body, p.size()+8); body+=p; }
  QByteArray icns("icns",4); be32(icns, body.size()+8); icns+=body;
  QFile g(argv[3]); g.open(QIODevice::WriteOnly); g.write(icns);
  return 0;
}
