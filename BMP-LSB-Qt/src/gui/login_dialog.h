#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;

class LoginDialog : public QDialog {
public:
    explicit LoginDialog(bool first_run, QWidget* parent = nullptr);

private:
    void submit();

    bool first_run;
    QLineEdit* password;
    QLineEdit* repeat = nullptr;
    QLabel* status;
};