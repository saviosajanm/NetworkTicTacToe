#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

class NetworkManager : public QObject {
    Q_OBJECT
public:
    enum Role { None, Host, Client };
    Q_ENUM(Role)

    explicit NetworkManager(QObject* parent = nullptr);
    ~NetworkManager();

    void setConfig(const QString& ip, quint16 port);
    void setRole(Role r);
    Role role() const { return m_role; }

    // Actions
    bool startHosting();   // Host: listen
    void joinHost();       // Client: connect
    void disconnectAll();

    bool isConnected() const;
    QString peerDescription() const;

    // Send one logical line (will append '\n')
    void sendLine(const QString& line);

signals:
    void connected();
    void disconnected();
    void lineReceived(const QString& line);
    void error(const QString& message);
    void listening(quint16 port);
    void roleConflict();

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    Role m_role = None;
    QString m_ip = "127.0.0.1";
    quint16 m_port = 5050;

    QTcpServer* m_server = nullptr; // only for Host
    QTcpSocket* m_socket = nullptr; // the active connection

    void cleanupServer();
    void cleanupSocket();
};

#endif // NETWORKMANAGER_H
