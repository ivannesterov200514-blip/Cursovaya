#include "catalog_page.h"
#include "path_field.h"
#include "../core/files.h"
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace std;

static uint32_t read_id(const QString& value) {
    const QString text = value.trimmed();
    for (const QChar symbol : text) {
        if (symbol < '0' || symbol > '9')
            throw runtime_error("Номер записи должен быть целым положительным числом");
    }
    bool ok = false;
    const auto id = text.toULongLong(&ok);
    if (!ok || id == 0 || id > numeric_limits<uint32_t>::max())
        throw runtime_error("Номер записи должен быть в диапазоне от 1 до 4 294 967 295");
    return static_cast<uint32_t>(id);
}

struct ImportedFile {
    filesystem::path source;
    filesystem::path destination;
};

static string catalog_file_name(const QString& value, const QString& suffix, vector<ImportedFile>& imported) {
    if (value.isEmpty())
        throw runtime_error("Укажите имя файла или нажмите «Выбрать…»");

    if (!QDir::isAbsolutePath(value)) {
        string name = value.toStdString();
        user_file(name, false);
        if (!suffix.isEmpty() && QFileInfo(value).suffix().compare(suffix, Qt::CaseInsensitive) != 0)
            throw runtime_error("Укажите имя BMP-изображения");
        return name;
    }

    auto source = input_path(value, suffix);
    string name = source.filename().u8string();
    if (!valid_file_name(name))
        throw runtime_error("Это имя файла не подходит для каталога. Переименуйте файл и выберите его снова");

    auto destination = filesystem::path("files") / source.filename();
    if (file_exists(destination)) {
        user_file(name, false);
        if (filesystem::equivalent(source, destination))
            return name;
        for (const auto& file : imported) {
            if (file.destination == destination && filesystem::equivalent(source, file.source))
                return name;
        }
        throw runtime_error("В каталоге уже есть файл «" + name + "». Выберите его или переименуйте новый файл");
    }

    destination = user_file(name, true);
    QFile file(qt_path(source));
    if (!file.copy(qt_path(destination)))
        throw runtime_error("Не удалось скопировать «" + name + "»: " + file.errorString().toStdString());
    imported.push_back({move(source), move(destination)});
    return name;
}

static bool edit_record_form(QWidget* parent, Record& record, bool adding) {
    QDialog dialog(parent);
    dialog.setObjectName("recordDialog");
    dialog.setWindowTitle(adding ? "Добавить задание" : "Изменить задание");
    dialog.setMinimumWidth(550);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 20, 22, 20);
    auto* hint = new QLabel("Выберите файлы или введите их имена из папки каталога. "
                            "При сохранении выбранные файлы будут скопированы в эту папку.", &dialog);
    hint->setWordWrap(true);
    hint->setObjectName("muted");
    layout->addWidget(hint);

    auto* form = new QFormLayout;
    auto* id = new QLineEdit(adding ? QString() : QString::number(record.id), &dialog);
    auto* title = new QLineEdit(QString::fromStdString(record.title), &dialog);
    auto* image = new PathField("recordImage", "Имя файла или путь к BMP", "BMP (*.bmp)", false, &dialog);
    auto* payload = new PathField("recordPayload", "Имя файла или путь к данным", "Текст (*.txt);;Все файлы (*)", false, &dialog);
    image->setText(QString::fromStdString(record.image));
    payload->setText(QString::fromStdString(record.payload));
    id->setObjectName("recordId");
    title->setObjectName("recordTitle");
    id->setReadOnly(!adding);
    id->setMaxLength(10);
    form->addRow("Номер", id);
    form->addRow("Название", title);
    form->addRow("Исходный BMP", image);
    form->addRow("Файл данных", payload);
    layout->addLayout(form);

    auto* error = new QLabel(&dialog);
    error->setObjectName("loginError");
    error->setWordWrap(true);
    error->hide();
    layout->addWidget(error);
    auto* buttons = new QDialogButtonBox(&dialog);
    auto* save = buttons->addButton(adding ? "Добавить" : "Сохранить", QDialogButtonBox::AcceptRole);
    save->setObjectName("recordSave");
    save->setProperty("primary", true);
    buttons->addButton("Отмена", QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        vector<ImportedFile> imported;
        try {
            imported.reserve(2);
            Record updated{read_id(id->text()), title->text().toStdString(), image->text().toStdString(), payload->text().toStdString()};
            if (!valid_title(updated.title))
                throw runtime_error("Введите название длиной до 80 байт");
            if (adding || updated.image != record.image)
                updated.image = catalog_file_name(image->text(), "bmp", imported);
            if (adding || updated.payload != record.payload)
                updated.payload = catalog_file_name(payload->text(), "", imported);

            if (adding)
                add_record(updated);
            else
                edit_record(updated);
            record = move(updated);
            dialog.accept();
        } catch (const exception& ex) {
            QString message = QString::fromUtf8(ex.what());
            for (const auto& file : imported) {
                error_code cleanup_error;
                filesystem::remove(file.destination, cleanup_error);
                if (cleanup_error)
                    message += "\nНе удалось удалить копию " + qt_path(file.destination);
            }
            error->setText(message);
            error->show();
        }
    });
    return dialog.exec() == QDialog::Accepted;
}

CatalogPage::CatalogPage(function<void(const Record&)> use_record, QWidget* parent)
    : QWidget(parent), use_record(::move(use_record)) {
    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(26, 19, 26, 22);
    page->setSpacing(12);

    auto* header = new QLabel("Каталог заданий", this);
    header->setObjectName("pageTitle");
    page->addWidget(header);

    auto* controls = new QHBoxLayout;
    source = new QComboBox(this);
    source->setObjectName("catalogSource");
    source->addItems({"Исходный каталог", "Второй файл (отбор)"});
    auto* create = new QPushButton("Создать каталог", this);
    create->setObjectName("catalogCreate");
    auto* select = new QPushButton("Отобрать по BMP…", this);
    select->setObjectName("catalogSelect");
    auto* folder = new QPushButton("Папка файлов", this);
    controls->addWidget(source);
    controls->addWidget(create);
    controls->addWidget(select);
    controls->addStretch();
    controls->addWidget(folder);
    page->addLayout(controls);

    auto* queries = new QHBoxLayout;
    search = new QLineEdit(this);
    search->setObjectName("catalogSearch");
    search->setPlaceholderText("Номер записи");
    auto* find = new QPushButton("Найти", this);
    find->setObjectName("catalogFind");
    filter = new QLineEdit(this);
    filter->setObjectName("catalogFilter");
    filter->setPlaceholderText("Фрагмент названия");
    auto* apply_filter = new QPushButton("Фильтровать", this);
    apply_filter->setObjectName("catalogApplyFilter");
    auto* reset = new QPushButton("Все записи", this);
    reset->setObjectName("catalogReset");
    queries->addWidget(search, 1);
    queries->addWidget(find);
    queries->addSpacing(10);
    queries->addWidget(filter, 2);
    queries->addWidget(apply_filter);
    queries->addWidget(reset);
    page->addLayout(queries);

    table = new QTableWidget(0, 4, this);
    table->setObjectName("catalogTable");
    table->setHorizontalHeaderLabels({"Номер", "Название", "Исходный BMP", "Файл данных"});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->verticalHeader()->hide();
    table->sortByColumn(0, Qt::AscendingOrder);
    table->setSortingEnabled(true);
    page->addWidget(table, 1);

    auto* actions = new QHBoxLayout;
    auto* add = new QPushButton("Добавить", this);
    add->setObjectName("catalogAdd");
    edit_button = new QPushButton("Изменить", this);
    edit_button->setObjectName("catalogEdit");
    delete_button = new QPushButton("Удалить", this);
    delete_button->setObjectName("catalogDelete");
    use_button = new QPushButton("Открыть для скрытия", this);
    use_button->setObjectName("catalogUse");
    use_button->setProperty("primary", true);
    actions->addWidget(add);
    actions->addWidget(edit_button);
    actions->addWidget(delete_button);
    actions->addStretch();
    actions->addWidget(use_button);
    page->addLayout(actions);

    status = new QLabel(this);
    status->setWordWrap(true);
    page->addWidget(status);

    connect(source, &QComboBox::currentIndexChanged, this, [this, find] {
        find->setEnabled(source->currentIndex() == 0);
        search->setEnabled(source->currentIndex() == 0);
        search->clear();
        filter->clear();
        run([this] { refresh(); });
    });
    connect(table, &QTableWidget::itemSelectionChanged, this, [this] { update_buttons(); });
    connect(create, &QPushButton::clicked, this, [this] {
        run([this] {
            create_records();
            source->setCurrentIndex(0);
            refresh();
            set_status("Исходный каталог создан.");
        });
    });
    connect(select, &QPushButton::clicked, this, [this] {
        const QString name = QInputDialog::getText(this, "Отбор во второй файл", "Точное имя исходного BMP (с учётом регистра)");
        if (name.isEmpty())
            return;
        run([this, name] {
            if (!valid_file_name(name.toStdString()))
                throw runtime_error("Укажите имя BMP без пути");
            const auto count = select_records(name.toStdString());
            source->setCurrentIndex(1);
            refresh();
            set_status(QString("Во второй файл записано заданий: %1.").arg(count));
        });
    });
    connect(folder, &QPushButton::clicked, this, [this] {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(QDir("files").absolutePath())))
            set_status("Не удалось открыть папку файлов каталога.", true);
    });
    connect(find, &QPushButton::clicked, this, [this] {
        run([this] {
            Record record;
            if (find_record(read_id(search->text()), record)) {
                show_records({record});
                set_status("Запись найдена.");
            } else {
                show_records({});
                set_status("Запись с таким номером не найдена.");
            }
        });
    });
    connect(search, &QLineEdit::returnPressed, find, &QPushButton::click);
    connect(apply_filter, &QPushButton::clicked, this, [this] {
        run([this] {
            auto rows = load_records(source->currentIndex() == 0 ? PRIMARY_FILE : SELECTED_FILE);
            const auto part = filter->text().toStdString();
            rows.erase(remove_if(rows.begin(), rows.end(), [&part](const Record& record) {
                return record.title.find(part) == string::npos;
            }), rows.end());
            show_records(rows);
            set_status(QString("Найдено заданий: %1.").arg(rows.size()));
        });
    });
    connect(filter, &QLineEdit::returnPressed, apply_filter, &QPushButton::click);
    connect(reset, &QPushButton::clicked, this, [this] {
        search->clear();
        filter->clear();
        run([this] { refresh(); });
    });
    connect(add, &QPushButton::clicked, this, [this] {
        Record record;
        if (edit_record_form(this, record, true)) {
            source->setCurrentIndex(0);
            run([this] { refresh(); set_status("Запись добавлена."); });
        }
    });
    connect(edit_button, &QPushButton::clicked, this, [this] {
        run([this] {
            auto record = current_record();
            if (edit_record_form(this, record, false)) {
                refresh();
                set_status("Запись изменена.");
            }
        });
    });
    connect(delete_button, &QPushButton::clicked, this, [this] {
        run([this] {
            const auto record = current_record();
            QMessageBox dialog(QMessageBox::Question, "Удалить запись", QString("Удалить задание № %1?").arg(record.id), QMessageBox::NoButton, this);
            auto* remove = dialog.addButton("Удалить", QMessageBox::AcceptRole);
            auto* cancel = dialog.addButton("Отмена", QMessageBox::RejectRole);
            dialog.setDefaultButton(cancel);
            dialog.exec();
            if (dialog.clickedButton() != remove)
                return;
            delete_record(record.id);
            refresh();
            set_status("Запись удалена.");
        });
    });
    connect(use_button, &QPushButton::clicked, this, [this] {
        run([this] { this->use_record(current_record()); });
    });

    run([this] { refresh(); });
}

void CatalogPage::run(const function<void()>& operation) {
    try {
        operation();
    } catch (const exception& error) {
        set_status(QString::fromUtf8(error.what()), true);
    }
}

void CatalogPage::refresh() {
    show_records({});
    const char* file = source->currentIndex() == 0 ? PRIMARY_FILE : SELECTED_FILE;
    if (!file_exists(file)) {
        set_status(source->currentIndex() == 0 ? "Создайте исходный каталог, чтобы добавлять задания."
                                              : "Сначала выполните отбор по имени BMP.");
        return;
    }
    const auto rows = load_records(file);
    show_records(rows);
    set_status(rows.empty() ? "В каталоге пока нет записей." : QString("Всего записей: %1.").arg(rows.size()));
}

void CatalogPage::show_records(const vector<Record>& records) {
    table->setSortingEnabled(false);
    table->clearContents();
    table->setRowCount(static_cast<int>(records.size()));
    for (int row = 0; row < table->rowCount(); row++) {
        const auto& record = records[static_cast<size_t>(row)];
        auto* id = new QTableWidgetItem;
        id->setData(Qt::DisplayRole, record.id);
        table->setItem(row, 0, id);
        table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(record.title)));
        table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(record.image)));
        table->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(record.payload)));
    }
    table->setSortingEnabled(true);
    update_buttons();
}

void CatalogPage::set_status(const QString& message, bool error) {
    status->setObjectName(error ? "loginError" : "muted");
    status->style()->unpolish(status);
    status->style()->polish(status);
    status->setText(message);
}

void CatalogPage::update_buttons() {
    const bool selected = table->currentRow() >= 0 && !table->selectedItems().isEmpty();
    edit_button->setEnabled(selected && source->currentIndex() == 0);
    delete_button->setEnabled(selected && source->currentIndex() == 0);
    use_button->setEnabled(selected);
}

Record CatalogPage::current_record() const {
    const int row = table->currentRow();
    if (row < 0 || table->selectedItems().isEmpty())
        throw runtime_error("Выберите запись в таблице");
    return {table->item(row, 0)->data(Qt::DisplayRole).toUInt(),
            table->item(row, 1)->text().toStdString(),
            table->item(row, 2)->text().toStdString(),
            table->item(row, 3)->text().toStdString()};
}