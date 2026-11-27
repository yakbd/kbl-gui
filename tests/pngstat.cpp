// pngstat light.png dark.png: checks that two screenshots are not blank and that the first is light and the
// second dark (mean luminance), so a theme that silently stays light fails the test.
#include <QImage>
#include <cmath>
#include <cstdio>

static bool stats(const char *path, double *mean, double *spread)
{
    QImage img(QString::fromLocal8Bit(path));
    if (img.isNull()) {
        std::printf("FAIL %s: not an image\n", path);
        return false;
    }
    img = img.convertToFormat(QImage::Format_RGB32);
    double sum = 0, sq = 0;
    long n = 0;
    for (int y = 0; y < img.height(); y += 2)
        for (int x = 0; x < img.width(); x += 2) {
            const QRgb p = img.pixel(x, y);
            const double l = 0.2126 * qRed(p) + 0.7152 * qGreen(p) + 0.0722 * qBlue(p);
            sum += l;
            sq += l * l;
            ++n;
        }
    *mean = sum / n;
    *spread = std::sqrt(std::max(0.0, sq / n - *mean * *mean));
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        std::fprintf(stderr, "usage: pngstat light.png dark.png\n");
        return 2;
    }
    double ml, sl, md, sd;
    if (!stats(argv[1], &ml, &sl) || !stats(argv[2], &md, &sd))
        return 1;
    std::printf("%s mean %.0f spread %.0f | %s mean %.0f spread %.0f\n", argv[1], ml, sl, argv[2], md, sd);
    bool ok = true;
    if (sl < 8 || sd < 8) {
        std::printf("FAIL a shot is blank (no contrast)\n");
        ok = false;
    }
    if (!(ml > 150 && md < 100)) {
        std::printf("FAIL the light shot is not light or the dark shot is not dark\n");
        ok = false;
    }
    return ok ? 0 : 1;
}
