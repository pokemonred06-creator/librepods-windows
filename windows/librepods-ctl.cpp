#include <QCoreApplication>
#include <QLocalSocket>
#include <QTextStream>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    if (argc < 2) {
        QTextStream(stderr) << "Usage: librepods-ctl <command>\n"
                            << "Commands:\n"
                            << "  noise:off           Disable noise control\n"
                            << "  noise:anc           Enable Active Noise Cancellation\n"
                            << "  noise:transparency  Enable Transparency mode\n"
                            << "  noise:adaptive      Enable Adaptive mode\n"
                            << "  spatial:on          Enable Spatial Audio Head Tracking\n"
                            << "  spatial:off         Disable Spatial Audio Head Tracking\n"
                            << "  spatial:recenter    Recenter Head Tracking Origin\n";
        return 1;
    }

    QString command = QString::fromUtf8(argv[1]);
    if (argc >= 3) {
        command = QString("%1:%2").arg(QString::fromUtf8(argv[1]), QString::fromUtf8(argv[2]));
    }

    QLocalSocket socket;
    socket.connectToServer("app_server");

    if (!socket.waitForConnected(1000)) {
        QTextStream(stderr) << "Could not connect to librepods (is it running?)\n";
        return 1;
    }

    socket.write(command.toUtf8());
    socket.flush();
    socket.waitForBytesWritten(500);
    socket.disconnectFromServer();
    return 0;
}
