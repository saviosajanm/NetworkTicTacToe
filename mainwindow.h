#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QColor>
#include <QLabel>
#include <QTimer>
#include "networkmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // UI actions
    void newGame();
    void resetBoard();
    void handleButtonClick();
    void setRoleX();
    void setRoleO();
    void setIpPort();
    void connectNetwork();
    void disconnectNetwork();

    // Network signals
    void onNetConnected();
    void onNetDisconnected();
    void onNetLine(const QString& line);
    void onNetError(const QString& msg);

    // Rematch
    void onRematchClicked();

    // Starting player
    void decideStartingPlayer();

    void onRoleConflict();

private:
    Ui::MainWindow *ui;

    // Board
    QPushButton* buttons[3][3];
    QChar currentPlayer;
    QChar myMark;
    bool myTurn = true;

    // Network config
    QString ip = "127.0.0.1";
    quint16 port = 5050;
    NetworkManager net;

    // Start timer
    QTimer startTimer;

    // Winning line flash
    QList<QPair<int,int>> winningCells;
    QPropertyAnimation *flashAnim = nullptr;
    QColor flashDark, flashLight;

    // Rematch tracking
    bool rematchRequestedByMe = false;
    bool rematchRequestedByOpponent = false;

    // Footer status
    QLabel *statusFooter = nullptr;

    QChar startingMark = '?';
    bool isStartingPlayerDecided = false;

    // helpers
    void setupMenus();
    bool checkWinAtEndOfMove(const QChar& mark);
    bool boardFull() const;
    void setBoardEnabled(bool on);
    void stopFlashing();
    void updateStatus();         // updates the main label showing "Turn: X" etc.
    void updateFooterStatus();   // updates footer with IP/port/network/game status

    // network helpers
    void sendHello();
    void sendMove(int r, int c);
    void sendReset();
    void sendRematchRequest();
    void sendWin();
    void sendStartingPlayer(QChar mark);
};

#endif // MAINWINDOW_H
