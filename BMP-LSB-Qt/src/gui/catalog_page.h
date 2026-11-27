#pragma once

#include "../core/records.h"

#include <QWidget>
#include <functional>

using namespace std;

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

class CatalogPage : public QWidget {
public:
    explicit CatalogPage(function<void(const Record&)> use_record, QWidget* parent = nullptr);

private:
    void run(const function<void()>& operation);
    void refresh();
    void show_records(const vector<Record>& records);
    void set_status(const QString& message, bool error = false);
    void update_buttons();
    Record current_record() const;

    function<void(const Record&)> use_record;
    QComboBox* source;
    QLineEdit* search;
    QLineEdit* filter;
    QTableWidget* table;
    QLabel* status;
    QPushButton* edit_button;
    QPushButton* delete_button;
    QPushButton* use_button;
};