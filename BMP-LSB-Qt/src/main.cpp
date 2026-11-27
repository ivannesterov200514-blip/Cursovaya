#include "core/auth.h"
#include "core/files.h"
#include "gui/login_dialog.h"
#include "gui/main_window.h"
#include "gui/path_field.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStyleFactory>
#include <exception>
#include <stdexcept>

using namespace std;

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName("NSTU");
    app.setApplicationName("BMP-LSB");
    app.setStyle(QStyleFactory::create("Fusion"));

    QFile style(":/interface.qss");
    if (style.open(QIODevice::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(style.readAll()));

    try {
        const QString data_dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (data_dir.isEmpty() || !QDir().mkpath(data_dir) || !QDir::setCurrent(data_dir))
            throw runtime_error("Не удалось открыть рабочую папку программы");
        const WorkspaceLock lock;
        if (!QDir().mkpath("files"))
            throw runtime_error("Не удалось создать папку для файлов каталога");

        LoginDialog dialog(!account_exists());
        if (dialog.exec() != QDialog::Accepted)
            return 0;

        MainWindow window;
        window.show();
        return app.exec();
    } catch (const exception& error) {
        QMessageBox::critical(nullptr, "Ошибка запуска BMP-LSB", QString::fromUtf8(error.what()));
        return 1;
    }
}