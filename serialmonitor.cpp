#include "serialmonitor.h"
#include <QDebug>
#include <QThread>
SerialMonitor::SerialMonitor(QObject *parent) : QObject(parent)
{
}
bool SerialMonitor::verifyBlock(unsigned short address,QByteArray block)
{
	QByteArray towrite = QByteArray().append(0xA7).append(address >> 8).append(address).append(block.size());
	m_port->write((const uint8_t*)(towrite.constData()),towrite.size());
	//m_port->write(QByteArray().append(0xA7).append(address >> 8).append(address).append(block.size()));
	//m_port->waitForBytesWritten(1);
	bool breaker = false;
	int timer = 0;
	while (!breaker && m_port->available()< block.size()+4)
	{
		timer++;
		if (timer > 100)
		{
			breaker = true;
		}
		QThread::currentThread()->msleep(10);
	}
	QByteArray newpacket;
	int size = readBytes(&newpacket,block.size()+4); //Read 16 bytes, + 4 bytes of reponse.
	if (size != block.size()+4)
	{
		qDebug() << "Bad size read from verifyBlock" << size << block.size()+4;
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

void SerialMonitor::closePort()
{
	m_port->close();
	delete m_port;
	m_port = 0;
}

bool SerialMonitor::openPort(QString portname)
{
	try
	{
		m_port = new serial::Serial();
		m_port->setPort(portname.toStdString());
		m_port->open();
		if (!m_port->isOpen())
		{
			qDebug() << "Error opening port:";
			return false;
		}
		m_port->setBaudrate(115200);
		m_port->setParity(serial::parity_none);
		m_port->setStopbits(serial::stopbits_one);
		m_port->setBytesize(serial::eightbits);
		m_port->setFlowcontrol(serial::flowcontrol_none);
		m_port->setTimeout(1,1,0,100,0); //1ms read timeout, 100ms write timeout.
		return true;
	}
	catch (serial::IOException ex)
	{
		return false;
	}
	catch (std::exception ex)
	{
		return false;
	}
}

bool SerialMonitor::verifySM()
{
	int retry = 0;
	while (retry++ <= 6)
	{
		if (retry == 1)
		{
			m_port->write((const uint8_t*)QByteArray().append(0x0D).data(),1);
		}
		else
		{
			m_port->write((const uint8_t*)QByteArray().append(0xB7).data(),1);
		}
		bool breaker = false;
		int timer = 0;
		while (!breaker && m_port->available() < 3)
		{
			timer++;
			if (timer > 10)
			{
				breaker = true;
			}
			QThread::currentThread()->msleep(10);
		}
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
			return true;
		}
		else
		{
			qDebug() << "Bad return:" << QString::number((unsigned char)verifybuf[0],16) << QString::number((unsigned char)verifybuf[2],16);
			readBytes(&verifybuf,10,50);
			//m_port->clear();
			m_port->flush();
			m_privBuffer.clear();
		}


	}
	//Timed out.
	qDebug() << "Timed out";
	return false;
}

bool SerialMonitor::selectPage(unsigned char page)
{
	QByteArray tosend = QByteArray().append(0xA2).append((char)0x0).append(0x30).append(page);
	m_port->write((const uint8_t*)tosend.data(),tosend.size());

	QByteArray newpacket;
	bool breaker = false;
	int timer = 0;
	while (!breaker && m_port->available() < 3)
	{
		timer++;
		if (timer > 10)
		{
			breaker = true;
		}
		QThread::currentThread()->msleep(10);
	}
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

bool SerialMonitor::readBlockToS19(unsigned char page,unsigned short address,unsigned char reqsize,QString *returnval)
{
	QByteArray tosend = QByteArray().append(0xA7).append(address >> 8).append(address).append(reqsize);
	m_port->write((const uint8_t*)tosend.data(),tosend.size());
	//m_port->waitForBytesWritten(1);
	QByteArray newpacket;
	int size = readBytes(&newpacket,reqsize+4); //Read 16 bytes, + 4 bytes of reponse.
	if (size != reqsize+4)
	{
		qDebug() << "Bad size read from S19";
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

bool SerialMonitor::writeBlock(unsigned short address,QByteArray block)
{
	QByteArray packet;
	packet.append(0xA8);
	packet.append((address >> 8) & 0xFF);
	packet.append((address) & 0xFF);
	packet.append(block.size()-1);
	packet.append(block);
	//qDebug() << "Writing" << internal.size() << "bytes to" << QString::number(address & 0xFFFF,16).toUpper();
	m_port->write((const uint8_t*)packet.data(),packet.size());
	bool breaker = false;
	int timer = 0;
	while (!breaker && m_port->available() < 3)
	{
		timer++;
		if (timer > 100)
		{
			breaker = true;
		}
		QThread::currentThread()->msleep(10);
	}

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
			qDebug() << "Bad verify on write block";
			qDebug() << QString::number((unsigned char)newpacket[0],16) << QString::number((unsigned char)newpacket[2],16);
			qDebug() << "Verify len:" <<m_privBuffer.size();
			return false;
		}
	}
}

bool SerialMonitor::eraseBlock()
{
	m_port->write((const uint8_t*)QByteArray().append(0xB8).data(),1);
	//m_port->waitForBytesWritten(1);
	//m_port->flush();
	bool breaker = false;
	int timer = 0;
	while (!breaker && m_port->available() < 3)
	{
		timer++;
		if (timer > 200)
		{
			breaker = true;
		}
		QThread::currentThread()->msleep(10);
	}
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
			qDebug() << "Bad verify on erase block";
			return false;
		}
	}
	else
	{
		qDebug() << "Bad read on erase block:" << size;
		return false;
	}
}

int SerialMonitor::readBytes(QByteArray *buf,int len,int timeout)
{
	if (m_privBuffer.size() >= len)
	{
		*buf = m_privBuffer.mid(0,len);
		m_privBuffer.remove(0,len);
		//qDebug() << "Read:" << len << "Left Over: " << m_privBuffer.size();
		return len;
	}
	uint8_t buff[1024];

	while (m_port->available() > 0)
	{
		//m_privBuffer.append(m_port->readAll());
		int newlen = m_port->read(buff,1);
		m_privBuffer.append((const char*)buff,newlen);
		//qDebug() << "Read:" << m_privBuffer.size();
		if (m_privBuffer.size() >= len)
		{
			*buf = m_privBuffer.mid(0,len);
			m_privBuffer.remove(0,len);
			//qDebug() << "Read:" << len << "Left Over: " << m_privBuffer.size();
			return len;
		}
	}
	return 0;
}

void SerialMonitor::sendReset()
{
	m_port->write((const uint8_t*)QByteArray().append(0xB4).data(),1); //reset

}

void SerialMonitor::jumpToSM()
{
	uint8_t buff[1024];
	const uint8_t jump_to_sm[] = "\xAA\x00\x03\x0A\x0D\xCC";
	qDebug() << "Attempting to jump to serial monitor";
	m_port->write(jump_to_sm,6); //jump_to_sm
	QThread::currentThread()->msleep(200);
	m_port->read(buff,1024);
}

bool SerialMonitor::isStreaming()
{
	uint8_t buff[1024];
	uint16_t res = 0;
	res = m_port->read(buff,1024);
	if (res > 0)
	{
		qDebug() << "Streaming detected...";
		return true;
	}
	else
		return false;
}
