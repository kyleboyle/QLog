#ifndef QLOG_UI_DXCCTABLEWIDGET_H
#define QLOG_UI_DXCCTABLEWIDGET_H

#include <QWidget>
#include <QTableView>
#include "data/Band.h"

class DxccTableModel;

class DxccTableWidget : public QTableView
{
    Q_OBJECT
public:
    explicit DxccTableWidget(QWidget *parent = nullptr);

public slots:
    void clear();
    void setDxcc(int dxcc, Band band);

private:
    DxccTableModel* dxccTableModel;
};

#endif // QLOG_UI_DXCCTABLEWIDGET_H
