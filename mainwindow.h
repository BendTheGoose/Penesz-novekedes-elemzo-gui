#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidgetItem>
#include <QGraphicsScene>
#include <QMap>
#include <QVector>
#include <QDateTime>
#include <QCloseEvent>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE


struct Meres {
    QDateTime ido;
    double meret;
    double homerseklet;
    QString megjegyzes;
    QString kepUtvonal;
};

struct Kiserlet {
    QString nev;
    QString tapanyag;
    QVector<Meres*> meresek;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:

    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_gombUjKiserlet_clicked();
    void on_gombUjMeres_clicked();
    void on_gombTorles_clicked();
    void on_gombKepToltes_clicked();
    void on_listaWidget_itemSelectionChanged();

    // ÚJ: Mentés és betöltés gombok
    void on_gombMentes_clicked();
    void on_gombBetoltes_clicked();

private:
    Ui::MainWindow *ui;

    QGraphicsScene *kepVaszon;
    QGraphicsScene *grafikonVaszon;

    QString ideiglenesKepUtvonal;
    bool mentveVolt;


    QMap<QTreeWidgetItem*, Kiserlet*> kiserletek;
    QMap<QTreeWidgetItem*, Meres*> meresekMap;

    void grafikonFrissites(Kiserlet* kiserlet);
    void kepElemzes(Meres* meres);
    void mentesFajlba();
    void betoltesFajlbol();
};
#endif // MAINWINDOW_H
