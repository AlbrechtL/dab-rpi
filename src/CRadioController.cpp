/*
 *    Copyright (C) 2017
 *    Albrecht Lohofener (albrechtloh@gmx.de)
 *
 *    This file is based on SDR-J
 *    Copyright (C) 2010, 2011, 2012
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *
 *    This file is part of the welle.io.
 *    Many of the ideas as implemented in welle.io are derived from
 *    other work, made available through the GNU general Public License.
 *    All copyrights of the original authors are recognized.
 *
 *    welle.io is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    welle.io is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with welle.io; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <QDebug>
#include <QSettings>

#include "CRadioController.h"
//#include "CInputFactory.h"
//#include "CRAWFile.h"
//#include "CRTL_TCP_Client.h"

#define AUDIOBUFFERSIZE 32768

CRadioController::CRadioController(QVariantMap& commandLineOptions, QObject *parent)
#ifdef Q_OS_ANDROID
    : CRadioControllerSource(parent)
#else
    : QObject(parent)
#endif
    , commandLineOptions(commandLineOptions)
{
    Device = NULL;

//    AudioBuffer = new RingBuffer<int16_t>(2 * AUDIOBUFFERSIZE);
//    Audio = new CAudio(AudioBuffer);

    MOTImage = new QImage();

//    spectrum_fft_handler = new common_fft(DABParams.T_u);

    // Init the technical data
    ResetTechnicalData();

    // Read channels from settings
    mStationList.loadStations();
    mStationList.sort();
    emit StationsChanged(mStationList.getList());

    // Init timers
    connect(&StationTimer, &QTimer::timeout, this, &CRadioController::StationTimerTimeout);
    connect(&ChannelTimer, &QTimer::timeout, this, &CRadioController::ChannelTimerTimeout);
    connect(&SyncCheckTimer, &QTimer::timeout, this, &CRadioController::SyncCheckTimerTimeout);

    // Init SDRDAB interface
    connect(&SDRDABInterface, &CSDRDABInterface::NewStationFound, this, &CRadioController::NewStation);
}

CRadioController::~CRadioController(void)
{
//    delete Audio;
}

void CRadioController::ResetTechnicalData(void)
{
    Status = Unknown;
    CurrentChannel = tr("Unknown");
    CurrentEnsemble = "";
    CurrentFrequency = 0;
    CurrentStation = "";
    CurrentStationType = "";
    CurrentLanguageType = "";
    CurrentTitle = tr("No Station");
    CurrentText = "";

    mIsSync = false;
    mIsFICCRC = false;
    mIsSignal = false;
    mSNR = 0;
    mFrequencyCorrection = 0;
    mBitRate = 0;
    mAudioSampleRate = 0;
    mIsStereo = true;
    mIsDAB = true;
    mFrameErrors = 0;
    mRSErrors = 0;
    mAACErrors = 0;
    mGainCount = 0;
    mStationCount = 0;
    CurrentManualGain = 0;
    CurrentManualGainValue = 0.0;
    CurrentVolume = 1.0;

    startPlayback = false;
    isChannelScan = false;
    isAGC = true;
    isHwAGC = true;

    UpdateGUIData();

    // Clear MOT
    MOTImage->loadFromData(0, 0, Q_NULLPTR);
    emit MOTChanged(*MOTImage);
}

void CRadioController::closeDevice()
{
    qDebug() << "RadioController:" << "Close device";

    delete Device;
    Device = NULL;

//    if (Audio)
//        Audio->reset();

    SyncCheckTimer.stop();

    // Reset the technical data
    ResetTechnicalData();

    emit isHwAGCSupportedChanged(isHwAGCSupported());
    emit DeviceClosed();
}

void CRadioController::openDevice(CVirtualInput* Dev)
{
    if (Device) {
        closeDevice();
    }
    this->Device = Dev;
    Initialise();
}

void CRadioController::onEventLoopStarted()
{
#ifdef Q_OS_ANDROID
    QString dabDevice = "rtl_tcp";
#else
    QString dabDevice = "auto";
#endif

    QString ipAddress = "127.0.0.1";
    uint16_t ipPort = 1234;
    QString rawFile = "";
    QString rawFileFormat = "u8";

    if(commandLineOptions["dabDevice"] != "")
        dabDevice = commandLineOptions["dabDevice"].toString();

    if(commandLineOptions["ipAddress"] != "")
        ipAddress = commandLineOptions["ipAddress"].toString();

    if(commandLineOptions["ipPort"] != "")
        ipPort = commandLineOptions["ipPort"].toInt();

    if(commandLineOptions["rawFile"] != "")
        rawFile = commandLineOptions["rawFile"].toString();

    if(commandLineOptions["rawFileFormat"] != "")
        rawFileFormat = commandLineOptions["rawFileFormat"].toString();

    if(dabDevice == "rawfile")
        SDRDABInterface.SetRAWInput(rawFile);

    // Init device
//    Device = CInputFactory::GetDevice(*this, dabDevice);

//    // Set rtl_tcp settings
//    if (Device->getID() == CDeviceID::RTL_TCP) {
//        CRTL_TCP_Client* RTL_TCP_Client = (CRTL_TCP_Client*)Device;

//        RTL_TCP_Client->setIP(ipAddress);
//        RTL_TCP_Client->setPort(ipPort);
//    }

//    // Set rawfile settings
//    if (Device->getID() == CDeviceID::RAWFILE) {
//        CRAWFile* RAWFile = (CRAWFile*)Device;

//        RAWFile->setFileName(rawFile, rawFileFormat);
//    }

    Initialise();
}

void CRadioController::Initialise(void)
{
//    mGainCount = Device->getGainCount();
//    emit GainCountChanged(mGainCount);

//    Device->setHwAgc(isHwAGC);

//    if(!isAGC) // Manual AGC
//    {
//        Device->setAgc(false);
//        Device->setGain(CurrentManualGain);
//        qDebug() << "RadioController:" << "AGC off";
//    }
//    else
//    {
//        qDebug() << "RadioController:" << "AGC on";
//    }

//    if(Audio)
//        Audio->setVolume(CurrentVolume);

    Status = Initialised;
    emit DeviceReady();
    emit isHwAGCSupportedChanged(isHwAGCSupported());
    UpdateGUIData();
}

void CRadioController::Play(QString Channel, QString Station)
{
    qDebug() << "RadioController:" << "Play channel:"
             << Channel << "station:" << Station;

    if (Status == Scanning) {
        StopScan();
    }

    /*DeviceRestart();
    startPlayback = false;

    SetChannel(Channel, false);
    SetStation(Station);*/

    if(Status != Playing)
        SDRDABInterface.Start();

    SDRDABInterface.TuneToStation(Station);

    Status = Playing;
    UpdateGUIData();

    // Store as last station
    QSettings Settings;
    QStringList StationElement;
    StationElement. append (Station);
    StationElement. append (Channel);
    Settings.setValue("lastchannel", StationElement);

    // Check every 15 s for a correct sync
    SyncCheckTimer.start(15000);
}

void CRadioController::Pause()
{
//    if (Device)
//        Device->stop();

//    if (Audio)
//        Audio->reset();

    SyncCheckTimer.stop();

    startPlayback = false;
    Status = Paused;
    UpdateGUIData();
}

void CRadioController::Stop()
{
//    if (Device)
//        Device->stop();

//    if (Audio)
//        Audio->reset();

    SyncCheckTimer.stop();

    startPlayback = false;
    Status = Stopped;
    UpdateGUIData();
}

void CRadioController::ClearStations()
{
    //	Clear old channels
    emit StationsCleared();
    mStationList.reset();
    emit StationsChanged(mStationList.getList());

    // Save the channels
    mStationList.saveStations();

    // Clear last station
    QSettings Settings;
    Settings.remove("lastchannel");
}

qreal CRadioController::Volume() const
{
    return CurrentVolume;
}

void CRadioController::setVolume(qreal Volume)
{
    CurrentVolume = Volume;

//    if (Audio)
//        Audio->setVolume(Volume);

    emit VolumeChanged(CurrentVolume);
}

void CRadioController::SetChannel(QString Channel, bool isScan, bool Force)
{
//    if(CurrentChannel != Channel || Force == true)
//    {
//        if(Device && Device->getID() == CDeviceID::RAWFILE)
//        {
//            CurrentChannel = "File";
//            CurrentEnsemble = "";
//            CurrentFrequency = 0;
//        }
//        else // A real device
//        {
//            CurrentChannel = Channel;
//            CurrentEnsemble = "";

//            // Convert channel into a frequency
//            CurrentFrequency = Channels.getFrequency(Channel);

//            if(CurrentFrequency != 0 && Device)
//            {
//                qDebug() << "RadioController: Tune to channel" <<  Channel << "->" << CurrentFrequency/1e6 << "MHz";
//                Device->setFrequency(CurrentFrequency);
//            }
//        }

//        DecoderRestart(isScan);

//        StationList.clear();

//        UpdateGUIData();
//    }
}

void CRadioController::SetManualChannel(QString Channel)
{
    // Play channel's first station, if available
    foreach(StationElement* station, mStationList.getList())
    {
        if (station->getChannelName() == Channel)
        {
            QString stationName = station->getStationName();
            qDebug() << "RadioController: Play channel" <<  Channel << "and first station" << stationName;
            Play(Channel, stationName);
            return;
        }
    }

    // Otherwise tune to channel and play first found station
    qDebug() << "RadioController: Tune to channel" <<  Channel;

    SyncCheckTimer.stop();
    DeviceRestart();

    startPlayback = true;
    Status = Playing;
    CurrentTitle = tr("Tuning") + " ... " + Channel;

    // Clear old data
    CurrentStation = "";
    CurrentStationType = "";
    CurrentLanguageType = "";
    CurrentText = "";

    UpdateGUIData();

    // Clear MOT
    MOTImage->loadFromData(0, 0, Q_NULLPTR);
    emit MOTChanged(*MOTImage);

    // Switch channel
    SetChannel(Channel, false, true);
}

void CRadioController::StartScan(void)
{
    qDebug() << "RadioController:" << "Start channel scan";

    SyncCheckTimer.stop();
    startPlayback = false;

    // ToDo: Just for testing
    SDRDABInterface.Start();

//    if(Device && Device->getID() == CDeviceID::RAWFILE)
//    {
//        CurrentTitle = tr("RAW File");
//        SetChannel(CChannels::FirstChannel, false); // Just a dummy
//        emit ScanStopped();
//    }
//    else
//    {
//        // Start with lowest frequency
//        QString Channel = CChannels::FirstChannel;
//        SetChannel(Channel, true);

//        isChannelScan = true;
//        mStationCount = 0;
//        CurrentTitle = tr("Scanning") + " ... " + Channel
//                + " (" + QString::number((int)(1 * 100 / NUMBEROFCHANNELS)) + "%)";
//        CurrentText = tr("Found channels") + ": " + QString::number(mStationCount);

//        Status = Scanning;

//        // Clear old data
//        CurrentStation = "";
//        CurrentStationType = "";
//        CurrentLanguageType = "";

//        UpdateGUIData();
//        emit ScanProgress(0);
//    }

    ClearStations();
}

void CRadioController::StopScan(void)
{
    qDebug() << "RadioController:" << "Stop channel scan";

    isChannelScan = false;
    CurrentTitle = tr("No Station");
    CurrentText = "";

    Status = Stopped;
    UpdateGUIData();
    emit ScanStopped();
}

QList<StationElement *> CRadioController::Stations() const
{
    return mStationList.getList();
}

QVariantMap CRadioController::GUIData(void) const
{
    return mGUIData;
}

void CRadioController::UpdateGUIData()
{
//    mGUIData["DeviceName"] = (Device) ? Device->getName() : "";

    // Init the GUI data map
    mGUIData["Status"] = Status;
    mGUIData["Channel"] = CurrentChannel;
    mGUIData["Ensemble"] = CurrentEnsemble.trimmed();
    mGUIData["Frequency"] = CurrentFrequency;
    mGUIData["Station"] = CurrentStation;
    mGUIData["StationType"] = CurrentStationType;
    mGUIData["Title"] = CurrentTitle.simplified();
    mGUIData["Text"] = CurrentText;
    mGUIData["LanguageType"] = CurrentLanguageType;

    //qDebug() << "RadioController:" <<  "UpdateGUIData";
    emit GUIDataChanged(mGUIData);
}

QImage CRadioController::MOT() const
{
    return *MOTImage;
}

QString CRadioController::DateTime() const
{
    QDateTime LocalTime = mCurrentDateTime.toLocalTime();
    return QLocale().toString(LocalTime, QLocale::ShortFormat);
}

bool CRadioController::isSync() const
{
    return mIsSync;
}

bool CRadioController::isFICCRC() const
{
    return mIsFICCRC;
}

bool CRadioController::isSignal() const
{
    return mIsSignal;
}

bool CRadioController::isStereo() const
{
    return mIsStereo;
}

bool CRadioController::isDAB() const
{
    return mIsDAB;
}

int CRadioController::SNR() const
{
    return mSNR;
}

int CRadioController::FrequencyCorrection() const
{
    return mFrequencyCorrection;
}

int CRadioController::BitRate() const
{
    return mBitRate;
}

int CRadioController::AudioSampleRate() const
{
    return mAudioSampleRate;
}

int CRadioController::FrameErrors() const
{
    return mFrameErrors;
}

int CRadioController::RSErrors() const
{
    return mRSErrors;
}

int CRadioController::AACErrors() const
{
    return mAACErrors;
}

int CRadioController::GainCount() const
{
    return mGainCount;
}

bool CRadioController::isHwAGCSupported() const
{
//    return (this->Device) ? this->Device->isHwAgcSupported() : false;
}

bool CRadioController::HwAGC() const
{
    return this->isHwAGC;
}

void CRadioController::setHwAGC(bool isHwAGC)
{
    this->isHwAGC = isHwAGC;

//    if (Device)
//    {
//        Device->setHwAgc(isHwAGC);
//        qDebug() << "RadioController:" << (isHwAGC ? "HwAGC on" : "HwAGC off");
//    }
    emit HwAGCChanged(isHwAGC);
}

bool CRadioController::AGC() const
{
    return this->isAGC;
}

void CRadioController::setAGC(bool isAGC)
{
    this->isAGC = isAGC;

//    if (Device)
//    {
//        Device->setAgc(isAGC);

//        if (!isAGC)
//        {
//            Device->setGain(CurrentManualGain);
//            qDebug() << "RadioController:" << "AGC off";
//        }
//        else
//        {
//            qDebug() << "RadioController:" <<  "AGC on";
//        }
//    }
    emit AGCChanged(isAGC);
}

float CRadioController::GainValue() const
{
    return CurrentManualGainValue;
}

int CRadioController::Gain() const
{
    return CurrentManualGain;
}

void CRadioController::setGain(int Gain)
{
    CurrentManualGain = Gain;
    emit GainChanged(CurrentManualGain);

//    if (Device)
//        CurrentManualGainValue = Device->setGain(Gain);
//    else
//        CurrentManualGainValue = -1.0;

    emit GainValueChanged(CurrentManualGainValue);
}

void CRadioController::setErrorMessage(QString Text)
{
    Status = Error;
    emit showErrorMessage(Text);
}

void CRadioController::setInfoMessage(QString Text)
{
    emit showInfoMessage(Text);
}

void CRadioController::setAndroidInstallDialog(QString Title, QString Text)
{
    emit showAndroidInstallDialog(Title, Text);
}

/********************
 * Private methods  *
 ********************/

void CRadioController::DeviceRestart()
{
    bool isPlay = false;

//    if(Device)
//        isPlay = Device->restart();

    if(!isPlay)
    {
        qDebug() << "RadioController:" << "Radio device is not ready or does not exits.";
        emit showErrorMessage(tr("Radio device is not ready or does not exits."));
        return;
    }
}

void CRadioController::DecoderRestart(bool isScan)
{

}

void CRadioController::SetStation(QString Station, bool Force)
{
    if(CurrentStation != Station || Force == true)
    {
        CurrentStation = Station;

        qDebug() << "RadioController: Tune to station" <<  Station;

        CurrentTitle = tr("Tuning") + " ... " + Station;

        // Wait if we found the station inside the signal
        StationTimer.start(1000);

        // Clear old data
        CurrentStationType = "";
        CurrentLanguageType = "";
        CurrentText = "";

        UpdateGUIData();

        // Clear MOT
        MOTImage->loadFromData(0, 0, Q_NULLPTR);
        emit MOTChanged(*MOTImage);
    }
}

void CRadioController::NextChannel(bool isWait)
{
    if(isWait) // It might be a channel, wait 10 seconds
    {
        ChannelTimer.start(10000);
    }
    else
    {
        QString Channel = Channels.getNextChannel();

        if(!Channel.isEmpty()) {
            SetChannel(Channel, true);

            int index = Channels.getCurrentIndex() + 1;

            CurrentTitle = tr("Scanning") + " ... " + Channel
                    + " (" + QString::number((int)(index * 100 / NUMBEROFCHANNELS)) + "%)";

            UpdateGUIData();
            emit ScanProgress(index);
        } else {
            StopScan();
        }
    }
}

/********************
 * Controller slots *
 ********************/

void CRadioController::StationTimerTimeout()
{
//    if(StationList.contains(CurrentStation))
//    {
//        audiodata AudioData;
//        memset(&AudioData, 0, sizeof(audiodata));

//        my_ficHandler->dataforAudioService(CurrentStation, &AudioData);

//        if(AudioData.defined == true)
//        {
//            // We found the station inside the signal, lets stop the timer
//            StationTimer.stop();

//            // Set station
//            my_mscHandler->set_audioChannel(&AudioData);

//            CurrentTitle = CurrentStation;

//            CurrentStationType = CDABConstants::getProgramTypeName(AudioData.programType);
//            CurrentLanguageType = CDABConstants::getLanguageName(AudioData.language);
//            mBitRate = AudioData.bitRate;
//            emit BitRateChanged(mBitRate);

//            if (AudioData.ASCTy == 077)
//                mIsDAB = false;
//            else
//                mIsDAB = true;
//            emit isDABChanged(mIsDAB);

//            Status = Playing;
//            UpdateGUIData();
//        }
//    }
}

void CRadioController::ChannelTimerTimeout(void)
{
    ChannelTimer.stop();

    if(isChannelScan)
        NextChannel(false);
}

void CRadioController::SyncCheckTimerTimeout(void)
{
    // A better approach is to use the MER since it is not implemented we use the this one
    if(!mIsSync ||
       (mIsSync && !mIsFICCRC) ||
       (mIsSync && mFrameErrors >= 10))
    {
        qDebug() << "RadioController: Restart syncing. isSync:" << mIsSync << ", isFICCRC:" << mIsFICCRC << ", FrameErrors:" << mFrameErrors;
        emit showInfoMessage(tr("Lost signal or bad signal quality, trying to find it again."));

        SetChannel(CurrentChannel, false, true);
        SetStation(CurrentStation, true);
    }
}

void CRadioController::NewStation(QString StationName)
{
    // ToDo Just for testing
    CurrentChannel = "File";

    //	Add new station into list
    if (!mStationList.contains(StationName, CurrentChannel))
    {
        mStationList.append(StationName, CurrentChannel);

        //	Sort stations
        mStationList.sort();

        emit StationsChanged(mStationList.getList());
        emit FoundStation(StationName, CurrentChannel);

        // Save the channels
        mStationList.saveStations();
    }
}

/*****************
 * Backend slots *
 *****************/

void CRadioController::addtoEnsemble(quint32 SId, const QString &Station)
{
    /*qDebug() << "RadioController: Found station" <<  Station
             << "(" << qPrintable(QString::number(SId, 16).toUpper()) << ")";

    if (startPlayback && StationList.isEmpty()) {
        qDebug() << "RadioController: Start playback of first station" << Station;
        startPlayback = false;
        Play(CurrentChannel, Station);
    }

    StationList.append(Station);

    if (Status == Scanning) {
        mStationCount++;
        CurrentText = tr("Found channels") + ": " + QString::number(mStationCount);
        UpdateGUIData();
    }*/


}

void CRadioController::nameofEnsemble(int id, const QString &Ensemble)
{
    qDebug() << "RadioController: Name of ensemble:" << Ensemble;
    (void)id;

    if (CurrentEnsemble == Ensemble)
        return;
    CurrentEnsemble = Ensemble;
    UpdateGUIData();
}

void CRadioController::changeinConfiguration()
{
    // Unknown use case
}

void CRadioController::displayDateTime(int *DateTime)
{
    QDate Date;
    QTime Time;

    int Year = DateTime[0];
    int Month = DateTime[1];
    int Day = DateTime[2];
    int Hour = DateTime[3];
    int Minute = DateTime[4];
    int Seconds	= DateTime [5];
    int HourOffset = DateTime[6];
    int MinuteOffset = DateTime[7];

    Time.setHMS(Hour, Minute, Seconds);
    mCurrentDateTime.setTime(Time);

    Date.setDate(Year, Month, Day);
    mCurrentDateTime.setDate(Date);

    int OffsetFromUtc = ((HourOffset * 3600) + (MinuteOffset * 60));
    mCurrentDateTime.setOffsetFromUtc(OffsetFromUtc);
    mCurrentDateTime.setTimeSpec(Qt::OffsetFromUTC);

    QDateTime LocalTime = mCurrentDateTime.toLocalTime();
    emit DateTimeChanged(QLocale().toString(LocalTime, QLocale::ShortFormat));

    return;
}

void CRadioController::show_ficSuccess(bool isFICCRC)
{
    if (mIsFICCRC == isFICCRC)
        return;
    mIsFICCRC = isFICCRC;
    emit isFICCRCChanged(mIsFICCRC);
}

void CRadioController::show_snr(int SNR)
{
    if (mSNR == SNR)
        return;
    mSNR = SNR;
    emit SNRChanged(mSNR);
}

void CRadioController::set_fineCorrectorDisplay(int FineFrequencyCorr)
{
    int CoarseFrequencyCorr = (mFrequencyCorrection / 1000);
    SetFrequencyCorrection((CoarseFrequencyCorr * 1000) + FineFrequencyCorr);
}

void CRadioController::set_coarseCorrectorDisplay(int CoarseFreuqencyCorr)
{
    int OldCoareFrequencyCorrr = (mFrequencyCorrection / 1000);
    int FineFrequencyCorr = mFrequencyCorrection - (OldCoareFrequencyCorrr * 1000);
    SetFrequencyCorrection((CoarseFreuqencyCorr * 1000) + FineFrequencyCorr);
}





void CRadioController::setSynced(char isSync)
{
//    bool sync = (isSync == SYNCED) ? true : false;
//    if (mIsSync == sync)
//        return;
//    mIsSync = sync;
//    emit isSyncChanged(mIsSync);
}

void CRadioController::setSignalPresent(bool isSignal)
{
    if (mIsSignal != isSignal) {
        mIsSignal = isSignal;
        emit isSignalChanged(mIsSignal);
    }

    if(isChannelScan)
        NextChannel(isSignal);
}

void CRadioController::SetFrequencyCorrection(int FrequencyCorrection)
{
    if (mFrequencyCorrection == FrequencyCorrection)
        return;
    mFrequencyCorrection = FrequencyCorrection;
    emit FrequencyCorrectionChanged(mFrequencyCorrection);
}

void CRadioController::newAudio(int SampleRate)
{
//    if(mAudioSampleRate != SampleRate)
//    {
//        qDebug() << "RadioController: Audio sample rate" <<  SampleRate << "kHz";
//        mAudioSampleRate = SampleRate;
//        emit AudioSampleRateChanged(mAudioSampleRate);

//        Audio->setRate(SampleRate);
//    }
}

void CRadioController::setStereo(bool isStereo)
{
    if (mIsStereo == isStereo)
        return;
    mIsStereo = isStereo;
    emit isStereoChanged(mIsStereo);
}

void CRadioController::show_frameErrors(int FrameErrors)
{
    if (mFrameErrors == FrameErrors)
        return;
    mFrameErrors = FrameErrors;
    emit FrameErrorsChanged(mFrameErrors);
}

void CRadioController::show_rsErrors(int RSErrors)
{
    if (mRSErrors == RSErrors)
        return;
    mRSErrors = RSErrors;
    emit RSErrorsChanged(mRSErrors);
}

void CRadioController::show_aacErrors(int AACErrors)
{
    if (mAACErrors == AACErrors)
        return;
    mAACErrors = AACErrors;
    emit AACErrorsChanged(mAACErrors);
}

void CRadioController::showLabel(QString Label)
{
    if (this->CurrentText == Label)
        return;
    this->CurrentText = Label;
    UpdateGUIData();
}

void CRadioController::showMOT(QByteArray Data, int Subtype, QString s)
{
    (void)s; // Not used, can be removed

    MOTImage->loadFromData(Data, Subtype == 0 ? "GIF" : Subtype == 1 ? "JPEG" : Subtype == 2 ? "BMP" : "PNG");

    emit MOTChanged(*MOTImage);
}

void CRadioController::UpdateSpectrum()
{
//    int Samples = 0;
//    int16_t T_u = DABParams.T_u;

//    //	Delete old data
//    spectrum_data.resize(T_u);

//    qreal tunedFrequency_MHz = 0;
//    qreal sampleFrequency_MHz = 2048000 / 1e6;
//    qreal dip_MHz = sampleFrequency_MHz / T_u;

//    qreal x(0);
//    qreal y(0);
//    qreal y_max(0);

//    // Get FFT buffer
//    DSPCOMPLEX* spectrumBuffer = spectrum_fft_handler->getVector();

//    // Get samples
//    tunedFrequency_MHz = CurrentFrequency / 1e6;
//    if(Device)
//        Samples = Device->getSpectrumSamples(spectrumBuffer, T_u);

//    // Continue only if we got data
//    if (Samples <= 0)
//        return;

//    // Do FFT to get the spectrum
//    spectrum_fft_handler->do_FFT();

//    //	Process samples one by one
//    for (int i = 0; i < T_u; i++) {
//        int half_Tu = T_u / 2;

//        //	Shift FFT samples
//        if (i < half_Tu)
//            y = abs(spectrumBuffer[i + half_Tu]);
//        else
//            y = abs(spectrumBuffer[i - half_Tu]);

//        // Apply a cumulative moving average filter
//        int avg = 4; // Number of y values to average
//        qreal CMA = spectrum_data[i].y();
//        y = (CMA * avg + y) / (avg + 1);

//        //	Find maximum value to scale the plotter
//        if (y > y_max)
//            y_max = y;

//        // Calc x frequency
//        x = (i * dip_MHz) + (tunedFrequency_MHz - (sampleFrequency_MHz / 2));

//        spectrum_data[i]= QPointF(x, y);
//    }

//    //	Set new data
//    emit SpectrumUpdated(round(y_max) + 1,
//                         tunedFrequency_MHz - (sampleFrequency_MHz / 2),
//                         tunedFrequency_MHz + (sampleFrequency_MHz / 2),
//                         spectrum_data);
}
