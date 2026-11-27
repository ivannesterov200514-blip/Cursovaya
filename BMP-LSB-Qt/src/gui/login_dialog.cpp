#include "login_dialog.h"
#include "../core/auth.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <exception>

using namespace std;

LoginDialog::LoginDialog(bool first_run, QWidget* parent)
    : QDialog(parent), first_run(first_run), password(new QLineEdit(this)), status(new QLabel(this)) {
    setObjectName("loginDialog");
    setWindowTitle(first_run ? "Настройка пароля — BMP-LSB" : "Вход — BMP-LSB");
    setMinimumWidth(430);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(12);

    auto* title = new QLabel("BMP-LSB", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);
    auto* description = new QLabel(first_run ? "Задайте пароль для работы с программой. Длина — от 8 до 128 байт."
                  : "Введите пароль, чтобы открыть проект.", this);
    description->setObjectName("muted");
    description->setWordWrap(true);
    layout->addWidget(description);

    password->setObjectName("password");
    password->setPlaceholderText(first_run ? "Новый пароль" : "Пароль");
    password->setEchoMode(QLineEdit::Password);
    password->setMaxLength(128);
    layout->addWidget(password);
    if (first_run) {
        repeat = new QLineEdit(this);
        repeat->setObjectName("repeatPassword");
        repeat->setPlaceholderText("Повторите пароль");
        repeat->setEchoMode(QLineEdit::Password);
        repeat->setMaxLength(128);
        layout->addWidget(repeat);
    }

    status->setObjectName("loginError");
    status->setWordWrap(true);
    status->hide();
    layout->addWidget(status);
    auto* buttons = new QDialogButtonBox(this);
    auto* submit_button = buttons->addButton(first_run ? "Сохранить и войти" : "Войти", QDialogButtonBox::AcceptRole);
    submit_button->setObjectName("loginSubmit");
    submit_button->setProperty("primary", true);
    buttons->addButton("Выход", QDialogButtonBox::RejectRole);
    submit_button->setDefault(true);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &LoginDialog::submit);
    connect(buttons, &QDialogButtonBox::rejected, this, &LoginDialog::reject);
    password->setFocus();
}

void LoginDialog::submit() {
    try {
        const auto value = password->text().toStdString();
        if (first_run) {
            if (!valid_password(value)) {
                status->setText("Введите пароль длиной от 8 до 128 байт.");
            } else if (password->text() != repeat->text()) {
                status->setText("Пароли не совпадают.");
                repeat->setFocus();
                repeat->selectAll();
            } else {
                register_account(value);
                accept();
                return;
            }
        } else if (check_password(value)) {
            accept();
            return;
        } else {
            status->setText("Неверный пароль.");
            password->selectAll();
        }
    } catch (const exception& error) {
        status->setText(QString::fromUtf8(error.what()));
    }
    status->show();
}