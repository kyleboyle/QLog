#ifndef GRIDSQUARE_H
#define GRIDSQUARE_H

#include <QObject>
#include <QString>
#include <QDebug>

class Gridsquare
{
public:
    explicit Gridsquare(const QString &in_grid = QString());
    explicit Gridsquare(const double lat, double lon);
    ~Gridsquare() {};
    static QRegularExpression gridRegEx();
    static QRegularExpression gridVUCCRegEx();

    bool isValid() const;
    double getLongitude() const {return lon;};
    double getLatitude() const {return lat;};
    QString getGrid() const { return grid;};
    bool distanceTo(const Gridsquare &in_grid, double &distance) const;
    bool distanceTo(double lat, double lon, double &distance) const;
    bool bearingTo(const Gridsquare &in_grid, double &bearing) const;
    bool bearingTo(double lat, double lon, double &bearing) const ;
    operator QString() const { return QString("Gridsquare: grid[%1]; valid[%2]; lat[%3]; lon[%4]")
                                      .arg(grid).arg(validGrid).arg(lat).arg(lon);};

private:
    QString grid;
    bool validGrid;
    double lat, lon;
};

#endif // GRIDSQUARE_H
