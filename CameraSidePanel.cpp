#include "CameraSidePanel.h"
#include <QInputDialog>
#include <QDragEnterEvent>
#include <QDropEvent>

CameraSidePanel::CameraSidePanel(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setupConnections();
}

void CameraSidePanel::setupUI() {
    listWidget = new QListWidget(this);
    listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    listWidget->setDefaultDropAction(Qt::MoveAction);
    listWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    addButton = new QPushButton("➕");
    delButton = new QPushButton("➖");
    dupButton = new QPushButton("⧉");

    auto buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(delButton);
    buttonLayout->addWidget(dupButton);

    auto layout = new QVBoxLayout(this);
    layout->addLayout(buttonLayout);
    layout->addWidget(listWidget);
    //layout->addStretch();


    // Setup camera attribute widgets
    formContainer = new QWidget(this);
    formLayout = new QFormLayout(formContainer);
    formContainer->setLayout(formLayout);

    layout->addWidget(formContainer); // add form under list

    layout->addStretch();
    setLayout(layout);
}

void CameraSidePanel::clearForm() {
    QLayoutItem* item;
    while ((item = formLayout->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    nameEdit = nullptr;
    xEdit = yEdit = zoomEdit = rotationEdit = nullptr;
    easingCombo = nullptr;
    //uuidLabel = nullptr;
    uuidSelected.clear();
}

void CameraSidePanel::populateForm(const GameFusion::CameraFrame& frame) {
    clearForm();

    nameEdit = new QLineEdit(QString::fromStdString(frame.name));
    xEdit = new QDoubleSpinBox(); xEdit->setRange(-10000, 10000); xEdit->setValue(frame.x);
    yEdit = new QDoubleSpinBox(); yEdit->setRange(-10000, 10000); yEdit->setValue(frame.y);
    zoomEdit = new QDoubleSpinBox(); zoomEdit->setRange(0.01, 200.0); zoomEdit->setValue(frame.zoom*100.0);
    rotationEdit = new QDoubleSpinBox(); rotationEdit->setRange(-360, 360); rotationEdit->setValue(frame.rotation);
    frameOffset = new QSpinBox(); frameOffset->setValue(frame.frameOffset);
    easingCombo = new QComboBox();
    easingCombo->addItems({"Linear", "EaseIn", "EaseOut", "EaseInOut", "Bezier", "Cut"});
    easingCombo->setCurrentIndex(static_cast<int>(frame.easing));

    uuidSelected = QString::fromStdString(frame.uuid);

    formLayout->addRow("Name", nameEdit);
    formLayout->addRow("X", xEdit);
    formLayout->addRow("Y", yEdit);
    formLayout->addRow("Zoom", zoomEdit);
    formLayout->addRow("Rotation", rotationEdit);
    formLayout->addRow("Frame Offset", frameOffset);
    formLayout->addRow("Easing", easingCombo);

    // Optional: connect editingFinished() or valueChanged() to a save callback
    connect(nameEdit, &QLineEdit::editingFinished, this, &CameraSidePanel::onAttributeChanged);
    connect(xEdit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CameraSidePanel::onAttributeChanged);
    connect(yEdit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CameraSidePanel::onAttributeChanged);
    connect(zoomEdit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CameraSidePanel::onAttributeChanged);
    connect(rotationEdit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CameraSidePanel::onAttributeChanged);
    connect(frameOffset, QOverload<int>::of(&QSpinBox::valueChanged), this, &CameraSidePanel::onAttributeChanged);
    connect(easingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CameraSidePanel::onAttributeChanged);
}

void CameraSidePanel::onAttributeChanged() {

    QString uuid = uuidSelected;

    if(!this->frameMap.contains(uuid))
        return;

    GameFusion::CameraFrame currentFrame = this->frameMap[uuid];

    currentFrame.name = nameEdit->text().toStdString();
    currentFrame.x = static_cast<float>(xEdit->value());
    currentFrame.y = static_cast<float>(yEdit->value());
    currentFrame.zoom = static_cast<float>(zoomEdit->value()/100.0);
    currentFrame.rotation = static_cast<float>(rotationEdit->value());
    currentFrame.frameOffset = static_cast<int>(frameOffset->value());
    currentFrame.easing = static_cast<GameFusion::EasingType>(easingCombo->currentIndex());

    emit cameraFrameUpdated(currentFrame);
}

void CameraSidePanel::setupConnections() {
    connect(addButton, &QPushButton::clicked, this, &CameraSidePanel::onAddCamera);
    connect(delButton, &QPushButton::clicked, this, &CameraSidePanel::onDeleteCamera);
    connect(dupButton, &QPushButton::clicked, this, &CameraSidePanel::onDuplicateCamera);
    connect(listWidget, &QListWidget::itemDoubleClicked, this, &CameraSidePanel::onRenameCamera);
    connect(listWidget->model(), &QAbstractItemModel::rowsMoved, this, &CameraSidePanel::onItemDropped);
    connect(listWidget, &QListWidget::currentTextChanged, this, &CameraSidePanel::onSelectionChanged);


}


void CameraSidePanel::setCameraList(const QStringList& cameraNames) {
    listWidget->clear();
    for (const QString& name : cameraNames) {
        auto item = new QListWidgetItem(name, listWidget);
        // Optionally set icon, color, etc.
        listWidget->addItem(item);
    }
}

void CameraSidePanel::setCameraList(const QString& panelUuid, const std::vector<GameFusion::CameraFrame>& frames) {
    frameMap.clear();
    currentPanel = panelUuid;
    listWidget->clear();
    clearForm();

    int cameraIndex = 0;

    for (const auto& frame : frames) {
        // Only import camera frames for this panel
        if (QString::fromStdString(frame.panelUuid) == panelUuid) {

            cameraIndex ++;
            QString cameraName = frame.name.c_str();
            if(cameraName.isEmpty())
                cameraName = QString("Camera %1").arg(cameraIndex);

            auto item = new QListWidgetItem(cameraName, listWidget);
            // Optionally set icon, color, etc.
            item->setData(Qt::UserRole, QString::fromStdString(frame.uuid)); // 🔑 Store uuid
            listWidget->addItem(item);

            frameMap[QString::fromStdString(frame.uuid)] = frame;
        }
    }


}

void CameraSidePanel::selectPanel(const QString& panelName) {
    currentPanel = panelName;
    // reload panel's cameras (you'd call your data source here)
}

QString CameraSidePanel::selectedCamera() const {
    auto item = listWidget->currentItem();
    return item ? item->text() : QString();
}

void CameraSidePanel::onAddCamera() {
    emit cameraAdded();
}

void CameraSidePanel::onDeleteCamera() {
    if (auto item = listWidget->currentItem()) {
        emit cameraDeleted(item->text());
        delete item;
    }
}

void CameraSidePanel::onDuplicateCamera() {
    if (auto item = listWidget->currentItem()) {
        emit cameraDuplicated(item->text());
    }
}

void CameraSidePanel::onRenameCamera(QListWidgetItem* item) {
    startInlineRename(item);
}

void CameraSidePanel::startInlineRename(QListWidgetItem* item) {
    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename Camera"),
                                            tr("New name:"), QLineEdit::Normal,
                                            item->text(), &ok);
    if (ok && !newName.isEmpty() && newName != item->text()) {
        emit cameraRenamed(item->text(), newName);
        item->setText(newName);
    }
}

void CameraSidePanel::onSelectionChanged() {
    if (auto item = listWidget->currentItem()) {

        QString uuid = item->data(Qt::UserRole).toString();
        if (frameMap.contains(uuid)) {
            populateForm(frameMap[uuid]);
        }

        uuidSelected = uuid;

        emit cameraSelected(uuidSelected);
    }
}

/*
void CameraSidePanel::onSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous) {
    clearForm();
    if (!current) return;

    QString uuid = current->data(Qt::UserRole).toString();
    if (frameMap.contains(uuid)) {
        populateForm(frameMap[uuid]);
    }
}
*/
void CameraSidePanel::onItemDropped() {
    QStringList newOrder;
    for (int i = 0; i < listWidget->count(); ++i) {
        newOrder << listWidget->item(i)->text();
    }
    emit cameraReordered(newOrder);
}

void CameraSidePanel::updateCameraFrame(const GameFusion::CameraFrame& frame){
    if(!nameEdit){
        populateForm(frame);
        return;
    }

    // Block signals temporarily for all UI widgets
    QSignalBlocker blockName(nameEdit);
    QSignalBlocker blockX(xEdit);
    QSignalBlocker blockY(yEdit);
    QSignalBlocker blockZoom(zoomEdit);
    QSignalBlocker blockRotation(rotationEdit);
    QSignalBlocker blockFrameOffset(frameOffset);
    QSignalBlocker blockEasing(easingCombo);

    nameEdit->setText(QString::fromStdString(frame.name));
    xEdit->setValue(frame.x);
    yEdit->setValue(frame.y);
    zoomEdit->setValue(frame.zoom*100.f);
    rotationEdit->setValue(frame.rotation);
    frameOffset->setValue(frame.frameOffset);
    uuidSelected = QString::fromStdString(frame.uuid);
    easingCombo->setCurrentIndex(static_cast<int>(frame.easing));
}
