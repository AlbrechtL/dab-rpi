/*
 *    Copyright (C) 2018
 *    Matthias P. Braendli (matthias.braendli@mpb.li)
 *
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

#include <string>
#include <iostream>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "CRAWFile.h"

static inline int64_t getMyTime(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return ((int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec);
}

#define INPUT_FRAMEBUFFERSIZE 8 * 32768

CRAWFile::CRAWFile(RadioControllerInterface& radioController) :
    radioController(radioController),
    FileName(""),
    FileFormat(CRAWFileFormat::Unknown),
    IQByteSize(1),
    SampleBuffer(INPUT_FRAMEBUFFERSIZE),
    SpectrumSampleBuffer(8192)
{
}

CRAWFile::~CRAWFile(void)
{
    ExitCondition = true;
    if (readerOK) {
        if (thread.joinable()) {
            thread.join();
        }
        fclose(filePointer);
    }
}

void CRAWFile::setFrequency(int32_t Frequency)
{
    (void)Frequency;
}

bool CRAWFile::restart(void)
{
    if (readerOK)
        readerPausing = false;
    return readerOK;
}

void CRAWFile::stop(void)
{
    if (readerOK)
        readerPausing = true;
}

void CRAWFile::reset()
{
}

float CRAWFile::setGain(int32_t Gain)
{
    (void)Gain;

    return 0;
}

int32_t CRAWFile::getGainCount()
{
    return 0;
}

void CRAWFile::setAgc(bool AGC)
{
    (void)AGC;
}

void CRAWFile::setHwAgc(bool hwAGC)
{
    (void)hwAGC;
}

std::string CRAWFile::getName()
{
    return "rawfile (" + FileName + ")";
}

CDeviceID CRAWFile::getID()
{
    return CDeviceID::RAWFILE;
}

void CRAWFile::setFileName(const std::string& FileName, const std::string& FileFormat)
{
    this->FileName = FileName;

    if(FileFormat == "u8")
    {
        this->FileFormat = CRAWFileFormat::U8;
        IQByteSize = 2;
    }
    else if(FileFormat == "s8")
    {
        this->FileFormat = CRAWFileFormat::S8;
        IQByteSize = 2;
    }
    else if(FileFormat == "s16le")
    {
        this->FileFormat = CRAWFileFormat::S16LE;
        IQByteSize = 4;
    }
    else if(FileFormat == "s16be")
    {
        this->FileFormat = CRAWFileFormat::S16BE;
        IQByteSize = 4;
    }
    else
    {
        this->FileFormat = CRAWFileFormat::Unknown;
        std::clog << "RAWFile: unknown file format" << std::endl;
        radioController.onMessage(message_level_t::Error,
                "Unknown RAW file format");
    }

    filePointer = fopen(FileName.c_str(), "rb");
    if (filePointer == nullptr) {
        std::clog << "RAWFile: Cannot open file: " << FileName << std::endl;
        radioController.onMessage(message_level_t::Error,
                "Cannot open file" + FileName);
        return;
    }

    readerOK = true;
    readerPausing = true;
    currPos = 0;
    thread = std::thread(&CRAWFile::run, this);
}

//	size is in I/Q pairs, file contains 8 bits values
int32_t CRAWFile::getSamples(DSPCOMPLEX* V, int32_t size)
{
    if (filePointer == nullptr)
        return 0;

    while ((int32_t)(SampleBuffer.GetRingBufferReadAvailable()) < IQByteSize * size)
        if (readerPausing)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return convertSamples(SampleBuffer, V, size);
}

int32_t CRAWFile::getSpectrumSamples(DSPCOMPLEX* V, int32_t size)
{
    return convertSamples(SpectrumSampleBuffer, V, size);
}

int32_t CRAWFile::getSamplesToRead(void)
{
    return SampleBuffer.GetRingBufferReadAvailable() / 2;
}

void CRAWFile::run(void)
{
    int32_t t, i;
    int32_t bufferSize = 32768;
    int32_t period;
    int64_t nextStop;

    if (!readerOK)
        return;

    ExitCondition = false;

    period = (32768 * 1000) / (IQByteSize * 2048); // full IQś read

    std::clog << "RAWFile" << "Period =" << period << std::endl;
    std::vector<uint8_t> bi(bufferSize);
    nextStop = getMyTime();
    while (!ExitCondition) {
        if (readerPausing) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            nextStop = getMyTime();
            continue;
        }

        while (SampleBuffer.WriteSpace() < bufferSize + 10) {
            if (ExitCondition)
                break;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        nextStop += period;
        t = readBuffer(bi.data(), bufferSize);
        if (t <= 0) {
            for (i = 0; i < bufferSize; i++)
                bi[i] = 0;
            t = bufferSize;
        }
        SampleBuffer.putDataIntoBuffer(bi.data(), t);
        SpectrumSampleBuffer.putDataIntoBuffer(bi.data(), t);
        if (nextStop - getMyTime() > 0)
            std::this_thread::sleep_for(std::chrono::microseconds(
                        nextStop - getMyTime()));
    }

    std::clog << "RAWFile:" <<  "Read threads ends" << std::endl;
}

/*
 *	length is number of uints that we read.
 */
int32_t CRAWFile::readBuffer(uint8_t* data, int32_t length)
{
    int32_t n;

    n = fread(data, sizeof(uint8_t), length, filePointer);
    currPos += n;
    if (n < length) {
        fseek(filePointer, 0, SEEK_SET);
        std::clog << "RAWFile:"  << "End of file, restarting" << std::endl;
        radioController.onMessage(message_level_t::Information,
                "End of file, restarting");
    }
    return n & ~01;
}

int32_t CRAWFile::convertSamples(RingBuffer<uint8_t>& Buffer, DSPCOMPLEX *V, int32_t size)
{
    int32_t amount, i;
    uint8_t* temp = (uint8_t*)alloca(IQByteSize * size * sizeof(uint8_t));

    amount = Buffer.getDataFromBuffer(temp, IQByteSize * size);

    // Unsigned 8-bit
    if(FileFormat == CRAWFileFormat::U8)
    {
        for (i = 0; i < amount / 2; i++)
        V[i] = DSPCOMPLEX(float(temp[2 * i] - 128) / 128.0, float(temp[2 * i + 1] - 128) / 128.0);
    }
    // Signed 8-bit
    else if(FileFormat == CRAWFileFormat::S8)
    {
        for (i = 0; i < amount / 2; i++)
        V[i] = DSPCOMPLEX(float((int8_t)temp[2 * i]) / 128.0, float((int8_t)temp[2 * i + 1]) / 128.0);
    }
    // Signed 16-bit little endian
    else if(FileFormat == CRAWFileFormat::S16LE)
    {
        int j=0;
        for (i = 0; i < amount / 4; i++)
        {
            int16_t IQ_I = (int16_t) (temp[j + 0] << 8) | temp[j + 1];
            int16_t IQ_Q = (int16_t) (temp[j + 2] << 8) | temp[j + 3];
            V[i] = DSPCOMPLEX((float) (IQ_I ), (float) (IQ_Q ));

            j +=IQByteSize;
        }
    }
    // Signed 16-bit big endian
    else if(FileFormat == CRAWFileFormat::S16BE)
    {
        int j=0;
        for (i = 0; i < amount / 4; i++)
        {
            int16_t IQ_I = (int16_t) (temp[j + 1] << 8) | temp[j + 0];
            int16_t IQ_Q = (int16_t) (temp[j + 3] << 8) | temp[j + 2];
            V[i] = DSPCOMPLEX((float) (IQ_I ), (float) (IQ_Q ));

            j +=IQByteSize;
        }
    }

    return amount / IQByteSize;
}
