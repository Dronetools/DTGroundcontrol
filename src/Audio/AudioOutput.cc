/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include <QApplication>
#include <QDebug>
#include <QRegularExpression>

#include "AudioOutput.h"
#include "QGCApplication.h"
#include "QGC.h"
#include "SettingsManager.h"


AudioOutput::AudioOutput(QGCApplication* app, QGCToolbox* toolbox)
    : QGCTool   (app, toolbox)
    , _tts      (nullptr)
{
    if (qgcApp()->runningUnitTests()) {
        // Cloud based unit tests don't have speech capabilty. If you try to crank up
        // speech engine it will pop a qWarning which prevents usage of QT_FATAL_WARNINGS
        return;
    }

    _tts = new QTextToSpeech(this);

    //-- Force TTS engine to English as all incoming messages from the autopilot
    //   are in English and not localized.
#ifdef Q_OS_LINUX
    _tts->setLocale(QLocale("en_US"));
#endif
    connect(_tts, &QTextToSpeech::stateChanged, this, &AudioOutput::_stateChanged);
}

void AudioOutput::say(const QString& inText)
{
    if (!_tts) {
        qDebug() << "say" << inText;
        return;
    }

    bool muted = qgcApp()->toolbox()->settingsManager()->appSettings()->audioMuted()->rawValue().toBool();
    muted |= qgcApp()->runningUnitTests();
    if (!muted && !qgcApp()->runningUnitTests()) {
        QString text = fixTextMessageForAudio(inText);
        qDebug() << "Mensaje MAVLINK recibido: " << text;
        QString traducido = traducir(text);
        if(_tts->state() == QTextToSpeech::Speaking) {
            if(!_texts.contains(traducido)) {
                //-- Some arbitrary limit
                if(_texts.size() > 20) {
                    _texts.removeFirst();
                }
                _texts.append(traducido);
            }
        } else {
            _tts->say(traducido);
        }
    }
}

QString AudioOutput::traducir(QString text){
    static QRegularExpression reFlightMode(R"(([\w\s]+)flight mode)");
    static QRegularExpression reInitFailed(R"(mode change to (\w+) failed: init failed)");
    static QRegularExpression rePreArmFailsafe(R"(mode change to (\w+) failed: init failed)");
    QMap<QString,QString> diccionarioModos;
    diccionarioModos["guided"] = "guiado";
    diccionarioModos["stabilize"] = "estabilizado";
    diccionarioModos["auto"] = "automático";
    diccionarioModos["altitude hold"] = "mantenimiento de altura";
    QString textoTraducido = text.trimmed().toLower();
    QRegularExpressionMatch matchFlightMode = reFlightMode.match(textoTraducido);
    QRegularExpressionMatch matchInitFailed = reInitFailed.match(textoTraducido);
    if(matchFlightMode.hasMatch()) {
        textoTraducido = "Modo de vuelo: ";
        QString modo = matchFlightMode.captured(1).trimmed();
        if(diccionarioModos.contains(modo)) modo = diccionarioModos.value(modo);
        textoTraducido += modo;
    }else if(matchInitFailed.hasMatch()){
        textoTraducido = "Error en cambio a modo $: fallo de inicialización";
        QString modo = matchInitFailed.captured(1).trimmed();
        if(diccionarioModos.contains(modo)) modo = diccionarioModos.value(modo);
        textoTraducido = textoTraducido.replace("$",modo);
    }else if(textoTraducido.contains("no such mode")){
        textoTraducido = textoTraducido.replace("no such mode","No existe el modo");
    }else if(textoTraducido == "pre arm: battery 1 below minimum arming voltage" || textoTraducido == "arm: battery 1 below minimum arming voltage"){
        textoTraducido = "Batería por debajo del mínimo requerido para el armado";
    }else if(textoTraducido == "pre arm: battery 1 low voltage failsafe" || textoTraducido == "pre arm: battery failsafe" || textoTraducido == "arm: battery failsafe"){
        textoTraducido = "Batería por debajo del voltaje de seguridad, feilseif";
    }else if(textoTraducido.contains("potential thrust loss")){ textoTraducido = "Pérdida del potencial de empuje";
    }else if(textoTraducido == "battery  level low"){ textoTraducido = "Nivel de batería bajo";
    }else if(textoTraducido.contains("battery failsafe")){ textoTraducido = "Modo feilseif";
    }else if(textoTraducido == "armed" ){textoTraducido = "armado";
    }else if(textoTraducido == "disarmed" ) textoTraducido = "desarmado";

    qDebug() << "Texto traducido: " << textoTraducido;
    return textoTraducido;
}

void AudioOutput::_stateChanged(QTextToSpeech::State state)
{
    if(state == QTextToSpeech::Ready) {
        if(_texts.size()) {
            QString text = _texts.first();
            _texts.removeFirst();
            _tts->say(text);
        }
    }
}

bool AudioOutput::getMillisecondString(const QString& string, QString& match, int& number) {
    static QRegularExpression re("([0-9]+ms)");
    QRegularExpressionMatchIterator i = re.globalMatch(string);
    while (i.hasNext()) {
        QRegularExpressionMatch qmatch = i.next();
        if (qmatch.hasMatch()) {
            match = qmatch.captured(0);
            number = qmatch.captured(0).replace("ms", "").toInt();
            return true;
        }
    }
    return false;
}

QString AudioOutput::fixTextMessageForAudio(const QString& string) {
    QString match;
    QString newNumber;
    QString result = string;

    //-- Look for codified terms
    if(result.contains("ERR ", Qt::CaseInsensitive)) {
        result.replace("ERR ", "error ", Qt::CaseInsensitive);
    }
    if(result.contains("ERR:", Qt::CaseInsensitive)) {
        result.replace("ERR:", "error.", Qt::CaseInsensitive);
    }
    if(result.contains("POSCTL", Qt::CaseInsensitive)) {
        result.replace("POSCTL", "Position Control", Qt::CaseInsensitive);
    }
    if(result.contains("ALTCTL", Qt::CaseInsensitive)) {
        result.replace("ALTCTL", "Altitude Control", Qt::CaseInsensitive);
    }
    if(result.contains("AUTO_RTL", Qt::CaseInsensitive)) {
        result.replace("AUTO_RTL", "auto Return To Launch", Qt::CaseInsensitive);
    } else if(result.contains("RTL", Qt::CaseInsensitive)) {
        result.replace("RTL", "Return To Launch", Qt::CaseInsensitive);
    }
    if(result.contains("ACCEL ", Qt::CaseInsensitive)) {
        result.replace("ACCEL ", "accelerometer ", Qt::CaseInsensitive);
    }
    if(result.contains("RC_MAP_MODE_SW", Qt::CaseInsensitive)) {
        result.replace("RC_MAP_MODE_SW", "RC mode switch", Qt::CaseInsensitive);
    }
    if(result.contains("REJ.", Qt::CaseInsensitive)) {
        result.replace("REJ.", "Rejected", Qt::CaseInsensitive);
    }
    if(result.contains("WP", Qt::CaseInsensitive)) {
        result.replace("WP", "way point", Qt::CaseInsensitive);
    }
    if(result.contains("CMD", Qt::CaseInsensitive)) {
        result.replace("CMD", "command", Qt::CaseInsensitive);
    }
    if(result.contains("COMPID", Qt::CaseInsensitive)) {
        result.replace("COMPID", "component eye dee", Qt::CaseInsensitive);
    }
    if(result.contains(" params ", Qt::CaseInsensitive)) {
        result.replace(" params ", " parameters ", Qt::CaseInsensitive);
    }
    if(result.contains(" id ", Qt::CaseInsensitive)) {
        result.replace(" id ", " eye dee ", Qt::CaseInsensitive);
    }
    if(result.contains(" ADSB ", Qt::CaseInsensitive)) {
        result.replace(" ADSB ", " Hey Dee Ess Bee ", Qt::CaseInsensitive);
    }
    if(result.contains(" EKF ", Qt::CaseInsensitive)) {
        result.replace(" EKF ", " Eee Kay Eff ", Qt::CaseInsensitive);
    }
    if(result.contains("PREARM", Qt::CaseInsensitive)) {
        result.replace("PREARM", "pre arm", Qt::CaseInsensitive);
    }
    if(result.contains("PITOT", Qt::CaseInsensitive)) {
        result.replace("PITOT", "pee toe", Qt::CaseInsensitive);
    }

    // Convert negative numbers
    QRegularExpression re(QStringLiteral("(-)[0-9]*\\.?[0-9]"));
    QRegularExpressionMatch reMatch = re.match(result);
    while (reMatch.hasMatch()) {
        if (!reMatch.captured(1).isNull()) {
            // There is a negative prefix
            result.replace(reMatch.capturedStart(1), reMatch.capturedEnd(1) - reMatch.capturedStart(1), tr(" negative "));
        }
        reMatch = re.match(result);
    }

    // Convert real number with decimal point
    re.setPattern(QStringLiteral("([0-9]+)(\\.)([0-9]+)"));
    reMatch = re.match(result);
    while (reMatch.hasMatch()) {
        if (!reMatch.captured(2).isNull()) {
            // There is a decimal point
            result.replace(reMatch.capturedStart(2), reMatch.capturedEnd(2) - reMatch.capturedStart(2), tr(" point "));
        }
        reMatch = re.match(result);
    }

    // Convert meter postfix after real number
    re.setPattern(QStringLiteral("[0-9]*\\.?[0-9]\\s?(m)([^A-Za-z]|$)"));
    reMatch = re.match(result);
    while (reMatch.hasMatch()) {
        if (!reMatch.captured(1).isNull()) {
            // There is a meter postfix
            result.replace(reMatch.capturedStart(1), reMatch.capturedEnd(1) - reMatch.capturedStart(1), tr(" meters"));
        }
        reMatch = re.match(result);
    }

    int number;
    if(getMillisecondString(string, match, number) && number > 1000) {
        if(number < 60000) {
            int seconds = number / 1000;
            newNumber = QString("%1 second%2").arg(seconds).arg(seconds > 1 ? "s" : "");
        } else {
            int minutes = number / 60000;
            int seconds = (number - (minutes * 60000)) / 1000;
            if (!seconds) {
                newNumber = QString("%1 minute%2").arg(minutes).arg(minutes > 1 ? "s" : "");
            } else {
                newNumber = QString("%1 minute%2 and %3 second%4").arg(minutes).arg(minutes > 1 ? "s" : "").arg(seconds).arg(seconds > 1 ? "s" : "");
            }
        }
        result.replace(match, newNumber);
    }
    return result;
}
