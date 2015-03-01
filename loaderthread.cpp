/************************************************************************************
 * EMSLoader - Open Source S12X Serial Monitor S19 Loader                           *
 * Copyright (C) 2013  Michael Carpenter (malcom2073@gmail.com)                     *
 *                                                                                  *
 * This file is a part of EMSLoader                                                 *
 *                                                                                  *
 * EMSLoader is free software; you can redistribute it and/or modify                *
 * it under the terms of the GNU General Public License as published by             *
 * the Free Software Foundation, either version 3 of the License, or                *
 * (at your option) any later version.                                              *
 *                                                                                  *
 * EMSLoader is distributed in the hope that it will be useful,                     *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of                   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU                *
 * General Public License for more details.                                         *
 *                                                                                  *
 * You should have received a copy of the GNU General Public License                *
 * along with EMSLoader.  If not, see <http://www.gnu.org/licenses/>.               *
 ************************************************************************************/

#include "loaderthread.h"
#include <QFile>
#include <QDebug>
#include <QDateTime>

LoaderThread::LoaderThread(QObject *parent) : QThread(parent)
{
}

/*
 +-------------------//------------------//-----------------------------+
 | type | count | address  |            data                 | checksum |
 +-------------------//------------------//-----------------------------+
 S2     14      F880B0    B746B7C687B7467D3044FD30421AEEFD      60
 */
//^^ shamelessly stolen from Seank's loader source :)

void LoaderThread::run()
{
	qint64 currentmsec = QDateTime::currentMSecsSinceEpoch();
	if (operation == "rip")
	{
		serialMonitor = new SerialMonitor();
		serialMonitor->openPort(m_portName);
		if (!serialMonitor->verifySM())
		{
			//Timed out
			serialMonitor->closePort();
			emit error("Unable to verify SM mode");
			delete serialMonitor;
			return;
		}

		//We're in SM mode! Let's do stuff.


		QFile output(m_fwFileName);
		if (!output.open(QIODevice::ReadWrite | QIODevice::Truncate))
		{
			serialMonitor->closePort();
			emit error("Unable to open file for writing");
			delete serialMonitor;

		}

		for (int i=0xE0;i<0xFF;i++)
		{

			//Select page
			serialMonitor->selectPage(i);

			for (int j=0x8000;j<0xBFFF;j+=16)
			{
				QString record;

				if (serialMonitor->readBlockToS19(i,j,16,&record))
				{
					if (record == "")
					{
						//Blank record
					}
					else
					{
                        output.write(record.toLatin1());
						output.flush();
					}
				}
				//msleep(1);
			}
		}
		serialMonitor->closePort();
		emit done(QDateTime::currentMSecsSinceEpoch() - currentmsec);
		qDebug() << "Current operation completed in:" << (QDateTime::currentMSecsSinceEpoch() - currentmsec) / 1000.0 << "seconds";
		delete serialMonitor;
		return;
	}
	else if (operation == "load")
	{
		serialMonitor = new SerialMonitor();

		if (!serialMonitor->openPort(m_portName))
		{
			qDebug() << "Unable to open port";
			emit error("Unable to open port. Ensure the device is connected, and you have the proper permissions to open the device");
			delete serialMonitor;
			return;
		}

		if (!serialMonitor->verifySM())
		{
			//Timed out
			serialMonitor->closePort();
			qDebug() << "Unable to open port";
			emit error("Unable to verify SM mode. Ensure you have the correct device selected, and it is in SM mode.");
			delete serialMonitor;
			return;
		}

		//We're in SM mode! Let's do stuff.

		unsigned char currpage = 0;
		int totalerror = 0;
		for (int i=0;i<m_s19File->getCompactRecordCount();i++)
		{

			emit progress(i,m_s19File->getCompactRecordCount());
			unsigned int address = m_s19File->getCompactRecord(i).first;
			unsigned char newpage = (address >> 16) & 0xFF;

			if (newpage != currpage)
			{
				currpage = newpage;
				qDebug() << "Selecting page:" << currpage;
				serialMonitor->selectPage(currpage);
				serialMonitor->eraseBlock();
			}

			for (int j=0;j<m_s19File->getCompactRecord(i).second.size();j+=252)
			{
				if (totalerror >= 100)
				{
					serialMonitor->closePort();
					emit error("Too many errors while loading firmware. Make sure you have a good connection to the device and try again");
					delete serialMonitor;
					return;
				}
				int size = (j+252< m_s19File->getCompactRecord(i).second.size()) ? 252 : (j - m_s19File->getCompactRecord(i).second.size());
				if (!serialMonitor->writeBlock((address & 0xFFFF) + j,m_s19File->getCompactRecord(i).second.mid(j,size)))
				{
					//Bad block. Retry.
					i--;
					totalerror++;
					continue;

				}
				if (!serialMonitor->verifyBlock((address & 0xFFFF) + j,m_s19File->getCompactRecord(i).second.mid(j,size)))
				{
					qDebug() << "Bad verification of written data. Go back one and try again";
					i--;
					totalerror++;
					continue;

				}

			}
		}
		serialMonitor->sendReset();
		serialMonitor->closePort();
		emit done(QDateTime::currentMSecsSinceEpoch() - currentmsec);
		qDebug() << "Current operation completed in:" << (QDateTime::currentMSecsSinceEpoch() - currentmsec) / 1000.0 << "seconds";
		delete serialMonitor;
		return;
	}
}

void LoaderThread::startLoad(S19File *file,QString com)
{
	m_s19File = file;
	//qDebug() << "Loading:" << load;
	qDebug() << "Com port:" << com;
	operation = "load";
	m_portName = com;
	start();
}
void LoaderThread::startRip(QString filename,QString portname)
{
	qDebug() << "Ripping:" << filename;
	qDebug() << "Com port:" << portname;
	m_fwFileName = filename;
	m_portName = portname;
	operation = "rip";
	start();
}


