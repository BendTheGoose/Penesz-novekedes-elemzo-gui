#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QImage>
#include <QPixmap>
#include <QPen>
#include <QGraphicsTextItem>
#include <QGraphicsSceneMouseEvent>
#include <QVariant>
#include <QGraphicsItem>
#include <algorithm>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

//grafikun kattintas cucc
class InteraktivVaszon : public QGraphicsScene {
public:
    QTreeWidget* faWidget;
    InteraktivVaszon(QTreeWidget* fa, QObject* parent = nullptr)
        : QGraphicsScene(parent), faWidget(fa) {}

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
        QGraphicsItem *elem = itemAt(event->scenePos(), QTransform());
        if (elem) {
            QVariant adat = elem->data(0);
            if (adat.isValid()) {
                QTreeWidgetItem* faElem = static_cast<QTreeWidgetItem*>(adat.value<void*>());
                if (faElem) {
                    faWidget->setCurrentItem(faElem);
                }
            }
        }
        QGraphicsScene::mousePressEvent(event);
    }
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);


    ui->idoMezo->setDateTime(QDateTime::currentDateTime());
    ui->idoMezo->setCalendarPopup(true);
    ui->meretMezo->setSingleStep(1.0);
    ui->hoMezo->setSingleStep(0.5);
    ui->meretMezo->setRange(0.0, 10000.0);
    ui->hoMezo->setRange(-20.0, 100.0);
    ui->listaWidget->setWordWrap(true);

    setWindowTitle("Slime Mold Tracker - Kísérleti Napló");


    ui->listaWidget->setColumnCount(2);
    ui->listaWidget->setHeaderLabels(QStringList() << "Név / Idő" << "Részletek");
    ui->listaWidget->setSortingEnabled(true);


    kepVaszon = new InteraktivVaszon(ui->listaWidget, this);
    ui->kepNezet->setScene(kepVaszon);

    grafikonVaszon = new InteraktivVaszon(ui->listaWidget, this);
    ui->grafikonNezet->setScene(grafikonVaszon);

    ideiglenesKepUtvonal = "";
    mentveVolt = true;
}

MainWindow::~MainWindow()
{
    qDeleteAll(kiserletek);
    qDeleteAll(meresekMap);
    delete ui;
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!mentveVolt) {
        QMessageBox::StandardButton valasz = QMessageBox::question(this, "Kilépés",
                                                                   "Vannak nem mentett változtatások. Szeretné elmenteni őket?",
                                                                   QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (valasz == QMessageBox::Yes) {
            mentesFajlba();
            event->accept();
        } else if (valasz == QMessageBox::Cancel) {
            event->ignore();
        } else {
            event->accept();
        }
    } else {
        event->accept();
    }
}


void MainWindow::on_gombUjKiserlet_clicked()
{
    if (ui->nevMezo->text().isEmpty()) {
        QMessageBox::warning(this, "Hiba", "Előbb adj egy nevet a kísérletnek!");
        return;
    }

    Kiserlet *kiserlet = new Kiserlet();
    kiserlet->nev = ui->nevMezo->text();
    kiserlet->tapanyag = ui->tapanyagMezo->text();

    QTreeWidgetItem *elem = new QTreeWidgetItem(ui->listaWidget);
    elem->setFlags(elem->flags() | Qt::ItemIsEditable);
    elem->setText(0, kiserlet->nev);
    elem->setText(1, "Tápanyag: " + kiserlet->tapanyag);

    ui->listaWidget->addTopLevelItem(elem);
    kiserletek.insert(elem, kiserlet);

    ui->nevMezo->clear();
    ui->tapanyagMezo->clear();
    mentveVolt = false;
}


void MainWindow::on_gombKepToltes_clicked()
{
    QString ujKep = QFileDialog::getOpenFileName(this, "Kép kiválasztása", "", "Képek (*.png *.jpg *.jpeg)");
    if (ujKep.isEmpty()) return;

    QList<QTreeWidgetItem*> kijeloltek = ui->listaWidget->selectedItems();


    if (!kijeloltek.isEmpty() && meresekMap.contains(kijeloltek.first())) {
        QTreeWidgetItem* meresElem = kijeloltek.first();
        Meres* meres = meresekMap[meresElem];
        meres->kepUtvonal = ujKep;
        mentveVolt = false;


        if (meres->meret == 0) {
            kepElemzes(meres);

            QString reszletek = QString("%1 mm² | %2 °C | %3").arg(meres->meret, 6, 'f', 2, QChar(' ')).arg(meres->homerseklet, 0, 'f', 1).arg(meres->megjegyzes);
            meresElem->setText(1, reszletek);
        }

        // Megjelenítés
        QPixmap pixmap(ujKep);
        kepVaszon->clear();
        kepVaszon->addPixmap(pixmap.scaled(ui->kepNezet->width()-5, ui->kepNezet->height()-5, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        grafikonFrissites(kiserletek[meresElem->parent()]);

        QMessageBox::information(this, "Siker", "Kép sikeresen csatolva a meglévő méréshez!");
    }

    else {
        ideiglenesKepUtvonal = ujKep;
        QPixmap pixmap(ideiglenesKepUtvonal);
        kepVaszon->clear();
        kepVaszon->addPixmap(pixmap.scaled(ui->kepNezet->width()-5, ui->kepNezet->height()-5, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}


void MainWindow::on_gombUjMeres_clicked()
{
    QList<QTreeWidgetItem*> kijeloltek = ui->listaWidget->selectedItems();
    if (kijeloltek.isEmpty()) {
        QMessageBox::warning(this, "Hiba", "Kérlek, először jelölj ki egy kísérletet!");
        return;
    }

    QTreeWidgetItem *szuloElem = kijeloltek.first();
    if (szuloElem->parent() != nullptr) szuloElem = szuloElem->parent();

    if (!kiserletek.contains(szuloElem)) {
        QMessageBox::warning(this, "Hiba", "Nem kísérletet jelöltél ki!");
        return;
    }

    Kiserlet *kiserlet = kiserletek[szuloElem];
    Meres *meres = new Meres();

    meres->ido = ui->idoMezo->dateTime();
    meres->meret = ui->meretMezo->value();
    meres->homerseklet = ui->hoMezo->value();
    meres->megjegyzes = ui->megjegyzesMezo->toPlainText();
    meres->kepUtvonal = ideiglenesKepUtvonal;

    if (!meres->kepUtvonal.isEmpty() && meres->meret == 0) {
        kepElemzes(meres);
    }

    kiserlet->meresek.append(meres);

    QTreeWidgetItem *meresElem = new QTreeWidgetItem(szuloElem);
    meresElem->setFlags(meresElem->flags() | Qt::ItemIsEditable);
    meresElem->setText(0, meres->ido.toString("yyyy.MM.dd hh:mm"));
    QString reszletek = QString("%1 mm² | %2 °C | %3").arg(meres->meret, 6, 'f', 2, QChar(' ')).arg(meres->homerseklet, 0, 'f', 1).arg(meres->megjegyzes);
    meresElem->setText(1, reszletek);

    meresekMap.insert(meresElem, meres);
    szuloElem->setExpanded(true);

    ideiglenesKepUtvonal = "";
    ui->megjegyzesMezo->clear();
    kepVaszon->clear();
    grafikonFrissites(kiserlet);
    mentveVolt = false;
}


void MainWindow::on_gombTorles_clicked()
{
    QList<QTreeWidgetItem*> kijeloltek = ui->listaWidget->selectedItems();
    if (kijeloltek.isEmpty()) return;

    QTreeWidgetItem *elem = kijeloltek.first();

    if (kiserletek.contains(elem)) {
        Kiserlet *kiserlet = kiserletek.take(elem);

        qDeleteAll(kiserlet->meresek);
        delete kiserlet;
    } else if (meresekMap.contains(elem)) {
        Meres *meres = meresekMap.take(elem);

        Kiserlet *kiserlet = kiserletek[elem->parent()];
        kiserlet->meresek.removeOne(meres);
        delete meres;
    }

    delete elem;
    kepVaszon->clear();
    grafikonVaszon->clear();
    mentveVolt = false;
}


void MainWindow::on_listaWidget_itemSelectionChanged()
{
    QList<QTreeWidgetItem*> kijeloltek = ui->listaWidget->selectedItems();
    kepVaszon->clear();
    grafikonVaszon->clear();

    if (kijeloltek.isEmpty()) return;
    QTreeWidgetItem *elem = kijeloltek.first();

    if (meresekMap.contains(elem)) {
        Meres *meres = meresekMap[elem];
        if (!meres->kepUtvonal.isEmpty()) {
            QPixmap pixmap(meres->kepUtvonal);
            if (!pixmap.isNull()) {
                kepVaszon->addPixmap(pixmap.scaled(ui->kepNezet->width()-5, ui->kepNezet->height()-5, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
        grafikonFrissites(kiserletek[elem->parent()]);
    }
    else if (kiserletek.contains(elem)) {
        grafikonFrissites(kiserletek[elem]);

        int xEltolas = 10, yEltolas = 10;
        int kiskepMeret = 80, tavolsag = 10;
        int maxSzelesseg = ui->kepNezet->width() - 20;

        for (int i = 0; i < elem->childCount(); ++i) {
            QTreeWidgetItem* gyerek = elem->child(i);
            Meres* m = meresekMap[gyerek];
            if (m && !m->kepUtvonal.isEmpty()) {
                QPixmap pix(m->kepUtvonal);
                if (!pix.isNull()) {
                    QPixmap kiskep = pix.scaled(kiskepMeret, kiskepMeret, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    QGraphicsPixmapItem* kepElem = kepVaszon->addPixmap(kiskep);
                    kepElem->setPos(xEltolas, yEltolas);
                    kepElem->setData(0, QVariant::fromValue((void*)gyerek));

                    xEltolas += kiskepMeret + tavolsag;
                    if (xEltolas + kiskepMeret > maxSzelesseg) {
                        xEltolas = 10; yEltolas += kiskepMeret + tavolsag;
                    }
                }
            }
        }
    }
}


void MainWindow::grafikonFrissites(Kiserlet* kiserlet)
{
    grafikonVaszon->clear();
    if (!kiserlet) return;

    QTreeWidgetItem* kiserletElem = kiserletek.key(kiserlet);
    if (!kiserletElem || kiserletElem->childCount() == 0) return;

    QPen tengelyToll(Qt::black, 2);
    QPen vonalToll(Qt::green, 3);

    int szelesseg = ui->grafikonNezet->width() - 80;
    int magassag = ui->grafikonNezet->height() - 60;
    int kezdX = 50, kezdY = 20;

    grafikonVaszon->addLine(kezdX, magassag + kezdY, kezdX + szelesseg + 20, magassag + kezdY, tengelyToll);
    grafikonVaszon->addLine(kezdX, kezdY - 10, kezdX, magassag + kezdY, tengelyToll);

    QGraphicsTextItem *xFelirat = grafikonVaszon->addText("Mérések sorszáma (Idő)");
    xFelirat->setPos(kezdX + szelesseg / 2 - 70, magassag + kezdY + 5);
    QGraphicsTextItem *yFelirat = grafikonVaszon->addText("Méret\n(%)");
    yFelirat->setPos(0, 0);

    QList<QPair<Meres*, QTreeWidgetItem*>> meresLista;
    for (int i = 0; i < kiserletElem->childCount(); ++i) {
        QTreeWidgetItem* gyerek = kiserletElem->child(i);
        if (meresekMap.contains(gyerek)) meresLista.append(qMakePair(meresekMap[gyerek], gyerek));
    }

    std::sort(meresLista.begin(), meresLista.end(), [](const QPair<Meres*, QTreeWidgetItem*>& a, const QPair<Meres*, QTreeWidgetItem*>& b) {
        return a.first->ido < b.first->ido;
    });

    double minMeret = meresLista.first().first->meret;
    double maxMeret = minMeret;
    for (const auto& par : meresLista) {
        if (par.first->meret > maxMeret) maxMeret = par.first->meret;
        if (par.first->meret < minMeret) minMeret = par.first->meret;
    }
    if (maxMeret == minMeret) maxMeret += 1;

    QTreeWidgetItem* jelenlegiKijelolt = ui->listaWidget->currentItem();
    QPointF utolsoPont;
    bool elso = true;
    int darab = meresLista.size();

    for (int i = 0; i < darab; ++i) {
        Meres* m = meresLista[i].first;
        QTreeWidgetItem* mElem = meresLista[i].second;

        double x = kezdX;
        if (darab > 1) x = kezdX + ((double)i / (darab - 1)) * szelesseg;
        double y = kezdY + magassag - ((m->meret - minMeret) / (maxMeret - minMeret)) * magassag;

        QColor pontSzin = (mElem == jelenlegiKijelolt) ? Qt::red : Qt::darkGreen;
        QGraphicsEllipseItem* pont = grafikonVaszon->addEllipse(x - 6, y - 6, 12, 12, QPen(pontSzin), QBrush(pontSzin));
        pont->setData(0, QVariant::fromValue((void*)mElem));

        QGraphicsTextItem *ertekFelirat = grafikonVaszon->addText(QString::number(m->meret, 'f', 1));
        ertekFelirat->setPos(x - 12, y - 25);
        ertekFelirat->setDefaultTextColor(Qt::darkGray);

        if (!elso) grafikonVaszon->addLine(utolsoPont.x(), utolsoPont.y(), x, y, vonalToll);
        utolsoPont = QPointF(x, y);
        elso = false;
    }
}


void MainWindow::kepElemzes(Meres* meres)
{
    QImage kep(meres->kepUtvonal);
    if (kep.isNull()) return;

    int gombaPixelek = 0;
    int osszesPixel = kep.width() * kep.height();

    for (int y = 0; y < kep.height(); ++y) {
        for (int x = 0; x < kep.width(); ++x) {
            QRgb szin = kep.pixel(x, y);
            if (qRed(szin) < 50 && qGreen(szin) < 50 && qBlue(szin) < 50) gombaPixelek++;
            else if (qRed(szin) > 150 && qGreen(szin) > 150 && qBlue(szin) < 100) gombaPixelek++;
        }
    }

    double lefedettseg = ((double)gombaPixelek / osszesPixel) * 100.0;
    meres->meret = lefedettseg;

    QString uzenet = QString("Képelemzés kész!\n\nLefedett terület: %1%\n").arg(lefedettseg, 0, 'f', 2);

    QTreeWidgetItem *jelenlegiElem = ui->listaWidget->currentItem();
    if (jelenlegiElem && jelenlegiElem->parent() != nullptr) jelenlegiElem = jelenlegiElem->parent();

    if (jelenlegiElem && kiserletek.contains(jelenlegiElem)) {
        Kiserlet* kiserlet = kiserletek[jelenlegiElem];
        if (kiserlet->meresek.size() > 1) {

            QVector<Meres*> rendezett = kiserlet->meresek;
            std::sort(rendezett.begin(), rendezett.end(), [](Meres* a, Meres* b) { return a->ido < b->ido; });


            int index = rendezett.indexOf(meres);
            if (index > 0) {
                double elozoMeret = rendezett[index - 1]->meret;
                double kulonbseg = meres->meret - elozoMeret;
                double novekedes = (kulonbseg / elozoMeret) * 100.0;

                uzenet += "\n--- Növekedés ---\n";

                if (kulonbseg > 0) uzenet += QString("Nőtt: +%1 mm² (+%2%)").arg(kulonbseg, 0, 'f', 2).arg(novekedes, 0, 'f', 1);

                else if (kulonbseg < 0) uzenet += QString("Zsugorodott: %1 mm² (%2%)").arg(kulonbseg, 0, 'f', 2).arg(novekedes, 0, 'f', 1);

                else uzenet += "Nem változott.";
            }
        }
    }
    QMessageBox::information(this, "Eredmény", uzenet);
}


void MainWindow::on_gombMentes_clicked() { mentesFajlba(); }

void MainWindow::on_gombBetoltes_clicked() { betoltesFajlbol(); }

void MainWindow::mentesFajlba()
{
    QJsonArray kiserletekTomb;
    for (Kiserlet* kiserlet : std::as_const(kiserletek)) {
        QJsonObject kiserletObj;
        kiserletObj["nev"] = kiserlet->nev;
        kiserletObj["tapanyag"] = kiserlet->tapanyag;

        QJsonArray meresekTomb;
        for (const Meres* m : std::as_const(kiserlet->meresek)) {
            QJsonObject mObj;
            mObj["ido"] = m->ido.toSecsSinceEpoch();
            mObj["meret"] = m->meret;
            mObj["homerseklet"] = m->homerseklet;
            mObj["megjegyzes"] = m->megjegyzes;
            mObj["kepUtvonal"] = m->kepUtvonal;
            meresekTomb.append(mObj);
        }
        kiserletObj["meresek"] = meresekTomb;
        kiserletekTomb.append(kiserletObj);
    }

    QJsonDocument doc(kiserletekTomb);
    QFile file("labor_naplo.json");

    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        mentveVolt = true;
        QMessageBox::information(this, "Mentés", "Az adatok sikeresen elmentve a 'labor_naplo.json' fájlba!");
    } else {
        QMessageBox::warning(this, "Hiba", "Nem sikerült menteni a fájlt!");
    }
}

void MainWindow::betoltesFajlbol()
{
    QFile file("labor_naplo.json");
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Infó", "Még nincs korábban elmentett kísérleti napló!");
        return;
    }

    if (!mentveVolt) {
        if (QMessageBox::question(this, "Betöltés", "A jelenlegi nem mentett adatok elvesznek. Folytatod?") != QMessageBox::Yes) {
            return;
        }
    }


    ui->listaWidget->clear();
    qDeleteAll(kiserletek); kiserletek.clear();
    meresekMap.clear();
    kepVaszon->clear(); grafikonVaszon->clear();

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonArray kiserletekTomb = doc.array();

    for (int i = 0; i < kiserletekTomb.size(); ++i) {
        QJsonObject kiserletObj = kiserletekTomb[i].toObject();
        Kiserlet *kiserlet = new Kiserlet();
        kiserlet->nev = kiserletObj["nev"].toString();
        kiserlet->tapanyag = kiserletObj["tapanyag"].toString();

        QTreeWidgetItem *kiserletElem = new QTreeWidgetItem(ui->listaWidget);
        kiserletElem->setFlags(kiserletElem->flags() | Qt::ItemIsEditable);
        kiserletElem->setText(0, kiserlet->nev);
        kiserletElem->setText(1, "Tápanyag: " + kiserlet->tapanyag);
        ui->listaWidget->addTopLevelItem(kiserletElem);
        kiserletek.insert(kiserletElem, kiserlet);

        QJsonArray meresekTomb = kiserletObj["meresek"].toArray();
        for (int j = 0; j < meresekTomb.size(); ++j) {
            QJsonObject mObj = meresekTomb[j].toObject();
            Meres *meres = new Meres();
            meres->ido = QDateTime::fromSecsSinceEpoch(mObj["ido"].toVariant().toLongLong());
            meres->meret = mObj["meret"].toDouble();
            meres->homerseklet = mObj["homerseklet"].toDouble();
            meres->megjegyzes = mObj["megjegyzes"].toString();
            meres->kepUtvonal = mObj["kepUtvonal"].toString();

            kiserlet->meresek.append(meres);

            QTreeWidgetItem *meresElem = new QTreeWidgetItem(kiserletElem);
            meresElem->setFlags(meresElem->flags() | Qt::ItemIsEditable);
            meresElem->setText(0, meres->ido.toString("yyyy.MM.dd hh:mm"));
            QString reszletek = QString("%1 mm² | %2 °C | %3").arg(meres->meret, 6, 'f', 2, QChar(' ')).arg(meres->homerseklet, 0, 'f', 1).arg(meres->megjegyzes);
            meresElem->setText(1, reszletek);
            meresekMap.insert(meresElem, meres);
        }
    }
    mentveVolt = true;
    QMessageBox::information(this, "Betöltés", "Az adatok sikeresen betöltve!");
}
