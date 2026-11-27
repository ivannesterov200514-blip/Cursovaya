#pragma once

#include "../core/records.h"

#include <QMainWindow>
#include <cstdint>
#include <functional>
#include <optional>

using namespace std;

struct ExtractedData;
class QCloseEvent;
class QLabel;
class QPlainTextEdit;
class QRadioButton;
class QStackedWidget;
class QTabWidget;
class PathField;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QWidget* make_hide_page();
    QWidget* make_extract_page();
    void closeEvent(QCloseEvent* event) override;
    void update_preview();
    void update_capacity();
    void hide_message();
    void extract_message();
    void open_record(const Record& record);
    void run_operation(QLabel* status, const QString& progress,
                       function<ExtractedData()> work,
                       function<void(const ExtractedData&)> finished);

    QTabWidget* tabs;
    PathField* cover;
    PathField* text_file;
    PathField* hide_output;
    QRadioButton* file_mode;
    QStackedWidget* payload_pages;
    QPlainTextEdit* message;
    QLabel* preview;
    QLabel* cover_size;
    QLabel* byte_count;
    QLabel* hide_status;
    optional<uint64_t> capacity;
    bool busy = false;

    PathField* stego_image;
    PathField* extract_output;
    QPlainTextEdit* extracted_text;
    QLabel* extract_status;
};