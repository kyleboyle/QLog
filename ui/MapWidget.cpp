#include <QGraphicsTextItem>
#include <QTime>
#include <QTimer>
#include <QDebug>
#include <QPainter>
#include <QVector3D>
#include <QtMath>
#include "MapWidget.h"
#include "core/debug.h"
#include "core/Gridsquare.h"
#include "data/StationProfile.h"

MODULE_IDENTIFICATION("qlog.ui.mapwidget");

MapWidget::MapWidget(QWidget *parent) :
    QGraphicsView(parent)
{
    FCT_IDENTIFICATION;

    scene = new QGraphicsScene(this);
    this->setScene(scene);
    this->setStyleSheet("background-color: transparent;");

    QPixmap pix(":/res/map/nasabluemarble.jpg");
    scene->addPixmap(pix);
    scene->setSceneRect(pix.rect());

    nightOverlay = new QGraphicsPixmapItem();
    scene->addItem(nightOverlay);

    /*sunItem = scene->addEllipse(0, 0, sunSize, sunSize,
                                QPen(QColor(235, 219, 52)),
                                QBrush(QColor(235, 219, 52),
                                        Qt::SolidPattern));
*/
    terminatorItem = scene->addPath(QPainterPath(), QPen(QColor(100, 100, 100), 2),
                                    QBrush(QColor(0, 0, 0),
                                        Qt::SolidPattern));

    this->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    redrawNightOverlay();
    clear();

    QTimer* timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(redraw()));
    timer->start(60000);
}

void MapWidget::clear() {
    FCT_IDENTIFICATION;

    QMutableListIterator<QGraphicsItem*> i(items);
    while (i.hasNext()) {
        QGraphicsItem* item = i.next();
        scene->removeItem(item);
        i.remove();
    }

    Gridsquare myGrid(StationProfilesManager::instance()->getCurProfile1().locator);

    double lat=0;
    double lon=0;

    if ( myGrid.isValid() )
    {
        lat = myGrid.getLatitude();
        lon = myGrid.getLongitude();
    }

    drawPoint(coordToPoint(lat, lon));
}

void MapWidget::redraw() {
    FCT_IDENTIFICATION;

    redrawNightOverlay();
}

void MapWidget::drawPoint(const QPoint &point) {
    FCT_IDENTIFICATION;

    items << scene->addEllipse(point.x()-2, point.y()-2, 4, 4,
                               QPen(QColor(255, 0, 0)),
                               QBrush(QColor(255, 0, 0),
                                      Qt::SolidPattern));
}

void MapWidget::drawLine(const QPoint &pointA, const QPoint &pointB) {
    FCT_IDENTIFICATION;

    QPainterPath path;
    path.moveTo(pointA);

    double latA, lonA, latB, lonB;
    pointToRad(pointA, latA, lonA);
    pointToRad(pointB, latB, lonB);

    double d = 2*asin(sqrt(pow(sin(latA-latB)/2, 2) + cos(latA)* cos(latB) * pow(sin((lonA-lonB)/2), 2)));

    double f = 0;
    for (int i = 0; i < 1000; i++)
    {
        double A = sin((1-f)*d)/sin(d);
        double B = sin(f*d)/sin(d);
        double x = A*cos(latA)*cos(lonA) + B*cos(latB)*cos(lonB);
        double y = A*cos(latA)*sin(lonA) + B*cos(latB)*sin(lonB);
        double z = A*sin(latA)           + B*sin(latB);
        double lat = atan2(z, sqrt(x*x + y*y));
        double lon = atan2(y, x);

        QPoint p = radToPoint(lat, lon);
        path.lineTo(p);
        path.moveTo(p);

        f += 0.001;
    }

    path.lineTo(pointB);
    path.closeSubpath();

    items << scene->addPath(path, QPen(QColor(255, 0, 0)),
                            QBrush(QColor(255, 0, 0), Qt::SolidPattern));
}

void MapWidget::redrawNightOverlay() {
    FCT_IDENTIFICATION;

    QDateTime current = QDateTime::currentDateTimeUtc();
    int secondOfDay = (QTime(0, 0, 0).secsTo(current.time()) + 43200) % 86400;
    int dayOfYear = current.date().dayOfYear();
    int daysInYear = QDate::isLeapYear(current.date().year()) ? 366 : 365;
    int longestDay = QDate(current.date().year(), 6, 21).dayOfYear();

    double yearProgress = static_cast<double>(dayOfYear-longestDay) / static_cast<double>(daysInYear);
    double tilt = 23.5 * cos(2.0 * M_PI * yearProgress);

    double sunX = cos(2 * M_PI * (secondOfDay / 86400.0));
    double sunY = -sin(2 * M_PI * (secondOfDay / 86400.0));
    double sunZ = tan(2 * M_PI * (tilt / 360.0));

    QVector3D sun(static_cast<float>(sunX), static_cast<float>(sunY), static_cast<float>(sunZ));
    sun.normalize();

    // <plot sun position>
    //double theta = acos(sunZ);
    //double phi = atan(sunY/sunX);
    //double sunLon = phi/M_PI * 180.0;
    //double sunLat = 90 - theta / M_PI * 180.0;
    //sunItem->setPos(coordToPoint(sunLat, sunLon) - QPoint(sunSize / 2, sunSize / 2));
    // </plot sun position>

    int maxX = static_cast<int>(scene->width());
    int maxY = static_cast<int>(scene->height());

    QImage overlay(maxX, maxY, QImage::Format_ARGB32);
    uchar* buffer = overlay.bits();

    for (int y = 0; y < maxY; y++) {
        double theta = M_PI * (static_cast<double>(y) / (static_cast<double>(maxY) - 1.0));
        double posZ = cos(theta);
        double sinTheta = sin(theta);

        for (int x = 0; x < maxX; x++) {
            double phi = 2 * M_PI * (static_cast<double>(x) / (static_cast<double>(maxX) - 1.0)) - M_PI;

            double posX = sinTheta * cos(phi);
            double posY = sinTheta * sin(phi);

            QVector3D pos(static_cast<float>(posX), static_cast<float>(posY), static_cast<float>(posZ));
            pos.normalize();

            buffer[0] = 0;
            buffer[1] = 0;
            buffer[2] = 0;

            float ill = QVector3D::dotProduct(sun, pos);
            if (ill <= -0.1f) {
                buffer[3] = 255;
            }
            else if (ill < 0.1f) {
                double illd = 1.0 - (static_cast<double>(ill) + 0.1) * 5.0;
                illd = pow(illd, 8);
                buffer[3] = static_cast<uchar>(255.0 * illd);
            }
            else {
                buffer[3] = 0;
            }
            buffer += 4;
        }
    }


    QImage night(":/res/map/nasaearthlights.jpg");

    QPainter painter;
    painter.begin(&overlay);
    painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    painter.drawImage(0, 0, night);
    painter.end();

    nightOverlay->setPixmap(QPixmap::fromImage(overlay));
}

void MapWidget::pointToRad(const QPoint &point, double& lat, double& lon) {
    FCT_IDENTIFICATION;

    lat = M_PI / 2.0 - static_cast<double>(point.y()) / (scene->height() - 1.0) * M_PI;
    lon = 2 * M_PI *(static_cast<double>(point.x()) / (scene->width() - 1.0)) - M_PI;
}

void MapWidget::pointToCoord(const QPoint &point, double& lat, double& lon) {
    FCT_IDENTIFICATION;

    lat = 90.0 - (point.y() / scene->height()) * 180.0;
    lon = 360.0 * (point.x() / scene->width()) - 180.0;
}

QPoint MapWidget::radToPoint(double lat, double lon) {
    FCT_IDENTIFICATION;

    int x = static_cast<int>((lon + M_PI) / (2.0 * M_PI) * scene->width());
    int y = static_cast<int>(scene->height() / 2.0 - (2.0 * lat / M_PI) * (scene->height() / 2.0));
    return QPoint(x, y);
}

QPoint MapWidget::coordToPoint(double lat, double lon) {
    FCT_IDENTIFICATION;

    int x = static_cast<int>((lon + 180.0) / 360.0 * scene->width());
    int y = static_cast<int>(scene->height() / 2.0 - (lat / 90.0) * (scene->height() / 2.0));
    return QPoint(x, y);
}

void MapWidget::setTarget(double lat, double lon) {
    FCT_IDENTIFICATION;

    clear();

    if (lat == 0.0 && lon == 0.0) return;

    Gridsquare myGrid(StationProfilesManager::instance()->getCurProfile1().locator);

    QPoint point = coordToPoint(lat, lon);
    drawPoint(point);

    if ( myGrid.isValid() )
    {
        double qthLat = myGrid.getLatitude();
        double qthLon = myGrid.getLongitude();
        QPoint qth = coordToPoint(qthLat, qthLon);
        drawPoint(qth);
        drawLine(qth, point);
    }
}

void MapWidget::showEvent(QShowEvent* event) {
    FCT_IDENTIFICATION;

    this->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    QWidget::showEvent(event);
}

void MapWidget::resizeEvent(QResizeEvent* event) {
    FCT_IDENTIFICATION;

    this->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    QWidget::resizeEvent(event);
}

MapWidget::~MapWidget()
{
}
