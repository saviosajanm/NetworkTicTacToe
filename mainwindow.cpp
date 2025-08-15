#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QHostAddress>
#include <QRandomGenerator>

static inline QString qcharToString(QChar c){ return QString(c); }

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , currentPlayer('X')
    , myMark('?')
{
    ui->setupUi(this);

    // Map board buttons
    buttons[0][0] = ui->btn00; buttons[0][1] = ui->btn01; buttons[0][2] = ui->btn02;
    buttons[1][0] = ui->btn10; buttons[1][1] = ui->btn11; buttons[1][2] = ui->btn12;
    buttons[2][0] = ui->btn20; buttons[2][1] = ui->btn21; buttons[2][2] = ui->btn22;

    for (int r=0;r<3;r++)
        for (int c=0;c<3;c++)
            connect(buttons[r][c], &QPushButton::clicked, this, &MainWindow::handleButtonClick);

    connect(ui->btnRematch, &QPushButton::clicked, this, &MainWindow::onRematchClicked);
    ui->btnRematch->setVisible(false);
    ui->btnRematch->setText("Rematch");

    setupMenus();

    ui->lblStatus->setStyleSheet("color: blue; font-weight: bold;");
    ui->lblStatus->setText("Local game. Choose Network → X or O to play.");

    // Footer status bar
    statusFooter = new QLabel(this);
    statusBar()->addPermanentWidget(statusFooter);
    updateFooterStatus();

    // Network signals
    connect(&net, &NetworkManager::roleConflict, this, &MainWindow::onRoleConflict);
    connect(&net, &NetworkManager::connected,    this, &MainWindow::onNetConnected);
    connect(&net, &NetworkManager::disconnected, this, &MainWindow::onNetDisconnected);
    connect(&net, &NetworkManager::lineReceived, this, &MainWindow::onNetLine);
    connect(&net, &NetworkManager::error,        this, &MainWindow::onNetError);
    connect(&net, &NetworkManager::listening,    this, [this](quint16 p) {
        port = p;
        updateFooterStatus();
        ui->lblStatus->setStyleSheet("color: blue; font-weight: bold;");
        ui->lblStatus->setText(QString("Listening on port %1").arg(p));
    });

    // Start timer
    startTimer.setSingleShot(true); // Ensure timer only fires once
    connect(&startTimer, &QTimer::timeout, this, &MainWindow::decideStartingPlayer);

    // Flash animation setup
    flashAnim = new QPropertyAnimation(this, "flashProgress", this);
    flashAnim->setDuration(300);
    flashAnim->setLoopCount(-1);
    flashAnim->setEasingCurve(QEasingCurve::InOutSine);
    connect(flashAnim, &QPropertyAnimation::valueChanged, this, [this](const QVariant &value) {
        double t = value.toDouble();
        QColor current = QColor::fromRgbF(
            flashDark.redF()   + (flashLight.redF()   - flashDark.redF())   * t,
            flashDark.greenF() + (flashLight.greenF() - flashDark.greenF()) * t,
            flashDark.blueF()  + (flashLight.blueF()  - flashDark.blueF())  * t
            );
        QString css = QString("color: %1; font-size: 26pt; font-weight: bold;").arg(current.name());
        for (auto cell : winningCells) {
            buttons[cell.first][cell.second]->setStyleSheet(css);
        }
    });
}

void MainWindow::onRoleConflict() {
    // Queue the disconnection to happen after this event finishes
    QTimer::singleShot(0, this, [this]() {
        QMessageBox::warning(this, "Role Conflict",
                             "Cannot connect - both players selected the same role. One must be X and the other O.");
        disconnectNetwork();
    });
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::setupMenus() {
    auto gameMenu = menuBar()->addMenu("&Game");
    auto actNew   = gameMenu->addAction("New Local Game");
    connect(actNew, &QAction::triggered, this, &MainWindow::newGame);
    gameMenu->addAction("Exit", this, &QWidget::close);

    auto netMenu  = menuBar()->addMenu("&Network");
    auto actX  = netMenu->addAction("Role: &X");
    connect(actX, &QAction::triggered, this, &MainWindow::setRoleX);
    auto actO = netMenu->addAction("Role: &O");
    connect(actO, &QAction::triggered, this, &MainWindow::setRoleO);
    netMenu->addAction("Set IP/Port…", this, &MainWindow::setIpPort);
    netMenu->addAction("Connect / Listen", this, &MainWindow::connectNetwork);
    netMenu->addAction("Disconnect", this, &MainWindow::disconnectNetwork);
}

void MainWindow::newGame() {
    net.disconnectAll();
    myMark = '?';
    currentPlayer = 'X';
    myTurn = true;

    rematchRequestedByMe = false;
    rematchRequestedByOpponent = false;
    ui->btnRematch->setText("Rematch");
    ui->btnRematch->setVisible(false);
    isStartingPlayerDecided = false;
    startingMark = '?';

    resetBoard();
    ui->lblStatus->setStyleSheet("color: blue; font-weight: bold;");
    ui->lblStatus->setText("New local game started.");
    updateFooterStatus();
}

void MainWindow::resetBoard() {
    stopFlashing();
    winningCells.clear();

    for (int r=0;r<3;r++)
        for (int c=0;c<3;c++) {
            buttons[r][c]->setText("");
            buttons[r][c]->setStyleSheet("");
            buttons[r][c]->setEnabled(true);
        }

    // Set current player
    if (net.role() != NetworkManager::None && net.isConnected()) {
        if (startingMark == '?') {
            // Wait for starting player decision
            currentPlayer = '?';
            myTurn = false;
            setBoardEnabled(false);
            ui->lblStatus->setStyleSheet("color: blue; font-weight: bold;");
            ui->lblStatus->setText("Starting new round...");
        } else {
            currentPlayer = startingMark;
            myTurn = (myMark == currentPlayer);
            setBoardEnabled(myTurn);
        }
    } else {
        // Local game - random start
        currentPlayer = QRandomGenerator::global()->bounded(2) ? 'X' : 'O';
        myMark = '?';
        myTurn = true;
    }

    ui->btnRematch->setVisible(false);
    ui->btnRematch->setText("Rematch");

    updateStatus();
    updateFooterStatus();
}

void MainWindow::decideStartingPlayer() {
    if (!isStartingPlayerDecided) {
        // Random 50:50 chance for who starts
        startingMark = QRandomGenerator::global()->bounded(2) ? 'X' : 'O';
        isStartingPlayerDecided = true;

        // Send to opponent
        if (net.isConnected()) {
            sendStartingPlayer(startingMark);
        }

        // Update self and board state
        resetBoard();
        updateStatus();
    }
}

void MainWindow::handleButtonClick() {
    QPushButton* b = qobject_cast<QPushButton*>(sender());
    if (!b || !b->text().isEmpty()) return;

    const bool networked = net.role() != NetworkManager::None && net.isConnected();
    if (networked && !myTurn) return;

    int rr=-1, cc=-1;
    for (int r=0;r<3;r++)
        for (int c=0;c<3;c++)
            if (buttons[r][c] == b) { rr=r; cc=c; }

    const QChar mark = networked ? myMark : currentPlayer;
    QString color;
    if (networked) {
        color = (mark == myMark) ? "green" : "red";
    } else {
        color = (mark == 'X') ? "purple" : "yellow";
    }
    b->setText(qcharToString(mark));
    b->setStyleSheet(QString("color: %1; font-size: 26pt; font-weight: bold;").arg(color));

    if (networked) {
        sendMove(rr, cc);
    }

    if (checkWinAtEndOfMove(mark)) {
        if (networked) {
            sendWin();
        }
        return;
    }

    if (networked) {
        myTurn = false;
        currentPlayer = (mark == 'X') ? 'O' : 'X';
        setBoardEnabled(false);
    } else {
        currentPlayer = (currentPlayer == 'X') ? 'O' : 'X';
    }
    updateStatus();
}

bool MainWindow::checkWinAtEndOfMove(const QChar& mark) {
    auto eq = [&](int r,int c){ return buttons[r][c]->text()==qcharToString(mark); };
    QList<QPair<int,int>> line;

    for (int r=0;r<3;r++) if (eq(r,0)&&eq(r,1)&&eq(r,2)) { line={{r,0},{r,1},{r,2}}; }
    for (int c=0;c<3;c++) if (eq(0,c)&&eq(1,c)&&eq(2,c)) { line={{0,c},{1,c},{2,c}}; }
    if (eq(0,0)&&eq(1,1)&&eq(2,2)) { line={{0,0},{1,1},{2,2}}; }
    if (eq(0,2)&&eq(1,1)&&eq(2,0)) { line={{0,2},{1,1},{2,0}}; }

    if (!line.isEmpty()) {
        winningCells = line;
        bool networked = net.role() != NetworkManager::None && net.isConnected();

        if (networked) {
            if (mark == myMark) {
                flashDark = QColor(0x00, 0x22, 0x00);
                flashLight = QColor(0x00, 0xFF, 0x88);
            } else {
                flashDark = QColor(0x33, 0x00, 0x00);
                flashLight = QColor(0xFF, 0x66, 0x66);
            }
        } else {
            if (mark == 'X') {
                flashDark = QColor(0x33, 0x00, 0x33);
                flashLight = QColor(0xFF, 0x99, 0xFF);
            } else {
                flashDark = QColor(0x66, 0x66, 0x00);
                flashLight = QColor(0xFF, 0xFF, 0x00);
            }
        }

        QString statusText;
        if (networked) {
            bool iWon = (mark == myMark);
            statusText = iWon ? "You WIN!" : "You LOSE!";
            ui->lblStatus->setStyleSheet(QString("color: %1; font-weight: bold;").arg(iWon ? "green" : "red"));
        } else {
            statusText = QString("%1 wins!").arg(mark);
            ui->lblStatus->setStyleSheet("color: blue; font-weight: bold;");
        }

        ui->lblStatus->setText(statusText);
        ui->btnRematch->setEnabled(true);
        ui->btnRematch->setVisible(true);
        ui->btnRematch->setText("Rematch");
        setBoardEnabled(false);

        if (flashAnim) {
            flashAnim->stop();
            flashAnim->setStartValue(0.0);
            flashAnim->setEndValue(1.0);
            flashAnim->start();
        }

        updateFooterStatus();
        return true;
    }

    if (boardFull()) {
        ui->lblStatus->setStyleSheet("color: blue; font-weight: bold;");
        ui->lblStatus->setText("It's a draw!");
        ui->btnRematch->setEnabled(true);
        ui->btnRematch->setVisible(true);
        ui->btnRematch->setText("Rematch");
        setBoardEnabled(false);
        updateFooterStatus();
        return true;
    }
    return false;
}

void MainWindow::stopFlashing() {
    if (flashAnim) flashAnim->stop();
    for (auto cell : winningCells) {
        buttons[cell.first][cell.second]->setStyleSheet("");
    }
    winningCells.clear();
}

bool MainWindow::boardFull() const {
    for (int r=0;r<3;r++)
        for (int c=0;c<3;c++)
            if (buttons[r][c]->text().isEmpty()) return false;
    return true;
}

void MainWindow::setBoardEnabled(bool on) {
    for (int r=0;r<3;r++)
        for (int c=0;c<3;c++)
            buttons[r][c]->setEnabled(on);
}

void MainWindow::setRoleX() {
    net.setRole(NetworkManager::Host);
    myMark='X';
    updateFooterStatus();
    ui->lblStatus->setStyleSheet("color: blue; font-weight: bold;");
    ui->lblStatus->setText("You are X. Click 'Connect/Listen' to start.");
}

void MainWindow::setRoleO() {
    net.setRole(NetworkManager::Client);
    myMark='O';
    updateFooterStatus();
    ui->lblStatus->setStyleSheet("color: blue; font-weight: bold;");
    ui->lblStatus->setText("You are O. Click 'Connect/Listen' to connect.");
}

void MainWindow::setIpPort() {
    bool ok=false;
    QString newIp = QInputDialog::getText(this,"IP","Enter IP:",QLineEdit::Normal,ip,&ok);
    if (!ok||newIp.isEmpty()) return;

    bool ok2=false;
    int p=QInputDialog::getInt(this,"Port","Enter port:",port,1024,65535,1,&ok2);
    if (!ok2) return;

    ip=newIp;
    port=static_cast<quint16>(p);
    net.setConfig(ip,port);
    updateFooterStatus();
}

void MainWindow::connectNetwork() {
    if (net.role()==NetworkManager::None) {
        QMessageBox::warning(this, "Role Not Set", "Please set your role (X or O) first.");
        return;
    }

    net.setConfig(ip,port);
    if (net.role()==NetworkManager::Host) {
        if(!net.startHosting()) {
            QMessageBox::critical(this, "Listen Error", "Failed to start listening. Check if port is available.");
        }
    } else {
        net.joinHost();
    }
    updateFooterStatus();
}

void MainWindow::disconnectNetwork() {
    net.disconnectAll();
    myTurn=true;
    myMark='?';
    isStartingPlayerDecided = false;
    startingMark = '?';

    rematchRequestedByMe = false;
    rematchRequestedByOpponent = false;
    ui->btnRematch->setText("Rematch");
    ui->btnRematch->setVisible(false);

    setBoardEnabled(true);
    updateStatus();
    updateFooterStatus();
}

void MainWindow::onNetConnected() {
    // Don't reset board yet - wait for role verification
    setBoardEnabled(false);
    isStartingPlayerDecided = false;
    startingMark = '?';

    ui->lblStatus->setStyleSheet("color: blue; font-weight: bold;");
    ui->lblStatus->setText("Verifying roles...");
    updateFooterStatus();
}

void MainWindow::onNetDisconnected() {
    setBoardEnabled(true);
    updateFooterStatus();
    ui->lblStatus->setStyleSheet("color: red; font-weight: bold;");
    ui->lblStatus->setText("Disconnected");
}

void MainWindow::onNetLine(const QString& line) {
    const QStringList parts=line.split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;
    const QString cmd=parts[0].toUpper();

    if (cmd=="MOVE" && parts.size()==3) {
        int r=parts[1].toInt(), c=parts[2].toInt();
        if (r<0||r>2||c<0||c>2) return;
        if (buttons[r][c]->text().isEmpty()) {
            QChar oppMark = (myMark=='X') ? 'O':'X';
            buttons[r][c]->setText(qcharToString(oppMark));
            buttons[r][c]->setStyleSheet("color: red; font-size: 26pt; font-weight: bold;");
            if (checkWinAtEndOfMove(oppMark)) {
                return;
            }
            myTurn=true;
            currentPlayer=myMark;
            setBoardEnabled(true);
            updateStatus();
        }
    } else if (cmd=="RESET") {
        rematchRequestedByMe = false;
        rematchRequestedByOpponent = false;
        ui->btnRematch->setText("Rematch");
        resetBoard();
        updateFooterStatus();
    } else if (cmd=="HELLO") {
        ui->lblStatus->setStyleSheet("color: blue; font-weight: bold;");
        ui->lblStatus->setText("Connection established! Game starts in 5 seconds...");
        if (net.role() == NetworkManager::Host) {
            startTimer.start(5000);
        }
    } else if (cmd=="REMATCH") {
        rematchRequestedByOpponent = true;
        if (rematchRequestedByMe) {
            // This code runs when both have agreed to a rematch
            rematchRequestedByMe = false;
            rematchRequestedByOpponent = false;
            ui->btnRematch->setText("Rematch");
            ui->btnRematch->setVisible(false); // Hide the button because the game is about to start

            // Reset the starting player state
            isStartingPlayerDecided = false;
            startingMark = '?';

            // Decide starting player and start immediately
            decideStartingPlayer();
        } else {
            ui->btnRematch->setVisible(true);
            ui->btnRematch->setText("Opponent requests rematch!");
            ui->btnRematch->setEnabled(true);
            ui->lblStatus->setStyleSheet("color: blue; font-weight: bold;");
        }
        updateFooterStatus();
    } else if (cmd=="WIN") {
        QChar oppMark = (myMark == 'X') ? 'O' : 'X';
        checkWinAtEndOfMove(oppMark);
    } else if (cmd=="START" && parts.size()==2) {
        QString markStr = parts[1];
        if (markStr=="X" || markStr=="O") {
            startingMark = markStr[0];
            isStartingPlayerDecided = true;
            resetBoard();
            updateStatus();
        }
    }
}

void MainWindow::onNetError(const QString& msg) {
    ui->lblStatus->setStyleSheet("color: red; font-weight: bold;");
    ui->lblStatus->setText("Network Error: " + msg);
    updateFooterStatus();
}

void MainWindow::sendHello() { net.sendLine("HELLO"); }
void MainWindow::sendMove(int r,int c) { net.sendLine(QString("MOVE %1 %2").arg(r).arg(c)); }
void MainWindow::sendReset() { net.sendLine("RESET"); }
void MainWindow::sendRematchRequest() { net.sendLine("REMATCH"); }
void MainWindow::sendWin() { net.sendLine("WIN"); }
void MainWindow::sendStartingPlayer(QChar mark) { net.sendLine(QString("START %1").arg(mark)); }

void MainWindow::updateStatus() {
    if (currentPlayer == '?') return; // Don't update status before game starts
    QString turn = QString("Turn: %1").arg(qcharToString(currentPlayer));
    ui->lblStatus->setStyleSheet("color: blue; font-weight: bold;");
    ui->lblStatus->setText(turn);
    updateFooterStatus();
}

void MainWindow::onRematchClicked() {
    if (net.role() == NetworkManager::None || !net.isConnected()) {
        resetBoard();
        return;
    }

    ui->btnRematch->setEnabled(true);

    if (rematchRequestedByOpponent) {
        // This code runs when we are accepting the opponent's rematch request
        rematchRequestedByOpponent = false;
        rematchRequestedByMe = false;
        ui->btnRematch->setText("Rematch");
        ui->btnRematch->setVisible(false); // Hide the button immediately

        sendRematchRequest(); // Send agreement to the opponent
        return;
    }

    rematchRequestedByMe = true;
    ui->btnRematch->setText("Waiting for opponent...");
    ui->btnRematch->setEnabled(false);
    sendRematchRequest();
    updateFooterStatus();
}

void MainWindow::updateFooterStatus() {
    QString roleText;
    switch(net.role()) {
    case NetworkManager::Host: roleText = "X"; break;
    case NetworkManager::Client: roleText = "O"; break;
    default: roleText = "None";
    }

    QString netStatus;
    if (net.isConnected()) {
        QString peer = net.peerDescription();
        netStatus = QString("Connected as %1 to %2")
                        .arg(roleText)
                        .arg(peer);
    } else {
        switch(net.role()) {
        case NetworkManager::Host:
            netStatus = QString("Listening as X on %1:%2").arg(ip).arg(port);
            break;
        case NetworkManager::Client:
            netStatus = QString("Connecting as O to %1:%2").arg(ip).arg(port);
            break;
        default:
            netStatus = "Disconnected";
        }
    }

    QString gameStatus = "Idle";
    if (rematchRequestedByMe)
        gameStatus = "Rematch requested";
    else if (rematchRequestedByOpponent)
        gameStatus = "Rematch pending";
    else if (!winningCells.isEmpty())
        gameStatus = "Game finished";
    else if (net.isConnected()) {
        if (isStartingPlayerDecided) {
            gameStatus = "Playing";
        } else {
            gameStatus = "Starting soon";
        }
    }

    statusFooter->setText(QString("IP: %1 | Port: %2 | Role: %3 | Game: %4")
                              .arg(ip)
                              .arg(port)
                              .arg(roleText)
                              .arg(gameStatus));
}
