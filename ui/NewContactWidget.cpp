#include <QtSql/QtSql>
#include <QShortcut>
#include <QDebug>
#include "core/Rig.h"
#include "core/utils.h"
#include "NewContactWidget.h"
#include "ui_NewContactWidget.h"

NewContactWidget::NewContactWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::NewContactWidget)
{
    ui->setupUi(this);

    rig = Rig::instance();

    QStringListModel* rigModel = new QStringListModel(this);
    ui->rigEdit->setModel(rigModel);

    QStringListModel* submodeModel = new QStringListModel(this);
    ui->submodeEdit->setModel(submodeModel);

    QSqlTableModel* modeModel = new QSqlTableModel();
    modeModel->setTable("modes");
    modeModel->setFilter("enabled = true");
    ui->modeEdit->setModel(modeModel);
    ui->modeEdit->setModelColumn(modeModel->fieldIndex("name"));
    modeModel->select();

    QStringList contestList = Data::instance()->contestList();
    contestList.prepend("");
    QStringListModel* contestModel = new QStringListModel(contestList, this);
    ui->contestEdit->setModel(contestModel);

    QStringList propagationModeList = Data::instance()->propagationModesList();
    propagationModeList.prepend("");
    QStringListModel* propagationModeModel = new QStringListModel(propagationModeList, this);
    ui->propagationModeEdit->setModel(propagationModeModel);

    connect(rig, &Rig::frequencyChanged,
            this, &NewContactWidget::changeFrequency);

    connect(rig, &Rig::modeChanged,
            this, &NewContactWidget::changeMode);

    connect(rig, &Rig::powerChanged,
            this, &NewContactWidget::changePower);

    contactTimer = new QTimer(this);
    connect(contactTimer, &QTimer::timeout, this, &NewContactWidget::updateTimeOff);

    connect(&callbook, &HamQTH::callsignResult, this, &NewContactWidget::callsignResult);

    new QShortcut(QKeySequence(Qt::Key_Escape), this, SLOT(resetContact()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(QKeySequence(Qt::Key_F10), this, SLOT(saveContact()), nullptr, Qt::ApplicationShortcut);
    new QShortcut(QKeySequence(Qt::Key_F9), this, SLOT(stopContactTimer()), nullptr, Qt::ApplicationShortcut);

    reloadSettings();
    readSettings();
    resetContact();
}

void NewContactWidget::readSettings() {
    QSettings settings;
    QString mode = settings.value("newcontact/mode", "CW").toString();
    double freq = settings.value("newcontact/frequency", 3.5).toDouble();
    QString rig = settings.value("newcontact/rig").toString();
    double power = settings.value("newcontact/power", 100).toDouble();

    ui->modeEdit->setCurrentText(mode);
    ui->frequencyEdit->setValue(freq);
    ui->rigEdit->setCurrentText(rig);
    ui->powerEdit->setValue(power);
}

void NewContactWidget::writeSettings() {
    QSettings settings;
    settings.setValue("newcontact/mode", ui->modeEdit->currentText());
    settings.setValue("newcontact/frequency", ui->frequencyEdit->value());
    settings.setValue("newcontact/rig", ui->rigEdit->currentText());
    settings.setValue("newcontact/power", ui->powerEdit->value());
}

void NewContactWidget::reloadSettings() {
    QString selectedRig = ui->rigEdit->currentText();
    QSettings settings;
    QStringList rigs = settings.value("station/rigs").toStringList();
    QStringListModel* model = dynamic_cast<QStringListModel*>(ui->rigEdit->model());
    model->setStringList(rigs);

    if (!selectedRig.isEmpty()) {
        ui->rigEdit->setCurrentText(selectedRig);
    }
}

void NewContactWidget::callsignChanged() {
    QString newCallsign = ui->callsignEdit->text().toUpper();
    if (newCallsign == callsign) {
        return;
    }
    else {
        callsign = newCallsign;
    }

    updateTime();

    if (callsign.isEmpty()) {
        stopContactTimer();
    }
    else {
        startContactTimer();
        queryDatabase(callsign);
        queryDxcc(callsign);
        callbook.queryCallsign(callsign);
    }
}


void NewContactWidget::queryDxcc(QString callsign) {
    dxccEntity = Data::instance()->lookupDxcc(callsign);
    if (dxccEntity.dxcc) {
         ui->dxccInfo->setText(dxccEntity.country);
         ui->cqEdit->setText(QString::number(dxccEntity.cqz));
         ui->ituEdit->setText(QString::number(dxccEntity.ituz));
         updateCoordinates(dxccEntity.latlon[0], dxccEntity.latlon[1], COORD_DXCC);

         QSqlQueryModel* queryModel = new QSqlQueryModel;
         queryModel->setQuery(QString("SELECT contacts.band,\n"
                              "count(CASE WHEN modes.dxcc = 'CW' THEN 1 END) as cw,\n"
                              "count(CASE WHEN modes.dxcc = 'PHONE' THEN 1 END) as phone,\n"
                              "count(CASE WHEN modes.dxcc = 'DIGITAL' THEN 1 END) as digital\n"
                              "FROM contacts\n"
                              "INNER JOIN modes ON (contacts.mode = modes.name)\n"
                              "INNER JOIN bands ON (contacts.band = bands.name)\n"
                              "WHERE contacts.dxcc = %1 AND bands.enabled = true\n"
                              "GROUP BY contacts.band\n"
                              "ORDER BY contacts.band").arg(dxccEntity.dxcc));

         ui->tableView->setModel(queryModel);
         ui->tableView->show();
   }
}

void NewContactWidget::queryDatabase(QString callsign) {
    QSqlQuery query;
    query.prepare("SELECT name, qth, gridsquare FROM contacts "
                  "WHERE callsign = :callsign ORDER BY start_time DESC LIMIT 1");
    query.bindValue(":callsign", callsign);
    query.exec();

    if (query.next()){
        ui->nameEdit->setText(query.value(0).toString());
        ui->qthEdit->setText(query.value(1).toString());
        ui->gridEdit->setText(query.value(2).toString());
        ui->callsignEdit->setStyleSheet("background-color: #99ff99;");
    }
    else {
        ui->callsignEdit->setStyleSheet("");
    }
}

void NewContactWidget::callsignResult(const QMap<QString, QString>& data) {
    if (!data.value("name").isEmpty() && ui->nameEdit->text().isEmpty()) {
        ui->nameEdit->setText(data.value("name"));
    }

    if (!data.value("gridsquare").isEmpty() && ui->gridEdit->text().isEmpty()) {
        ui->gridEdit->setText(data.value("gridsquare"));
    }

    if (!data.value("qth").isEmpty() && ui->qthEdit->text().isEmpty()) {
        ui->qthEdit->setText(data.value("qth"));
    }

    if (!data.value("qsl_via").isEmpty() && ui->qslViaEdit->text().isEmpty()) {
        ui->qslViaEdit->setText(data.value("qsl_via"));
    }

    if (!data.value("cqz").isEmpty() && ui->cqEdit->text().isEmpty()) {
        ui->cqEdit->setText(data.value("cqz"));
    }

    if (!data.value("ituz").isEmpty() && ui->ituEdit->text().isEmpty()) {
        ui->ituEdit->setText(data.value("ituz"));
    }

    qDebug() << data;

    if (ui->callsignEdit->styleSheet().isEmpty()) {
        ui->callsignEdit->setStyleSheet("background-color: #bbddff;");
    }
}

void NewContactWidget::frequencyChanged() {
    double freq = ui->frequencyEdit->value();
    QString band = Data::band(freq);

    if (band.isEmpty()) {
        ui->bandText->setText("OOB!");
    }
    else {
        ui->bandText->setText(band);
    }

    rig->setFrequency(freq);
}

void NewContactWidget::modeChanged() {
    QString modeName = ui->modeEdit->currentText();
    rig->setMode(modeName);

    QSqlTableModel* modeModel = dynamic_cast<QSqlTableModel*>(ui->modeEdit->model());
    QSqlRecord record = modeModel->record(ui->modeEdit->currentIndex());
    QString submodes = record.value("submodes").toString();

    QStringList submodeList = QJsonDocument::fromJson(submodes.toUtf8()).toVariant().toStringList();
    QStringListModel* model = dynamic_cast<QStringListModel*>(ui->submodeEdit->model());
    model->setStringList(submodeList);

    if (!submodeList.isEmpty()) {
        submodeList.prepend("");
        model->setStringList(submodeList);
        ui->submodeEdit->setEnabled(true);
    }
    else {
        QStringList list;
        model->setStringList(list);
        ui->submodeEdit->setEnabled(false);
    }

    defaultReport = record.value("rprt").toString();

    setDefaultReport();
}

void NewContactWidget::gridChanged() {
    double lat, lon;
    bool valid = gridToCoord(ui->gridEdit->text(), lat, lon);
    if (!valid) return;
    updateCoordinates(lat, lon, COORD_GRID);
}

void NewContactWidget::resetContact() {
    updateTime();
    ui->callsignEdit->clear();
    ui->nameEdit->clear();
    ui->qthEdit->clear();
    ui->gridEdit->clear();
    ui->commentEdit->clear();
    ui->dxccInfo->clear();
    ui->distanceInfo->clear();
    ui->bearingInfo->clear();
    ui->qslViaEdit->clear();
    ui->cqEdit->clear();
    ui->ituEdit->clear();

    stopContactTimer();
    setDefaultReport();

    ui->callsignEdit->setStyleSheet("");
    ui->callsignEdit->setFocus();
    callsign = QString();
    coordPrec = COORD_NONE;
    emit newTarget(0, 0);
}

void NewContactWidget::saveContact() {
    QSettings settings;
    QSqlTableModel model;
    model.setTable("contacts");
    model.removeColumn(model.fieldIndex("id"));

    QDateTime start = QDateTime(ui->dateEdit->date(), ui->timeOnEdit->time(), Qt::UTC);
    QDateTime end = QDateTime(ui->dateEdit->date(), ui->timeOffEdit->time(), Qt::UTC);

    QSqlRecord record = model.record();
    record.setValue("callsign", callsign);
    record.setValue("rst_sent", ui->rstSentEdit->text());
    record.setValue("rst_rcvd", ui->rstRcvdEdit->text());
    record.setValue("name", ui->nameEdit->text());
    record.setValue("qth", ui->qthEdit->text());
    record.setValue("gridsquare", ui->gridEdit->text());
    record.setValue("start_time", start);
    record.setValue("end_time", end);
    record.setValue("freq", ui->frequencyEdit->value());
    record.setValue("band", ui->bandText->text());
    record.setValue("mode", ui->modeEdit->currentText());
    record.setValue("submode", ui->submodeEdit->currentText());
    record.setValue("cqz", ui->cqEdit->text().toInt());
    record.setValue("ituz", ui->ituEdit->text().toInt());
    record.setValue("dxcc", dxccEntity.dxcc);
    record.setValue("country", dxccEntity.country);
    record.setValue("cont", dxccEntity.cont);

    QMap<QString, QVariant> fields;

    if (!ui->commentEdit->text().isEmpty()) {
        fields.insert("comment", ui->commentEdit->text());
    }

    if (!ui->qslViaEdit->text().isEmpty()) {
        fields.insert("qsl_via", ui->qslViaEdit->text());
    }

    if (ui->powerEdit->value() != 0.0) {
        fields.insert("tx_pwr", ui->powerEdit->value());
    }

    if (!ui->rigEdit->currentText().isEmpty()) {
        fields.insert("my_rig", ui->rigEdit->currentText());
    }

    if (!settings.value("station/grid").toString().isEmpty()) {
        fields.insert("my_gridsquare", settings.value("station/grid").toString());
    }

    if (!settings.value("station/operator").toString().isEmpty()) {
        fields.insert("operator", settings.value("station/operator").toString());
    }

    QJsonDocument doc = QJsonDocument::fromVariant(QVariant(fields));
    record.setValue("fields", QString(doc.toJson()));

    qDebug() << record;

    if (!model.insertRecord(-1, record)) {
        qDebug() << model.lastError();
        return;
    }

    if (!model.submitAll()) {
        qDebug() << model.lastError();
        return;
    }

    resetContact();
    emit contactAdded();
}

void NewContactWidget::startContactTimer() {
    if (!contactTimer->isActive()) {
        contactTimer->start(1000);
    }
}

void NewContactWidget::stopContactTimer() {
    if (contactTimer->isActive()) {
        contactTimer->stop();
    }
    updateTimeOff();
}

void NewContactWidget::updateTime() {
    ui->dateEdit->setDate(QDate::currentDate());
    ui->timeOnEdit->setTime(QDateTime::currentDateTimeUtc().time());
    ui->timeOffEdit->setTime(QDateTime::currentDateTimeUtc().time());
    startContactTimer();
}

void NewContactWidget::updateTimeOff() {
    ui->timeOffEdit->setTime(QDateTime::currentDateTimeUtc().time());
}

void NewContactWidget::updateCoordinates(double lat, double lon, CoordPrecision prec) {
    if (prec <= coordPrec) return;

    QSettings settings;
    QString myGrid = settings.value("station/grid").toString();

    double myLat, myLon;
    gridToCoord(myGrid, myLat, myLon);

    double distance = coordDistance(myLat, myLon, lat, lon);
    int bearing = coordBearing(myLat, myLon, lat, lon);

    ui->distanceInfo->setText(QString::number(distance, '.', 1) + " km");
    ui->bearingInfo->setText(QString("%1°").arg(bearing));

    coordPrec = prec;

    emit newTarget(lat, lon);
}

void NewContactWidget::changeFrequency(double freq) {
    ui->frequencyEdit->blockSignals(true);
    ui->frequencyEdit->setValue(freq);
    ui->frequencyEdit->blockSignals(false);
}

void NewContactWidget::changeMode(QString mode) {
    ui->modeEdit->blockSignals(true);
    ui->modeEdit->setCurrentText(mode);
    ui->modeEdit->blockSignals(false);
}

void NewContactWidget::changePower(double power) {
    ui->powerEdit->blockSignals(true);
    ui->powerEdit->setValue(power);
    ui->powerEdit->blockSignals(false);
}

void NewContactWidget::tuneDx(QString callsign, double frequency) {
    resetContact();
    ui->callsignEdit->setText(callsign);
    ui->frequencyEdit->setValue(frequency);
    callsignChanged();
    stopContactTimer();
}

void NewContactWidget::setDefaultReport() {
    if (defaultReport.isEmpty()) {
        defaultReport = "599";
    }

    ui->rstRcvdEdit->setText(defaultReport);
    ui->rstSentEdit->setText(defaultReport);
}

NewContactWidget::~NewContactWidget() {
    writeSettings();
    delete ui;
}
