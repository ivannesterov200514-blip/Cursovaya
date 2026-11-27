#pragma once
#include <QWidget>
#include <filesystem>

using namespace std;

class QLineEdit;

class PathField : public QWidget {
public:
    PathField(const QString& name, const QString& placeholder, const QString& filter, bool save, QWidget* parent = nullptr);

    QLineEdit* input() const;
    QString text() const;
    void setText(const QString& text);

private:
    QLineEdit* edit;
};

filesystem::path fs_path(const QString& path);
QString qt_path(const filesystem::path& path);
filesystem::path input_path(const QString& path, const QString& suffix);
filesystem::path output_path(const QString& path, const QString& suffix);