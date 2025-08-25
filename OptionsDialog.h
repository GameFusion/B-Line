#pragma once

#include <QDialog>

class QCheckBox;

class OptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OptionsDialog(QWidget *parent = nullptr);

    bool isMultiLayerSelectionEnabled() const;

private:
    QCheckBox *multiLayerSelectionCheckBox;
};
