#include "optionsdialog.h"
#include <QVBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>

OptionsDialog::OptionsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Options"));
    setModal(true);
    resize(300, 150);

    multiLayerSelectionCheckBox = new QCheckBox(tr("Enable multi-layer stroke selection"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this
        );

    connect(buttonBox, &QDialogButtonBox::accepted, this, &OptionsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &OptionsDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(multiLayerSelectionCheckBox);
    layout->addWidget(buttonBox);
    setLayout(layout);
}

bool OptionsDialog::isMultiLayerSelectionEnabled() const
{
    return multiLayerSelectionCheckBox->isChecked();
}
