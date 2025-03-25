#include <QApplication>
#include "SerialSender.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    SerialSender serialSender;
    serialSender.show();

    return app.exec();
}
