#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>

class ErrorDialog : public QDialog {
    Q_OBJECT
public:
    explicit ErrorDialog(const QString &title, const QStringList &errors, QWidget *parent = nullptr)
        : QDialog(parent) {
        setWindowTitle("Error Details");

        QVBoxLayout *layout = new QVBoxLayout(this);

        QLabel *titleLabel = new QLabel(title, this);
        QFont titleFont = titleLabel->font();
        titleFont.setPointSize(14);  // larger font size
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        layout->addWidget(titleLabel);

        QPlainTextEdit *errorText = new QPlainTextEdit(this);
        errorText->setPlainText(errors.join("\n\n"));
        errorText->setReadOnly(true);
        errorText->setMinimumSize(600, 300); // adjust size as needed
        layout->addWidget(errorText);

        QPushButton *closeButton = new QPushButton("Close", this);
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
        layout->addWidget(closeButton);

        setLayout(layout);
    }
};
