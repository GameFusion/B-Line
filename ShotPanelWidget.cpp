
#include "ShotPanelWidget.h"


ShotPanelWidget::ShotPanelWidget(QWidget *parent)
		: QWidget(parent), ui(new Ui::ShotPanelWidget)
{
	ui->setupUi(this);
}
	
ShotPanelWidget::~ShotPanelWidget()
{
		;
}

// Converts milliseconds to timecode string HH:MM:SS:FF based on fps
static QString msToTimeCode(int ms, float fps) {
    if (fps <= 0.0f) fps = 24.0f; // fallback fps

    int totalSeconds = ms / 1000;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    int frames = static_cast<int>(((ms % 1000) * fps) / 1000);

    // Use QString::asprintf or QString::arg with zero padding
    return QString("%1:%2:%3:%4")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(frames, 2, 10, QChar('0'));
}


void ShotPanelWidget::setPanelInfo(GameFusion::Scene *scene, GameFusion::Shot *shot, GameFusion::Panel *panel, float fps)
{



    // Shot level info
    ui->lineEdit_ShotName->setText(shot->name.c_str());
    //ui->lineEdit_ShotNumber->setText(shot->number);
    //ui->lineEdit_ShotSuffix->setText(shot->suffix);
    ui->lineEdit_ShotDurationTC->setText(msToTimeCode(shot->endTime - shot->startTime, fps));
    ui->lineEdit_ShotStartTC->setText(msToTimeCode(shot->startTime, fps));
    ui->lineEdit_ShotEndTC->setText(msToTimeCode(shot->endTime, fps));

    // Panel list population
    ui->comboBox_panelList->clear();
    QStringList allPanelNames;

    for(auto p: shot->panels){
        allPanelNames.append(p.name.c_str());
    }

    ui->comboBox_panelList->addItems(allPanelNames);
    int panelIndex = allPanelNames.indexOf(panel->name.c_str());
    if (panelIndex >= 0)
        ui->comboBox_panelList->setCurrentIndex(panelIndex);
    else
        ui->comboBox_panelList->setEditText(panel->name.c_str()); // fallback

    // Panel duration
    ui->lineEdit_PanelDurationTC->setText(msToTimeCode(panel->durationTime, fps));
    float mspf = fps > 0 ? 1000./fps : 1;
    ui->label_PanelDurationFrames->setText(QString::number(panel->durationTime / mspf));
    ui->label_FPS->setText(QString("FPS: %1").arg(fps));

    // Character combo box
    ui->comboBox_Character->clear();
    QStringList shotCharacters;
    QString allCharacterDialog;

    for(auto character: shot->characters){
        shotCharacters.append(character.name.c_str());

        if(character.dialogNumber > -1)
            allCharacterDialog += QString::number(character.dialogNumber) + ". ";
        if(character.onScreen)
            allCharacterDialog += character.name + "\n";
        else
            allCharacterDialog += character.name + "(OS)\n";
        if(!character.dialogParenthetical.empty()){
            allCharacterDialog += "("+character.dialogParenthetical + ")\n";
        }
        if(!character.dialogue.empty()){
            allCharacterDialog += character.dialogue + "\n";
        }
        if(!character.emotion.empty()){
            allCharacterDialog += "AI EMOTION: "+character.emotion + "\n";
        }
        if(!character.intent.empty()){
            allCharacterDialog += "AI INTENT: "+character.intent + "\n";
        }

        allCharacterDialog += "\n";
    }
    ui->comboBox_Character->addItems(shotCharacters);

    //int characterIndex = sceneCharacters.indexOf(shot->character);
    //if (characterIndex >= 0)
        ui->comboBox_Character->setCurrentIndex(0);
    //else
    //    ui->comboBox_Character->setEditText(panel.character); // if not in list

    // Panel content
    ui->plainTextEdit_CharacterDialog->setPlainText(allCharacterDialog);
    ui->plainTextEdit_Action->setPlainText(shot->action.c_str());
    ui->plainTextEdit_Notes->setPlainText(shot->notes.c_str());
    ui->plainTextEdit_EFX->setPlainText((shot->fx + "\n" + shot->lighting).c_str());
    ui->plainTextEdit_Anim_and_AI_notes->setPlainText(shot->intent.c_str());
    /**/
}
