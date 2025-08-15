#include "networkmanager.h"
#include <QHostAddress>
#include <QTextStream>

NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent)
{
}

NetworkManager::~NetworkManager() {
    disconnectAll();
}

void NetworkManager::setConfig(const QString& ip, quint16 port) {
    m_ip = ip;
    m_port = port;
}

void NetworkManager::setRole(Role r) {
    m_role = r;
}

bool NetworkManager::startHosting() {
    cleanupServer();
    cleanupSocket();
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &NetworkManager::onNewConnection);
    if (!m_server->listen(QHostAddress::Any, m_port)) {
        emit error(QString("Listen failed: %1").arg(m_server->errorString()));
        cleanupServer();
        return false;
    }
    emit listening(m_server->serverPort());
    return true;
}

void NetworkManager::joinHost() {
    cleanupServer();
    cleanupSocket();
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &NetworkManager::onSocketError);
    connect(m_socket, &QTcpSocket::connected, this, [this]() {
        // Send role immediately after connection
        QString roleMsg = QString("ROLE %1").arg(m_role == Host ? "X" : "O");
        sendLine(roleMsg);
        // Notify the UI that the connection is established and verification is starting
        emit connected();
    });

    m_socket->connectToHost(QHostAddress(m_ip), m_port);
}

void NetworkManager::disconnectAll() {
    if (m_socket) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
        cleanupSocket();
    }
    if (m_server) {
        m_server->close();
        cleanupServer();
    }
    emit disconnected();
}

bool NetworkManager::isConnected() const {
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

QString NetworkManager::peerDescription() const {
    if (m_socket) {
        return QString("%1:%2").arg(m_socket->peerAddress().toString()).arg(m_socket->peerPort());
    }
    return "None";
}

void NetworkManager::sendLine(const QString& line) {
    if (!isConnected()) return;
    QByteArray data = line.toUtf8();
    data.append('\n');
    m_socket->write(data);
}

void NetworkManager::onNewConnection() {
    if (!m_server) return;

    // Accept only one connection
    if (m_socket) {
        QTcpSocket* extra = m_server->nextPendingConnection();
        if (extra) {
            extra->disconnectFromHost();
            extra->deleteLater();
        }
        return;
    }

    m_socket = m_server->nextPendingConnection();
    if (!m_socket) return;

    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &NetworkManager::onSocketError);

    // As the host, send our role to the client to initiate the handshake
    QString myRoleMsg = QString("ROLE %1").arg(m_role == Host ? "X" : "O");
    sendLine(myRoleMsg);

    emit connected();
}

void NetworkManager::onSocketReadyRead() {
    if (!m_socket) return;
    while (m_socket->canReadLine()) {
        QByteArray line = m_socket->readLine().trimmed();
        if (!line.isEmpty()) {
            QString message = QString::fromUtf8(line);

            if (message.startsWith("ROLE ")) {
                QString opponentMark = message.split(' ')[1];
                Role opponentRole = (opponentMark == "X") ? Host : Client;

                // Check for a role conflict (i.e., roles are the same)
                if (opponentRole == m_role) {
                    sendLine("ROLE_CONFLICT");
                    emit roleConflict();
                    return;
                } else {
                    // Roles are compatible, the handshake is successful
                    emit lineReceived("HELLO");
                }
            } else if (message == "ROLE_CONFLICT") {
                emit roleConflict();
                return;
            } else {
                emit lineReceived(message);
            }
        }
    }
}

void NetworkManager::onSocketDisconnected() {
    cleanupSocket();
    emit disconnected();
}

void NetworkManager::onSocketError(QAbstractSocket::SocketError socketError) {
    Q_UNUSED(socketError);
    if (m_socket) {
        emit error(m_socket->errorString());
    }
}

void NetworkManager::cleanupServer() {
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
}

void NetworkManager::cleanupSocket() {
    if (m_socket) {
        m_socket->disconnect();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}
