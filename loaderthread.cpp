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
bool LoaderThread::selectPage(unsigned char page)
{
	m_port->write(QByteArray().append(0xA2).append((char)0x0).append(0x30).append(page));
	m_port->waitForBytesWritten(1);
	QByteArray newpacket;
	int ret = readBytes(&newpacket,3,1000);
	if (ret != 3)
	{
		qDebug() << "Bad return" << ret;
		return false;
	}
	else
	{
		if ((unsigned char)newpacket[0] ==  0xE0 && (unsigned char)newpacket[2] == 0x3E)
		{
			//qDebug() << "smVerify good";
			return true;
		}
		else
		{
			qDebug() << "Bad verify" << QString::number(newpacket[0],16).toUpper() << QString::number(newpacket[2],16).toUpper();
			qDebug() << "Verify len:" <<m_privBuffer.size();
			return false;
		}
	}
}
bool LoaderThread::verifyBlock(unsigned short address,QByteArray block)
{
	m_port->write(QByteArray().append(0xA7).append(address >> 8).append(address).append(block.size()));
	m_port->waitForBytesWritten(1);
	QByteArray newpacket;
	int size = readBytes(&newpacket,block.size()+4); //Read 16 bytes, + 4 bytes of reponse.
	if (size != block.size()+4)
	{
		qDebug() << "Bad size read from FreeEMS";
		//qDebug() << "Error on page:" << "0x" + QString::number(page,16).toUpper() << "address:" << "0x" + QString::number(address,16).toUpper();
		return false;
	}
	else
	{
		for (int i=0;i<block.size();i++)
		{
			if (((unsigned char)newpacket[i]) != ((unsigned char)block[i]))
			{
				return false;
			}
		}
		return true;
	}
}

bool LoaderThread::readBlockToS19(unsigned char page,unsigned short address,unsigned char reqsize,QString *returnval)
{
	m_port->write(QByteArray().append(0xA7).append(address >> 8).append(address).append(reqsize));
	m_port->waitForBytesWritten(1);
	QByteArray newpacket;
	int size = readBytes(&newpacket,reqsize+4); //Read 16 bytes, + 4 bytes of reponse.
	if (size != reqsize+4)
	{
		qDebug() << "Bad size read from FreeEMS";
		qDebug() << "Error on page:" << "0x" + QString::number(page,16).toUpper() << "address:" << "0x" + QString::number(address,16).toUpper();
		return false;
	}
	else
	{
		//qDebug() << "Read from page:" << "0x" + QString::number(page,16).toUpper() << "address:" << "0x" + QString::number(address,16).toUpper();
		QString record = "S214";
		record += ((page <= 0xF) ? "0" : "") + QString::number(page,16).toUpper();
		record += QString((address <= 0xFFF) ? "0" : "") + ((address <= 0xFF) ? "0" : "") + ((address <= 0xF) ? "0" : "") + QString::number(address,16).toUpper();
		unsigned char checksum = reqsize+4 + (page & 0xFF) + ((unsigned char)(address >> 8) & 0xFF) + ((unsigned char)address & 0xFF);
		bool blank = true;
		for (int i=0;i<reqsize;i++)
		{
			if ((unsigned char)newpacket[i] != 0xFF)
			{
				blank = false;
			}
			record += ((unsigned char)newpacket[i] <= 0xF ? "0" : "") + QString::number(((unsigned char)newpacket[i]),16).toUpper();
			checksum += (unsigned char)newpacket[i];
		}
		checksum = ~(checksum);
		record += (checksum <= 0xF ? "0" : "") + QString::number(checksum,16).toUpper();
		record += "\r\n";
		if (!blank) //Only write non blank records.
		{
			//output.write(record.toAscii());
			//output.flush();
			*returnval = record;
			return true;
		}
		*returnval = QString("");
		return true;
	}
}

bool LoaderThread::writeBlock(unsigned short address,QByteArray block)
{
	QByteArray packet;
	packet.append(0xA8);
	packet.append((address >> 8) & 0xFF);
	packet.append((address) & 0xFF);
	packet.append(block.size()-1);
	packet.append(block);
	//qDebug() << "Writing" << internal.size() << "bytes to" << QString::number(address & 0xFFFF,16).toUpper();
	m_port->write(packet);
	m_port->waitForBytesWritten(1);

	QByteArray newpacket;
	int ret = readBytes(&newpacket,3,3000);
	if (ret != 3)
	{
		qDebug() << "Bad return" << ret;
		return false;
	}
	else
	{
		if ((unsigned char)newpacket[0] ==  0xE0 && (unsigned char)newpacket[2] == 0x3E)
		{
			//qDebug() << "smVerify good";
			return true;
		}
		else
		{
			qDebug() << "Bad verify";
			qDebug() << QString::number((unsigned char)newpacket[0],16) << QString::number((unsigned char)newpacket[2],16);
			qDebug() << "Verify len:" <<m_privBuffer.size();
			return false;
		}
	}
}

bool LoaderThread::eraseBlock()
{
	m_port->write(QByteArray().append(0xB8));
	m_port->waitForBytesWritten(1);
	QByteArray newpacket;
	int size = readBytes(&newpacket,3);
	if (size == 3)
	{
		if ((unsigned char)newpacket[0] ==  0xE0 && (unsigned char)newpacket[2] == 0x3E)
		{
			//qDebug() << "Read:" << QString::number((unsigned char)newpacket[0],16) << QString::number((unsigned char)newpacket[2],16);
			return true;
		}
		else
		{
			qDebug() << "Bad verify";
			return false;
		}
	}
	else
	{
		qDebug() << "Bad read:" << size;
		return false;
	}
}
bool LoaderThread::verifySM()
{
	int retry = 0;
	while (retry++ <= 3)
	{
		m_port->write(QByteArray().append(0x0D));
		m_port->waitForBytesWritten(1);
		QByteArray verifybuf;
		int verifylen = readBytes(&verifybuf,3);
		qDebug() << "Verify len:" << verifylen;
		if ((unsigned char)verifybuf[0] == 0xE0)
		{
			if ((unsigned char)verifybuf[2] == 0x3E)
			{
				qDebug() << "In SM mode";
				return true;
			}
		}
		else if ((unsigned char)verifybuf[0] == 0xE1)
		{
			if ((unsigned char)verifybuf[2] == 0x3E)
			{
				qDebug() << "In SM mode two";
				return true;
			}
		}
		else
		{
			qDebug() << "Bad return:" << QString::number((unsigned char)verifybuf[0],16) << QString::number((unsigned char)verifybuf[2],16);
		}
	}
	//Timed out.
	return false;
}

void LoaderThread::run()
{
	qint64 currentmsec = QDateTime::currentMSecsSinceEpoch();
	if (operation == "rip")
	{
		openPort();
		if (!verifySM())
		{
			//Timed out
			m_port->close();
			delete m_port;
			return;
		}

		//We're in SM mode! Let's do stuff.


		QFile output(m_fwFileName);
		output.open(QIODevice::ReadWrite | QIODevice::Truncate);

		for (int i=0xE0;i<0xFF;i++)
		{

			//Select page
			selectPage(i);

			for (int j=0x8000;j<0xBFFF;j+=16)
			{
				QString record;
				if (readBlockToS19(i,j,16,&record))
				{
					if (record == "")
					{
						//Blank record
					}
					else
					{
						output.write(record.toAscii());
						output.flush();
					}
				}
				//msleep(1);
			}
		}
		m_port->close();
		delete m_port;
		m_port = 0;
		emit done();
		qDebug() << "Current operation completed in:" << (QDateTime::currentMSecsSinceEpoch() - currentmsec) / 1000.0 << "seconds";
		return;
	}
	else if (operation == "load")
	{
		QFile fwfile(m_fwFileName);
		fwfile.open(QIODevice::ReadOnly);
		while (!fwfile.atEnd())
		{
			QString line = fwfile.readLine().replace("\r","").replace("\n","");
			if (line.startsWith("S2"))
			{
				bool ok = false;
				int linesize = line.mid(2,2).toInt(&ok,16);
				if ((linesize * 2) != line.length()-4)
				{
					qDebug() << "Bad Line" << linesize << line.length();
				}
				else
				{
					QString address = line.mid(4,6);
					QString data = line.mid(10);
					m_addressList.append(QPair<QString,QString>(address,data));
					//qDebug() << "Address:" << address << "Line" << data;
				}
			}
		}
		fwfile.close();
		qDebug() << m_addressList.size() << "records loaded";

		openPort();


		if (!verifySM())
		{
			//Timed out
			m_port->close();
			delete m_port;
			return;
		}

		//We're in SM mode! Let's do stuff.

		unsigned char currpage = 0;
		for (int i=0;i<m_addressList.size();i++)
		{
			emit progress(i,m_addressList.size());
			bool ok = false;
			unsigned int address = m_addressList[i].first.toInt(&ok,16);
			QByteArray internal;
			for (int j=0;j<m_addressList[i].second.size()-3;j+=2)
			{
				internal.append(QString(m_addressList[i].second[j]).append(m_addressList[i].second[j+1]).toInt(&ok,16));
			}
			unsigned char newpage = (address >> 16) & 0xFF;
			QByteArray packet;
			QByteArray newpacket;
			if (newpage != currpage)
			{

				currpage = newpage;
				qDebug() << "Selecting page:" << currpage;
				selectPage(currpage);
				eraseBlock();
			}

			if (!writeBlock(address,internal))
			{
				//Bad block. Retry.
				i--;
				continue;
			}
			if (!verifyBlock(address,internal))
			{
				qDebug() << "Bad verification of written data. Go back one and try again";
				i--;
				continue;
			}
			//msleep(1000);
		}
		m_port->write(QByteArray().append(0xB4)); //reset
		m_port->waitForBytesWritten(1);
		m_port->close();
		delete m_port;
		m_port = 0;
		emit done();
		qDebug() << "Current operation completed in:" << (QDateTime::currentMSecsSinceEpoch() - currentmsec) / 1000.0 << "seconds";
		return;
	}
}
bool LoaderThread::openPort()
{
	m_port = new QSerialPort();
	m_port->setPortName(m_portName);
	if (!m_port->open(QIODevice::ReadWrite))
	{
		qDebug() << "Unable to open port";
		return false;
	}
	m_port->setBaudRate(115200);
	m_port->setParity(QSerialPort::NoParity);
}

void LoaderThread::startLoad(QString load,QString com)
{
	qDebug() << "Loading:" << load;
	qDebug() << "Com port:" << com;
	operation = "load";
	m_portName = com;
	m_fwFileName = load;
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

int LoaderThread::readBytes(QByteArray *buf,int len,int timeout)
{
	if (m_privBuffer.size() >= len)
	{
		*buf = m_privBuffer.mid(0,len);
		m_privBuffer.remove(0,len);
		return len;
	}
	while (m_port->waitForReadyRead(timeout))
	{
		m_privBuffer.append(m_port->readAll());
		//qDebug() << "Read:" << m_privBuffer.size();
		if (m_privBuffer.size() >= len)
		{
			*buf = m_privBuffer.mid(0,len);
			m_privBuffer.remove(0,len);
			return len;
		}
	}
	return 0;
}