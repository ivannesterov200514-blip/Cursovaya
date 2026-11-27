#include "path_field.h"
#include "../core/files.h"
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <stdexcept>

using namespace std;
namespace fs = filesystem;

PathField::PathField(const QString& name, const QString& placeholder, const QString& filter, bool save, QWidget* parent)
    : QWidget(parent), edit(new QLineEdit(this)) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    edit->setObjectName(name);
    edit->setPlaceholderText(placeholder);
    layout->addWidget(edit, 1);
    auto* button = new QPushButton(save ? "Сохранить…" : "Выбрать…", this);
    layout->addWidget(button);

    connect(button, &QPushButton::clicked, this, [this, filter, save] {
        QString directory = text();
        if (directory.isEmpty())
            directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if (directory.isEmpty())
            directory = QDir::currentPath();
        const QString selected = save
            ? QFileDialog::getSaveFileName(this, "Выбрать путь сохранения", directory, filter, nullptr, QFileDialog::DontConfirmOverwrite)
            : QFileDialog::getOpenFileName(this, "Выбрать файл", directory, filter);
        if (!selected.isEmpty())
            setText(selected);
    });
    setFocusProxy(edit);
}

QLineEdit* PathField::input() const {
    return edit;
}

QString PathField::text() const {
    return edit->text().trimmed();
}

void PathField::setText(const QString& text) {
    edit->setText(text);
    edit->setCursorPosition(0);
}

fs::path fs_path(const QString& path) {
    return fs::u8path(path.toStdString());
}

QString qt_path(const fs::path& path) {
    return QString::fromStdString(path.u8string());
}

static void check_absolute(const QString& path) {
    if (path.isEmpty())
        throw runtime_error("Укажите путь к файлу");
    if (!QDir::isAbsolutePath(path))
        throw runtime_error("Укажите полный путь или выберите файл в диалоге");
}

fs::path input_path(const QString& path, const QString& suffix) {
    check_absolute(path);
    const QFileInfo file(path);
    if (!suffix.isEmpty() && file.suffix().compare(suffix, Qt::CaseInsensitive) != 0)
        throw runtime_error("Выберите файл ." + suffix.toStdString());
    const auto result = fs_path(path);
    if (!file_exists(result))
        throw runtime_error("Указанный файл не найден");
    return result;
}

fs::path output_path(const QString& path, const QString& suffix) {
    check_absolute(path);
    const QFileInfo typed(path);
    QString name = path;
    if (typed.suffix().isEmpty() && !suffix.isEmpty())
        name += "." + suffix;
    const QFileInfo file(name);
    if (!suffix.isEmpty() && file.suffix().compare(suffix, Qt::CaseInsensitive) != 0)
        throw runtime_error("Укажите имя результата с расширением ." + suffix.toStdString());
    if (!valid_file_name(file.fileName().toStdString()))
        throw runtime_error("Укажите допустимое имя файла результата");
    if (!file.absoluteDir().exists())
        throw runtime_error("Папка для сохранения не найдена");
    const auto result = fs_path(name);
    if (file_exists(result))
        throw runtime_error("Файл результата уже существует. Укажите другое имя");
    return result;
}