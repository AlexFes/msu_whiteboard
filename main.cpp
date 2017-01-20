#include <QApplication>
#include <QWidget>
#include "whitebrd.h"

int main(int argc, char *argv[]) {
    
    QApplication app(argc, argv);

    WhiteBoard window;

    // window.resize(800, 600);
    window.setWindowTitle("White Board");
    window.showMaximized();

    return app.exec();
}
