#include "main_window.h"
#include "catalog_page.h"
#include "path_field.h"
#include "../core/bmp.h"
#include "../core/files.h"
#include "../core/stego.h"

#include <QCloseEvent>
#include <QFutureWatcher>
#include <QImage>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QStringDecoder>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>
#include <exception>
#include <fstream>
#include <vector>

using namespace std;

namespace fs = filesystem;

static QLabel* label(const QString& title, const char* name, QWidget* parent) {
    auto* result = new QLabel(title, parent);
    result->setObjectName(name);
    result->setWordWrap(true);
    return result;
}

static void add_path(QVBoxLayout* layout, const QString& title, PathField* path) {
    auto* name = new QLabel(title, path->parentWidget());
    name->setObjectName("fieldLabel");
    name->setBuddy(path->input());
    layout->addWidget(name);
    layout->addWidget(path);
}

static QFrame* panel(QWidget* parent) {
    auto* result = new QFrame(parent);
    result->setObjectName("panel");
    return result;
}

static QVBoxLayout* panel_layout(QFrame* panel) {
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(19, 16, 19, 16);
    layout->setSpacing(8);
    return layout;
}

static void status_message(QLabel* status, const QString& message, bool error = false) {
    status->setProperty("error", error);
    status->style()->unpolish(status);
    status->style()->polish(status);
    status->setText(message);
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("BMP-LSB — скрытие данных в изображениях");
    resize(1080, 760);
    setMinimumSize(890, 690);

    auto* root = new QWidget(this);
    root->setObjectName("root");
    auto* main_layout = new QVBoxLayout(root);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);
    setCentralWidget(root);

    auto* banner = new QFrame(root);
    banner->setObjectName("banner");
    auto* brand = new QVBoxLayout(banner);
    brand->setContentsMargins(28, 18, 28, 17);
    brand->setSpacing(2);
    brand->addWidget(label("BMP-LSB", "brand", banner));
    brand->addWidget(label("Стеганография в BMP-изображениях", "brandCaption", banner));
    main_layout->addWidget(banner);

    tabs = new QTabWidget(root);
    tabs->setObjectName("workspaceTabs");
    tabs->setDocumentMode(true);
    tabs->addTab(make_hide_page(), "Скрытие");
    tabs->addTab(make_extract_page(), "Извлечение");
    tabs->addTab(new CatalogPage([this](const Record& record) { open_record(record); }, tabs), "Каталог");
    main_layout->addWidget(tabs, 1);
}

QWidget* MainWindow::make_hide_page() {
    auto* page = new QWidget(tabs);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(26, 17, 26, 22);
    layout->setSpacing(12);
    layout->addWidget(label("Скрыть сообщение", "pageTitle", page));

    auto* columns = new QHBoxLayout;
    columns->setSpacing(16);
    layout->addLayout(columns, 1);
    auto* fields = panel(page);
    auto* form = panel_layout(fields);
    columns->addWidget(fields, 3);
    auto* image = panel(page);
    auto* preview_layout = panel_layout(image);
    columns->addWidget(image, 2);

    cover = new PathField("coverPath", "Полный путь к изображению", "BMP (*.bmp)", false, fields);
    add_path(form, "Исходное изображение", cover);

    auto* modes = new QHBoxLayout;
    auto* manual_mode = new QRadioButton("Ввести текст", fields);
    manual_mode->setObjectName("manualMode");
    manual_mode->setChecked(true);
    file_mode = new QRadioButton("Загрузить файл", fields);
    file_mode->setObjectName("fileMode");
    modes->addWidget(manual_mode);
    modes->addWidget(file_mode);
    modes->addStretch();
    form->addSpacing(6);
    form->addLayout(modes);

    payload_pages = new QStackedWidget(fields);
    message = new QPlainTextEdit(payload_pages);
    message->setObjectName("manualText");
    message->setAccessibleName("Текст сообщения");
    message->setPlaceholderText("Введите текст, который нужно скрыть…");
    message->setMinimumHeight(91);
    message->setTabChangesFocus(true);
    payload_pages->addWidget(message);

    auto* file_page = new QWidget(payload_pages);
    auto* file_layout = new QVBoxLayout(file_page);
    file_layout->setContentsMargins(0, 0, 0, 0);
    text_file = new PathField("textPath", "Полный путь к TXT или файлу данных", "Текстовые файлы (*.txt);;Все файлы (*)", false, file_page);
    file_layout->addWidget(text_file);
    file_layout->addStretch();
    payload_pages->addWidget(file_page);
    form->addWidget(payload_pages, 1);

    hide_output = new PathField("hideOutputPath", "Полный путь к новому BMP", "BMP (*.bmp)", true, fields);
    form->addSpacing(6);
    add_path(form, "Куда сохранить BMP", hide_output);

    auto* hide_button = new QPushButton("Скрыть и сохранить", fields);
    hide_button->setObjectName("hideButton");
    hide_button->setProperty("primary", true);
    auto* submit = new QHBoxLayout;
    submit->addStretch();
    submit->addWidget(hide_button);
    form->addSpacing(5);
    form->addLayout(submit);

    preview_layout->addWidget(label("Исходное BMP", "cardTitle", image));
    preview = label("Выберите BMP для предпросмотра", "imagePreview", image);
    preview->setObjectName("imagePreview");
    preview->setAlignment(Qt::AlignCenter);
    preview->setMinimumSize(255, 170);
    preview_layout->addWidget(preview, 1);
    cover_size = label("Размер и вместимость появятся после выбора BMP.", "muted", image);
    preview_layout->addWidget(cover_size);
    byte_count = label("Размер сообщения: 0 байт.", "fieldLabel", image);
    byte_count->setObjectName("byteCount");
    preview_layout->addWidget(byte_count);

    hide_status = label("", "operationStatus", page);
    hide_status->setMinimumHeight(42);
    layout->addWidget(hide_status);

    auto* delay = new QTimer(page);
    delay->setSingleShot(true);
    delay->setInterval(250);
    connect(cover->input(), &QLineEdit::textChanged, this, [delay] { delay->start(); });
    connect(delay, &QTimer::timeout, this, &MainWindow::update_preview);
    connect(message, &QPlainTextEdit::textChanged, this, &MainWindow::update_capacity);
    connect(text_file->input(), &QLineEdit::textChanged, this, &MainWindow::update_capacity);
    connect(file_mode, &QRadioButton::toggled, this, [this](bool enabled) {
        payload_pages->setCurrentIndex(enabled ? 1 : 0);
        update_capacity();
    });
    connect(hide_button, &QPushButton::clicked, this, &MainWindow::hide_message);
    return page;
}

QWidget* MainWindow::make_extract_page() {
    auto* page = new QWidget(tabs);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(26, 17, 26, 22);
    layout->setSpacing(12);
    layout->addWidget(label("Извлечь сообщение", "pageTitle", page));

    auto* columns = new QHBoxLayout;
    columns->setSpacing(16);
    layout->addLayout(columns, 1);
    auto* fields = panel(page);
    auto* form = panel_layout(fields);
    columns->addWidget(fields, 3);
    auto* preview_panel = panel(page);
    auto* preview_layout = panel_layout(preview_panel);
    columns->addWidget(preview_panel, 2);

    stego_image = new PathField("stegoImagePath", "Полный путь к BMP со скрытыми данными", "BMP (*.bmp)", false, fields);
    add_path(form, "Изображение со скрытыми данными", stego_image);
    form->addSpacing(6);
    extract_output = new PathField("extractOutputPath", "Полный путь к новому файлу", "Текстовые файлы (*.txt);;Все файлы (*)", true, fields);
    add_path(form, "Куда сохранить извлечённые данные", extract_output);
    form->addStretch();

    auto* button = new QPushButton("Извлечь и сохранить", fields);
    button->setObjectName("extractButton");
    button->setProperty("primary", true);
    auto* submit = new QHBoxLayout;
    submit->addStretch();
    submit->addWidget(button);
    form->addLayout(submit);

    preview_layout->addWidget(label("Извлечённый текст", "cardTitle", preview_panel));
    extracted_text = new QPlainTextEdit(preview_panel);
    extracted_text->setObjectName("recoveredPreview");
    extracted_text->setReadOnly(true);
    extracted_text->setPlaceholderText("Здесь появится предпросмотр текста UTF-8.");
    preview_layout->addWidget(extracted_text, 1);

    extract_status = label("", "operationStatus", page);
    extract_status->setMinimumHeight(42);
    layout->addWidget(extract_status);
    connect(button, &QPushButton::clicked, this, &MainWindow::extract_message);
    return page;
}

static QImage make_thumbnail(ifstream& file, const Bmp& bmp) {
    QSize size(static_cast<int>(bmp.width), static_cast<int>(bmp.height));
    if (size.width() > 286 || size.height() > 218)
        size.scale(286, 218, Qt::KeepAspectRatio);
    size.setWidth(max(1, size.width()));
    size.setHeight(max(1, size.height()));
    QImage image(size, QImage::Format_RGB888);

    // Для предпросмотра читаем только строки, которые попадают в миниатюру.
    // Если даже одна строка слишком длинная, читаем только нужные пиксели.
    constexpr uint64_t ROW_LIMIT = 128 * 1024;
    vector<char> row(bmp.row_bytes <= ROW_LIMIT ? static_cast<size_t>(bmp.row_bytes) : 0);
    for (int y = 0; y < size.height(); y++) {
        const auto original_y = static_cast<uint64_t>(y) * bmp.height / size.height();
        const auto stored_y = bmp.top_down ? original_y : bmp.height - 1 - original_y;
        const auto offset = bmp.pixel_offset + stored_y * bmp.row_stride;
        if (!row.empty()) {
            file.seekg(static_cast<streamoff>(offset));
            if (!file.read(row.data(), static_cast<streamsize>(row.size())))
                return {};
        }
        auto* line = image.scanLine(y);
        for (int x = 0; x < size.width(); x++) {
            const auto column = static_cast<uint64_t>(x) * bmp.width / size.width() * 3;
            char sample[3];
            if (row.empty()) {
                file.seekg(static_cast<streamoff>(offset + column));
                if (!file.read(sample, sizeof(sample)))
                    return {};
            }
            const char* bgr = row.empty() ? sample : row.data() + column;
            line[x * 3] = static_cast<unsigned char>(bgr[2]);
            line[x * 3 + 1] = static_cast<unsigned char>(bgr[1]);
            line[x * 3 + 2] = static_cast<unsigned char>(bgr[0]);
        }
    }
    return image;
}

void MainWindow::update_preview() {
    capacity.reset();
    preview->setText("Выберите BMP для предпросмотра");
    cover_size->setText("Размер и вместимость появятся после выбора BMP.");
    if (!cover->text().isEmpty()) {
        try {
            const auto path = input_path(cover->text(), "bmp");
            ifstream file(path, ios::binary);
            const Bmp bmp = read_bmp(file);
            const auto bytes = available_bytes(bmp);
            if (bytes >= STEGO_HEADER_SIZE)
                capacity = bytes - STEGO_HEADER_SIZE;
            const QImage thumbnail = make_thumbnail(file, bmp);
            if (!thumbnail.isNull()) {
                preview->setPixmap(QPixmap::fromImage(thumbnail).scaled(286, 218, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                preview->setText("Предпросмотр недоступен");
            }
            cover_size->setText(capacity
                ? QString("%1 × %2 px\nВместимость: %3 байт").arg(bmp.width).arg(bmp.height).arg(*capacity)
                : QString("%1 × %2 px\nЗаголовок сообщения не помещается").arg(bmp.width).arg(bmp.height));
        } catch (const exception& error) {
            cover_size->setText(QString::fromUtf8(error.what()));
        }
    }
    update_capacity();
}

void MainWindow::update_capacity() {
    qint64 size = 0;
    if (file_mode->isChecked()) {
        const QFileInfo file(text_file->text());
        size = file.isFile() ? file.size() : -1;
    } else {
        size = message->toPlainText().toUtf8().size();
    }
    if (size < 0) {
        status_message(byte_count, "Размер сообщения: —");
        return;
    }
    const bool too_big = capacity && static_cast<qulonglong>(size) > *capacity;
    status_message(byte_count, too_big
        ? QString("Размер сообщения: %1 байт. Превышена вместимость.").arg(size)
        : QString("Размер сообщения: %1 байт.").arg(size), too_big);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (busy)
        event->ignore();
    else
        QMainWindow::closeEvent(event);
}

struct TaskResult {
    ExtractedData data{};
    QString error;
};

void MainWindow::run_operation(QLabel* status, const QString& progress,
                               function<ExtractedData()> work,
                               function<void(const ExtractedData&)> finished) {
    busy = true;
    tabs->setEnabled(false);
    status_message(status, progress);
    auto* watcher = new QFutureWatcher<TaskResult>(this);
    connect(watcher, &QFutureWatcher<TaskResult>::finished, this, [this, status, finished, watcher] {
        const auto result = watcher->result();
        watcher->deleteLater();
        busy = false;
        tabs->setEnabled(true);
        if (result.error.isEmpty())
            finished(result.data);
        else
            status_message(status, result.error, true);
    });
    watcher->setFuture(QtConcurrent::run([work]() -> TaskResult {
        try {
            return {work(), {}};
        } catch (const exception& error) {
            return {{}, QString::fromUtf8(error.what())};
        }
    }));
}

void MainWindow::hide_message() {
    if (busy)
        return;
    try {
        const auto image = input_path(cover->text(), "bmp");
        const auto output = output_path(hide_output->text(), "bmp");
        const bool from_file = file_mode->isChecked();
        const auto payload = from_file ? input_path(text_file->text(), "") : fs::path{};
        const auto text = from_file ? string{} : message->toPlainText().toStdString();
        run_operation(hide_status, "Скрытие данных и запись BMP…", [image, output, payload, text, from_file] {
            const auto size = from_file ? hide_data(image, payload, output) : hide_text(image, text, output);
            return ExtractedData{size, {}};
        }, [this, output](const ExtractedData& result) {
            hide_output->setText(qt_path(output));
            stego_image->setText(qt_path(output));
            status_message(hide_status, QString("Скрыто %1 байт. BMP сохранён: %2") .arg(result.size).arg(qt_path(output)));
        });
    } catch (const exception& error) {
        status_message(hide_status, QString::fromUtf8(error.what()), true);
    }
}

void MainWindow::extract_message() {
    if (busy)
        return;
    extracted_text->clear();
    extracted_text->setPlaceholderText("Здесь появится предпросмотр текста UTF-8.");
    try {
        const auto image = input_path(stego_image->text(), "bmp");
        const QString typed = extract_output->text();
        const auto output = output_path(typed, QFileInfo(typed).suffix().isEmpty() ? "txt" : "");
        run_operation(extract_status, "Извлечение данных и запись файла…", [image, output] {
            constexpr size_t PREVIEW_LIMIT = 256 * 1024;
            return extract_data(image, output, PREVIEW_LIMIT);
        }, [this, output](const ExtractedData& data) {
            extract_output->setText(qt_path(output));
            QStringDecoder decoder(QStringDecoder::Utf8);
            const QString text = decoder(QByteArrayView(data.preview.data(), static_cast<qsizetype>(data.preview.size())));
            const bool binary = any_of(text.begin(), text.end(), [](QChar ch) {
                return ch.unicode() < 32 && ch != '\n' && ch != '\r' && ch != '\t';
            });
            if (decoder.hasError() || binary) {
                extracted_text->setPlaceholderText("Данные сохранены. Предпросмотр доступен для текста UTF-8.");
            } else if (data.size == 0) {
                extracted_text->setPlaceholderText("Скрытый файл пуст.");
            } else {
                extracted_text->setPlainText(text);
                if (data.size > data.preview.size())
                    extracted_text->appendPlainText("… Предпросмотр сокращён.");
            }
            status_message(extract_status, QString("Извлечено %1 байт. Файл сохранён: %2") .arg(data.size).arg(qt_path(output)));
        });
    } catch (const exception& error) {
        status_message(extract_status, QString::fromUtf8(error.what()), true);
    }
}

void MainWindow::open_record(const Record& record) {
    const auto image = fs::absolute(user_file(record.image, false));
    const auto data = fs::absolute(user_file(record.payload, false));
    cover->setText(qt_path(image));
    text_file->setText(qt_path(data));
    file_mode->setChecked(true);
    hide_output->setText("");
    status_message(hide_status, "");
    tabs->setCurrentIndex(0);
}